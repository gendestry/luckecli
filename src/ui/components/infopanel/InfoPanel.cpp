#include "ui/components/infopanel/InfoPanel.h"
#include "core/domain/SharedState.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

#include <string>

namespace ui
{
    using namespace ftxui;

    namespace
    {
        Element label(const std::string &s) { return text(s) | color(Color::RGB(80, 190, 190)); }
        Element value(const std::string &s) { return text(s) | color(Color::RGB(220, 190, 100)); }

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
                return text("offline") | color(Color::RGB(210, 100, 100));
            const char *bars = rssi >= -55 ? "▁▃▅▇"
                               : rssi >= -67 ? "▁▃▅ "
                               : rssi >= -78 ? "▁▃  "
                                             : "▁   ";
            auto c = rssi >= -67 ? Color::RGB(90, 200, 110)
                     : rssi >= -78 ? Color::RGB(220, 190, 100)
                                   : Color::RGB(210, 100, 100);
            return hbox({text(bars) | color(c), text(" "),
                         text(std::to_string(rssi) + "dB") | color(Color::RGB(130, 130, 130))});
        }

        // One fixture's read-only detail block, or a "gone" note if it's no longer
        // cached.
        Element fixtureBlock(const core::Client &client, const std::string &ip, int fixtureId)
        {
            if (fixtureId < 0 || fixtureId >= static_cast<int>(client.fixtures.size()))
                return text(" fixture " + std::to_string(fixtureId) + " on " + ip + " is gone") |
                       color(Color::RGB(210, 100, 100));

            const auto &f = client.fixtures[fixtureId];

            Elements rows;
            rows.push_back(hbox({
                text(f.name) | bold | color(Color::RGB(190, 160, 230)),
                text("  "),
                text(f.type) | color(Color::RGB(130, 130, 130)),
            }));
            rows.push_back(hbox({label("  ip        "), text(ip) | color(Color::RGB(140, 170, 210))}));
            rows.push_back(hbox({label("  universe  "), value(std::to_string(f.universe))}));
            rows.push_back(hbox({label("  address   "), value(std::to_string(f.address))}));
            rows.push_back(hbox({label("  footprint "), value(std::to_string(f.footprint))}));
            // Presets are rendered separately by PresetDropdown (interactive when
            // a single fixture is selected), so they're intentionally omitted here.

            return vbox(std::move(rows));
        }

        // Style an editable field: a subtle filled box that brightens on focus.
        Element editBox(InputState s)
        {
            auto e = s.element;
            if (s.is_placeholder)
                e = e | dim;
            e = e | color(Color::RGB(220, 190, 100));
            e = e | bgcolor(s.focused ? Color::RGB(60, 60, 85) : Color::RGB(40, 40, 55));
            return e | size(WIDTH, GREATER_THAN, 10);
        }
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
                    e = e | bgcolor(Color::RGB(40, 40, 55)) | bold;
                return e;
            };
            return Button(opt);
        };
        m_applyBtn = iconBtn("✓", Color::RGB(90, 200, 110), [this] { applyEdits(); });   // ✔ check
        m_revertBtn = iconBtn("↻", Color::RGB(220, 160, 90), [this] { revertEdits(); }); // ↺ undo

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

        m_wifiApplyBtn = iconBtn("✓", Color::RGB(90, 200, 110), [this] { applyWifi(); });
        m_wifiRevertBtn = iconBtn("↻", Color::RGB(220, 160, 90), [this] { revertWifi(); });

        // Section headers: clickable buttons that flip the matching fold flag. The
        // arrow glyph reflects the live state via the captured flag pointer.
        auto sectionHdr = [](const std::string &title, bool *open)
        {
            auto opt = ButtonOption::Simple();
            opt.on_click = [open] { *open = !*open; };
            opt.transform = [title, open](const EntryState &s)
            {
                auto e = text(std::string(*open ? "▾ " : "▸ ") + title) |
                         color(Color::RGB(80, 190, 190)) | bold;
                if (s.focused)
                    e = e | bgcolor(Color::RGB(40, 40, 55));
                return e;
            };
            return Button(opt);
        };
        m_fixtureHdr = sectionHdr("Fixture", &m_openFixture);
        m_engineHdr = sectionHdr("Engine", &m_openEngine);
        m_wifiHdr = sectionHdr("WiFi", &m_openWifi);

        // Focus tree. Each section's interactive body is gated by Maybe(...) on its
        // fold flag, so collapsed sections are skipped when tabbing through.
        m_container = Container::Vertical({
            m_fixtureHdr,
            Maybe(Container::Vertical({
                      m_nameInput,
                      m_universeInput,
                      m_addressInput,
                      Container::Horizontal({m_applyBtn, m_revertBtn}),
                  }),
                  &m_openFixture),
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

        const bool single = m_exec && m_exec->selection().size() == 1;
        if (!single)
        {
            m_ip.clear();
            m_fixtureId = -1;
            return;
        }

        const auto &sel = m_exec->selection().front();
        const auto clients = m_state.snapshot();
        const core::Client *client = findClient(clients, sel.ip);
        if (!client || sel.fixtureId < 0 ||
            sel.fixtureId >= static_cast<int>(client->fixtures.size()))
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
        if (sel.ip == m_ip && sel.fixtureId == m_fixtureId)
            return;

        const auto &fx = client->fixtures[sel.fixtureId];
        m_name = m_baseName = fx.name;
        m_universe = m_baseUniverse = std::to_string(fx.universe);
        m_address = m_baseAddress = std::to_string(fx.address);
        // WiFi credentials are device-level; reload them when the target moves to
        // a different device (or fixture, harmlessly).
        m_ssid = m_baseSsid = client->wifi.ssid;
        m_password = m_basePassword = client->wifi.password;
        m_ip = sel.ip;
        m_fixtureId = sel.fixtureId;
    }

    Element InfoPanel::render() const
    {
        // Clip to the available height and scroll with m_scroll / focus, so long
        // listings (many selected fixtures) don't overflow the panel.
        auto scrolled = [this](Element e)
        { return std::move(e) | focusPositionRelative(0.f, m_scroll) | yframe | vscroll_indicator; };

        if (!m_active || !m_exec)
            return scrolled(infoListing(m_state, m_exec ? m_exec->selection()
                                                        : std::vector<core::commands::CommandExecutor::Selection>{}));

        const auto &sel = m_exec->selection().front();
        const auto clients = m_state.snapshot();
        const core::Client *client = findClient(clients, sel.ip);
        if (!client || sel.fixtureId < 0 ||
            sel.fixtureId >= static_cast<int>(client->fixtures.size()))
            return scrolled(infoListing(m_state, m_exec->selection()));

        const auto &fx = client->fixtures[sel.fixtureId];

        auto editRow = [](const std::string &name, const Component &input)
        { return hbox({label(name), input->Render()}); };

        // A section header row: the (interactive) header button plus an optional
        // live summary shown to its right (handy when the section is collapsed).
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
            text(fx.name) | bold | color(Color::RGB(190, 160, 230)),
            text("  "),
            text(fx.type) | color(Color::RGB(130, 130, 130)),
            filler(),
            text(client->engine.version.empty() ? "?" : client->engine.version) |
                color(Color::RGB(130, 130, 130)),
        }));

        // ── Fixture ──────────────────────────────────────────────
        rows.push_back(headerRow(m_fixtureHdr));
        if (m_openFixture)
        {
            rows.push_back(hbox({label("  ip        "), text(sel.ip) | color(Color::RGB(140, 170, 210))}));
            rows.push_back(editRow("  name      ", m_nameInput));
            rows.push_back(editRow("  universe  ", m_universeInput));
            rows.push_back(editRow("  address   ", m_addressInput));
            rows.push_back(hbox({label("  footprint "), value(std::to_string(fx.footprint))}));

            const bool dirty = m_name != m_baseName || m_universe != m_baseUniverse ||
                               m_address != m_baseAddress;
            rows.push_back(hbox({
                text("  "),
                m_applyBtn->Render(),
                text("  "),
                m_revertBtn->Render(),
                filler(),
                text(dirty ? "edited " : "") | color(Color::RGB(220, 160, 90)),
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
        // Collapsed summary: ssid + signal badge, so health is visible at a glance.
        Element wifiSummary = hbox({
            text(client->wifi.ssid.empty() ? "—" : client->wifi.ssid) | color(Color::RGB(140, 170, 210)),
            text("  "),
            rssiBadge(client->wifi.rssi, client->wifi.connected),
        });
        rows.push_back(headerRow(m_wifiHdr, std::move(wifiSummary)));
        if (m_openWifi)
        {
            rows.push_back(hbox({label("  status    "),
                                 client->wifi.connected
                                     ? text("● connected") | color(Color::RGB(90, 200, 110))
                                     : text("○ offline") | color(Color::RGB(210, 100, 100))}));
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
                text(dirty ? "edited " : "") | color(Color::RGB(220, 160, 90)),
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
                blocks.push_back(text(" " + sel.ip + " offline") | color(Color::RGB(210, 100, 100)));
            else
                blocks.push_back(fixtureBlock(*client, sel.ip, sel.fixtureId));
        }

        return vbox(std::move(blocks));
    }

}
