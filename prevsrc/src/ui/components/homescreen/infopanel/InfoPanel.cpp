#include "ui/components/homescreen/infopanel/InfoPanel.h"
#include "ui/Theme.h"
#include "core/domain/SharedState.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

#include <string>

namespace ui
{
    using namespace ftxui;

    namespace
    {
        Element label(const std::string &s) { return text(s) | color(Theme::lblColor()); }
        Element value(const std::string &s) { return text(s) | color(Theme::valColor()); }

        // The live client for `ip` in a fresh snapshot, or nullptr if it's offline.
        const core::Client *findClient(const std::vector<std::pair<std::string, core::Client>> &clients,
                                 const std::string &ip)
        {
            for (const auto &[cip, c] : clients)
                if (cip == ip)
                    return &c;
            return nullptr;
        }

        // A compact signal-strength glyph + dBm reading from an RSSI value.
        // -55 and up is excellent; below -78 is poor.
        Element rssiBadge(int rssi, bool connected)
        {
            if (!connected)
                return text("offline") | color(Theme::errColor());
            const char *bars = rssi >= -55 ? "▁▃▅▇"
                               : rssi >= -67 ? "▁▃▅ "
                               : rssi >= -78 ? "▁▃  "
                                             : "▁   ";
            auto c = rssi >= -67 ? Theme::okColor()
                     : rssi >= -78 ? Theme::valColor()
                                   : Theme::errColor();
            return hbox({text(bars) | color(c), text(" "),
                         text(std::to_string(rssi) + "dB") | color(Theme::muted())});
        }

        // One fixture's full read-only block (name header + details). Used by the
        // zero-selection fallback listing.
        Element fixtureBlock(const core::Client &client, const std::string &ip, int fixtureId)
        {
            if (fixtureId < 0 || fixtureId >= static_cast<int>(client.fixtures.size()))
                return text(" fixture " + std::to_string(fixtureId) + " on " + ip + " is gone") |
                       color(Theme::errColor());

            const auto &f = client.fixtures[fixtureId];

            Elements rows;
            rows.push_back(hbox({
                text(f.name) | bold | color(Theme::accent()),
                text("  "),
                text(f.type) | color(Theme::muted()),
            }));
            rows.push_back(hbox({label("  ip        "), text(ip) | color(Theme::ipColor())}));
            rows.push_back(hbox({label("  universe  "), value(std::to_string(f.universe))}));
            rows.push_back(hbox({label("  address   "), value(std::to_string(f.address))}));
            rows.push_back(hbox({label("  footprint "), value(std::to_string(f.footprint))}));

            return vbox(std::move(rows));
        }

        // The detail rows for one fixture *without* the name header — the header is
        // drawn separately as the collapsible fold row in the multi listing.
        Element fixtureDetail(const core::Client &client, const std::string &ip, int fixtureId)
        {
            if (fixtureId < 0 || fixtureId >= static_cast<int>(client.fixtures.size()))
                return text("     fixture is gone") | color(Theme::errColor());

            const auto &f = client.fixtures[fixtureId];
            Elements rows;
            rows.push_back(hbox({label("     ip        "), text(ip) | color(Theme::ipColor())}));
            rows.push_back(hbox({label("     universe  "), value(std::to_string(f.universe))}));
            rows.push_back(hbox({label("     address   "), value(std::to_string(f.address))}));
            rows.push_back(hbox({label("     footprint "), value(std::to_string(f.footprint))}));
            return vbox(std::move(rows));
        }

        // Style an editable field: a subtle filled box that brightens on focus.
        Element editBox(InputState s)
        {
            auto e = s.element;
            if (s.is_placeholder)
                e = e | dim;
            e = e | color(Theme::valColor());
            e = e | bgcolor(s.focused ? Theme::bgSelected() : Theme::bgFocus());
            return e | size(WIDTH, GREATER_THAN, 10);
        }
    }

    std::string InfoPanel::keyOf(const std::string &ip, int id)
    {
        return ip + "#" + std::to_string(id);
    }

    bool InfoPanel::editing() const
    {
        return m_nameInput->Focused() || m_universeInput->Focused() ||
               m_addressInput->Focused() || m_ssidInput->Focused() ||
               m_passwordInput->Focused();
    }

    InfoPanel::InfoPanel(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec)
    {
        // Editable fields commit on Enter, firing the matching command. The cache
        // is patched by the command handler, so sync() will mirror it back.
        InputOption nameOpt;
        nameOpt.content = &m_name;
        nameOpt.multiline = false;
        nameOpt.transform = editBox;
        nameOpt.on_enter = [this]
        { applyEdits(); };
        m_nameInput = Input(nameOpt);

        // Universe / address are numeric — swallow non-digit characters so the
        // command always gets a parseable value.
        auto numericInput = [](StringRef content, std::function<void()> onEnter)
        {
            InputOption opt;
            opt.content = content;
            opt.multiline = false;
            opt.transform = editBox;
            opt.on_enter = std::move(onEnter);
            return CatchEvent(Input(opt), [](Event e)
                              {
                if (e.is_character())
                {
                    const std::string &s = e.character();
                    return s.size() == 1 && (s[0] < '0' || s[0] > '9');
                }
                return false; });
        };

        m_universeInput = numericInput(&m_universe, [this]
                                       { applyEdits(); });
        m_addressInput = numericInput(&m_address, [this]
                                      { applyEdits(); });

        // Apply / revert: glyph buttons (no text). Apply commits edited fields;
        // revert resets them to the last-known committed values.
        auto iconBtn = [](const std::string &glyph, Color c, std::function<void()> cb)
        {
            auto opt = ButtonOption::Simple();
            opt.on_click = std::move(cb);
            opt.transform = [glyph, c](const EntryState &s)
            {
                auto e = text(" " + glyph + " ") | color(c);
                if (s.focused)
                    e = e | bgcolor(Theme::bgFocus()) | bold;
                return e;
            };
            return Button(opt);
        };
        m_applyBtn = iconBtn("✓", Theme::okColor(), [this] { applyEdits(); });   // ✔ check
        m_revertBtn = iconBtn("↻", Theme::warnColor(), [this] { revertEdits(); }); // ↺ undo

        // Engine toggles commit on change. sync() only writes the bound bools
        // (which doesn't fire on_change), so toggling here always reflects a real
        // user action.
        CheckboxOption serialOpt;
        serialOpt.label = "serial report";
        serialOpt.checked = &m_serial;
        serialOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_serial ? "serialprint on" : "serialprint off"); };
        m_serialBox = Checkbox(serialOpt);

        CheckboxOption wirelessOpt;
        wirelessOpt.label = "wireless report";
        wirelessOpt.checked = &m_wireless;
        wirelessOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_wireless ? "wirelessprint on" : "wirelessprint off"); };
        m_wirelessBox = Checkbox(wirelessOpt);

        CheckboxOption animationOpt;
        animationOpt.label = "wifi animation";
        animationOpt.checked = &m_animation;
        animationOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_animation ? "wifianimation on" : "wifianimation off"); };
        m_animationBox = Checkbox(animationOpt);

        // WiFi credentials. ssid is a plain field; password renders masked. Both
        // commit together through `setwifi` (Enter in either, or the apply glyph).
        InputOption ssidOpt;
        ssidOpt.content = &m_ssid;
        ssidOpt.multiline = false;
        ssidOpt.transform = editBox;
        ssidOpt.on_enter = [this] { applyWifi(); };
        m_ssidInput = Input(ssidOpt);

        InputOption passOpt;
        passOpt.content = &m_password;
        passOpt.multiline = false;
        passOpt.password = true; // render as dots
        passOpt.transform = editBox;
        passOpt.on_enter = [this] { applyWifi(); };
        m_passwordInput = Input(passOpt);

        m_wifiApplyBtn = iconBtn("✓", Theme::okColor(), [this] { applyWifi(); });
        m_wifiRevertBtn = iconBtn("↻", Theme::warnColor(), [this] { revertWifi(); });

        // Section headers: clickable buttons that flip the matching fold flag. The
        // arrow glyph reflects the live state via the captured flag pointer.
        auto sectionHdr = [](const std::string &title, bool *open)
        {
            auto opt = ButtonOption::Simple();
            opt.on_click = [open] { *open = !*open; };
            opt.transform = [title, open](const EntryState &s)
            {
                auto e = text(std::string(*open ? "▾ " : "▸ ") + title) |
                         color(Theme::lblColor()) | bold;
                if (s.focused)
                    e = e | bgcolor(Theme::bgFocus());
                return e;
            };
            return Button(opt);
        };
        m_fixtureHdr = sectionHdr("Fixture", &m_openFixture);
        m_engineHdr = sectionHdr("Engine", &m_openEngine);
        m_wifiHdr = sectionHdr("WiFi", &m_openWifi);

        // Holds the per-fixture fold headers for the multi-selection listing;
        // refilled by rebuildMulti() whenever the selection identity changes.
        m_multiContainer = Container::Vertical({});

        buildContainer();
    }

    void InfoPanel::buildContainer()
    {
        // Fixture sub-tree: editable fields, then the (optional) preset picker,
        // then the apply/revert bar — all gated by the Fixture fold flag.
        Components fixtureBody{m_nameInput, m_universeInput, m_addressInput};
        if (m_presetComp)
            // Only focusable while the preset picker is actually shown.
            fixtureBody.push_back(m_presetActive ? Maybe(m_presetComp, m_presetActive)
                                                 : m_presetComp);
        fixtureBody.push_back(Container::Horizontal({m_applyBtn, m_revertBtn}));

        m_singleContainer = Container::Vertical({
            m_fixtureHdr,
            Maybe(Container::Vertical(std::move(fixtureBody)), &m_openFixture),
            m_engineHdr,
            Maybe(Container::Vertical({m_serialBox, m_wirelessBox, m_animationBox}), &m_openEngine),
            m_wifiHdr,
            Maybe(Container::Vertical({
                      m_ssidInput,
                      m_passwordInput,
                      Container::Horizontal({m_wifiApplyBtn, m_wifiRevertBtn}),
                  }),
                  &m_openWifi),
        });

        // Top level: the single form when one fixture is selected, the fold
        // headers when several are. Each side only joins the focus tree when live.
        m_container = Container::Vertical({
            Maybe(m_singleContainer, [this] { return m_active; }),
            Maybe(m_multiContainer, [this] { return m_multi; }),
        });
    }

    void InfoPanel::setPresetSection(Component comp,
                                     std::function<Element()> render,
                                     std::function<bool()> active)
    {
        m_presetComp = std::move(comp);
        m_presetRender = std::move(render);
        m_presetActive = std::move(active);
        buildContainer(); // splice the preset component into the fixture sub-tree
    }

    void InfoPanel::rebuildMulti(const std::vector<core::commands::CommandExecutor::Selection> &sel)
    {
        std::vector<std::string> keys;
        keys.reserve(sel.size());
        for (const auto &s : sel)
            keys.push_back(keyOf(s.ip, s.fixtureId));
        if (keys == m_multiKeys)
            return; // same fixtures in the same order — keep the existing buttons

        m_multiKeys = keys;
        m_multiHeaders.clear();
        m_multiContainer->DetachAllChildren();

        for (const auto &k : keys)
        {
            auto opt = ButtonOption::Simple();
            opt.on_click = [this, k] { m_collapsed[k] = !m_collapsed[k]; };
            opt.transform = [this, k](const EntryState &s)
            {
                auto it = m_collapsed.find(k);
                const bool collapsed = it != m_collapsed.end() && it->second;
                auto e = text(collapsed ? " ▸ " : " ▾ ") | color(Theme::lblColor()) | bold;
                if (s.focused)
                    e = e | bgcolor(Theme::bgFocus());
                return e;
            };
            auto btn = Button(opt);
            m_multiHeaders.push_back(btn);
            m_multiContainer->Add(btn);
        }
    }

    void InfoPanel::applyEdits()
    {
        if (!m_onCommand)
            return;
        if (!m_name.empty() && m_name != m_baseName)
            m_onCommand("setname " + m_name);
        if (!m_universe.empty() && m_universe != m_baseUniverse)
            m_onCommand("setuniverse " + m_universe);
        if (!m_address.empty() && m_address != m_baseAddress)
            m_onCommand("setaddress " + m_address);

        // Adopt the typed values; the command handler patches the cache and sync()
        // will keep mirroring it from here.
        m_baseName = m_name;
        m_baseUniverse = m_universe;
        m_baseAddress = m_address;
    }

    void InfoPanel::revertEdits()
    {
        m_name = m_baseName;
        m_universe = m_baseUniverse;
        m_address = m_baseAddress;
    }

    void InfoPanel::applyWifi()
    {
        if (!m_onCommand)
            return;
        if (m_ssid == m_baseSsid && m_password == m_basePassword)
            return;
        // setwifi sets both credentials in one shot.
        m_onCommand("setwifi ssid " + m_ssid + " pass " + m_password);
        m_baseSsid = m_ssid;
        m_basePassword = m_password;
    }

    void InfoPanel::revertWifi()
    {
        m_ssid = m_baseSsid;
        m_password = m_basePassword;
    }

    void InfoPanel::scroll(int delta)
    {
        m_scroll += static_cast<float>(delta) * 0.1f;
        if (m_scroll < 0.f)
            m_scroll = 0.f;
        if (m_scroll > 1.f)
            m_scroll = 1.f;
    }

    void InfoPanel::sync()
    {
        m_active = false;
        m_multi = false;

        if (!m_exec)
        {
            m_ip.clear();
            m_fixtureId = -1;
            return;
        }

        const auto sel = m_exec->selection();
        if (sel.size() <= 1)
        {
            // Leaving multi-selection: drop the fold-header buttons so they don't
            // linger in the component tree (and hold focus) once the multi listing
            // is hidden. rebuildMulti() only ever adds; nothing else clears them.
            if (!m_multiKeys.empty())
            {
                m_multiKeys.clear();
                m_multiHeaders.clear();
                m_multiContainer->DetachAllChildren();
            }
        }

        if (sel.empty())
        {
            m_ip.clear();
            m_fixtureId = -1;
            return;
        }

        // Several fixtures: collapsible read-only listing. Rebuild the fold
        // headers only when the selection identity changes.
        if (sel.size() > 1)
        {
            m_multi = true;
            rebuildMulti(sel);
            m_ip.clear();
            m_fixtureId = -1;
            return;
        }

        // Single fixture from here on.
        const auto &s = sel.front();
        const auto clients = m_state.snapshot();
        const core::Client *client = findClient(clients, s.ip);
        if (!client || s.fixtureId < 0 ||
            s.fixtureId >= static_cast<int>(client->fixtures.size()))
        {
            m_ip.clear();
            m_fixtureId = -1;
            return;
        }

        m_active = true;

        // Engine toggles always mirror the live device state.
        m_serial = client->engine.serial_report;
        m_wireless = client->engine.wireless_report;
        m_animation = client->engine.wifi_animation;

        // Only (re)load the editable text when the target fixture changes, so the
        // 1s refresh doesn't overwrite an edit the user is mid-way through typing.
        if (s.ip == m_ip && s.fixtureId == m_fixtureId)
            return;

        const auto &fx = client->fixtures[s.fixtureId];
        m_name = m_baseName = fx.name;
        m_universe = m_baseUniverse = std::to_string(fx.universe);
        m_address = m_baseAddress = std::to_string(fx.address);
        // WiFi credentials are device-level; reload them when the target moves to
        // a different device (or fixture, harmlessly).
        m_ssid = m_baseSsid = client->wifi.ssid;
        m_password = m_basePassword = client->wifi.password;
        m_ip = s.ip;
        m_fixtureId = s.fixtureId;
    }

    Element InfoPanel::render() const
    {
        // Clip to the available height and scroll with m_scroll / focus, so long
        // listings (many selected fixtures) don't overflow the panel.
        auto scrolled = [this](Element e)
        { return std::move(e) | focusPositionRelative(0.f, m_scroll) | yframe | vscroll_indicator; };

        // ── Multi-selection: collapsible per-fixture blocks ──────────────────
        if (m_multi && m_exec)
        {
            const auto sel = m_exec->selection();
            // If the selection drifted since the last sync(), the fold buttons may
            // not line up — fall back to the plain listing until the next sync().
            if (m_multiHeaders.size() != sel.size())
                return scrolled(infoListing(m_state, sel));

            const auto clients = m_state.snapshot();
            Elements blocks;
            for (std::size_t i = 0; i < sel.size(); ++i)
            {
                const auto &s = sel.items()[i];
                const std::string key = keyOf(s.ip, s.fixtureId);
                auto it = m_collapsed.find(key);
                const bool collapsed = it != m_collapsed.end() && it->second;

                const core::Client *client = findClient(clients, s.ip);
                const bool valid = client && s.fixtureId >= 0 &&
                                   s.fixtureId < static_cast<int>(client->fixtures.size());

                if (i)
                    blocks.push_back(separator());

                // Fold-header row: arrow button + name/type, ip pinned right.
                Elements hdr{m_multiHeaders[i]->Render()};
                if (valid)
                {
                    const auto &f = client->fixtures[s.fixtureId];
                    hdr.push_back(text(f.name) | bold | color(Theme::accent()));
                    hdr.push_back(text("  "));
                    hdr.push_back(text(f.type) | color(Theme::muted()));
                }
                else
                {
                    hdr.push_back(text("fixture " + std::to_string(s.fixtureId)) |
                                  color(Theme::errColor()));
                }
                hdr.push_back(filler());
                hdr.push_back(text(s.ip) | color(Theme::ipColor()));
                blocks.push_back(hbox(std::move(hdr)));

                if (!collapsed)
                {
                    if (!client)
                        blocks.push_back(text("     offline") | color(Theme::errColor()));
                    else
                        blocks.push_back(fixtureDetail(*client, s.ip, s.fixtureId));
                }
            }
            return scrolled(vbox(std::move(blocks)));
        }

        // ── Zero selection / offline single fixture: read-only fallback ──────
        if (!m_active || !m_exec)
            return scrolled(infoListing(m_state, m_exec ? m_exec->selection().items()
                                                        : std::vector<core::commands::CommandExecutor::Selection>{}));

        // ── Single fixture: editable, sectioned form ─────────────────────────
        const auto &sel = m_exec->selection().front();
        const auto clients = m_state.snapshot();
        const core::Client *client = findClient(clients, sel.ip);
        if (!client || sel.fixtureId < 0 ||
            sel.fixtureId >= static_cast<int>(client->fixtures.size()))
            return scrolled(infoListing(m_state, m_exec->selection()));

        const auto &fx = client->fixtures[sel.fixtureId];

        auto editRow = [](const std::string &name, const Component &input)
        { return hbox({label(name), input->Render()}); };

        auto headerRow = [](const Component &hdr, Element summary = nullptr)
        {
            Elements row{hdr->Render(), text("  ")};
            if (summary)
            {
                row.push_back(filler());
                row.push_back(std::move(summary));
                row.push_back(text(" "));
            }
            return hbox(std::move(row));
        };

        Elements rows;

        // Title line: fixture name + type, with the engine version pinned right.
        rows.push_back(hbox({
            text(fx.name) | bold | color(Theme::accent()),
            text("  "),
            text(fx.type) | color(Theme::muted()),
            filler(),
            text(client->engine.version.empty() ? "?" : client->engine.version) |
                color(Theme::muted()),
        }));

        // ── Fixture ──────────────────────────────────────────────
        rows.push_back(headerRow(m_fixtureHdr));
        if (m_openFixture)
        {
            rows.push_back(hbox({label("  ip        "), text(sel.ip) | color(Theme::ipColor())}));
            rows.push_back(editRow("  name      ", m_nameInput));
            rows.push_back(editRow("  universe  ", m_universeInput));
            rows.push_back(editRow("  address   ", m_addressInput));

            // Preset picker, embedded among the fixture fields (above footprint).
            // render() already carries its own 2-space offset, so no extra indent.
            if (m_presetRender && (!m_presetActive || m_presetActive()))
                rows.push_back(m_presetRender());

            rows.push_back(hbox({label("  footprint "), value(std::to_string(fx.footprint))}));

            const bool dirty = m_name != m_baseName || m_universe != m_baseUniverse ||
                               m_address != m_baseAddress;
            rows.push_back(hbox({
                text("  "),
                m_applyBtn->Render(),
                text("  "),
                m_revertBtn->Render(),
                filler(),
                text(dirty ? "edited " : "") | color(Theme::warnColor()),
            }));
        }

        // ── Engine ───────────────────────────────────────────────
        rows.push_back(headerRow(m_engineHdr));
        if (m_openEngine)
        {
            rows.push_back(hbox({text("  "), m_serialBox->Render()}));
            rows.push_back(hbox({text("  "), m_wirelessBox->Render()}));
            rows.push_back(hbox({text("  "), m_animationBox->Render()}));
        }

        // ── WiFi ─────────────────────────────────────────────────
        Element wifiSummary = hbox({
            text(client->wifi.ssid.empty() ? "—" : client->wifi.ssid) | color(Theme::ipColor()),
            text("  "),
            rssiBadge(client->wifi.rssi, client->wifi.connected),
        });
        rows.push_back(headerRow(m_wifiHdr, std::move(wifiSummary)));
        if (m_openWifi)
        {
            rows.push_back(hbox({label("  status    "),
                                 client->wifi.connected
                                     ? text("● connected") | color(Theme::okColor())
                                     : text("○ offline") | color(Theme::errColor())}));
            rows.push_back(hbox({label("  signal    "), rssiBadge(client->wifi.rssi, client->wifi.connected)}));
            rows.push_back(editRow("  ssid      ", m_ssidInput));
            rows.push_back(editRow("  password  ", m_passwordInput));

            const bool dirty = m_ssid != m_baseSsid || m_password != m_basePassword;
            rows.push_back(hbox({
                text("  "),
                m_wifiApplyBtn->Render(),
                text("  "),
                m_wifiRevertBtn->Render(),
                filler(),
                text(dirty ? "edited " : "") | color(Theme::warnColor()),
            }));
        }

        return scrolled(vbox(std::move(rows)));
    }

    Element infoListing(core::SharedState &state,
                        const std::vector<core::commands::CommandExecutor::Selection> &selection)
    {
        if (selection.empty())
            return text(" nothing selected") | dim | center;

        // Index the snapshot by IP so several selected fixtures on one device
        // each get a fresh lookup.
        const auto clients = state.snapshot();

        Elements blocks;
        for (std::size_t i = 0; i < selection.size(); ++i)
        {
            const auto &sel = selection[i];
            if (i)
                blocks.push_back(separator());

            const core::Client *client = findClient(clients, sel.ip);
            if (!client)
                blocks.push_back(text(" " + sel.ip + " offline") | color(Theme::errColor()));
            else
                blocks.push_back(fixtureBlock(*client, sel.ip, sel.fixtureId));
        }

        return vbox(std::move(blocks));
    }

}
