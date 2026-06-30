#include "InfoPanel.h"
#include "SharedState.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

#include <string>

namespace Test
{
    using namespace ftxui;

    namespace
    {
        Element label(const std::string &s) { return text(s) | color(Color::RGB(80, 190, 190)); }
        Element value(const std::string &s) { return text(s) | color(Color::RGB(220, 190, 100)); }

        // The live client for `ip` in a fresh snapshot, or nullptr if it's offline.
        const Client *findClient(const std::vector<std::pair<std::string, Client>> &clients,
                                 const std::string &ip)
        {
            for (const auto &[cip, c] : clients)
                if (cip == ip)
                    return &c;
            return nullptr;
        }

        // One fixture's read-only detail block, or a "gone" note if it's no longer
        // cached.
        Element fixtureBlock(const Client &client, const std::string &ip, int fixtureId)
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

    InfoPanel::InfoPanel(SharedState &state, Commands::CommandExecutor *&exec)
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

        m_container = Container::Vertical({
            m_nameInput,
            m_universeInput,
            m_addressInput,
            Container::Horizontal({m_applyBtn, m_revertBtn}),
            m_serialBox,
            m_wirelessBox,
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
        const Client *client = findClient(clients, sel.ip);
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

        // Only (re)load the editable text when the target fixture changes, so the
        // 1s refresh doesn't overwrite an edit the user is mid-way through typing.
        if (sel.ip == m_ip && sel.fixtureId == m_fixtureId)
            return;

        const auto &fx = client->fixtures[sel.fixtureId];
        m_name = m_baseName = fx.name;
        m_universe = m_baseUniverse = std::to_string(fx.universe);
        m_address = m_baseAddress = std::to_string(fx.address);
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
                                                        : std::vector<Commands::CommandExecutor::Selection>{}));

        const auto &sel = m_exec->selection().front();
        const auto clients = m_state.snapshot();
        const Client *client = findClient(clients, sel.ip);
        if (!client || sel.fixtureId < 0 ||
            sel.fixtureId >= static_cast<int>(client->fixtures.size()))
            return scrolled(infoListing(m_state, m_exec->selection()));

        const auto &fx = client->fixtures[sel.fixtureId];

        auto editRow = [](const std::string &name, const Component &input)
        { return hbox({label(name), input->Render()}); };

        Elements rows;
        rows.push_back(hbox({
            text(fx.name) | bold | color(Color::RGB(190, 160, 230)),
            text("  "),
            text(fx.type) | color(Color::RGB(130, 130, 130)),
            filler(), // push the engine version to the far right
            text(client->engine.version.empty() ? "?" : client->engine.version) |
                color(Color::RGB(130, 130, 130)),
        }));
        rows.push_back(hbox({label("  ip        "), text(sel.ip) | color(Color::RGB(140, 170, 210))}));
        rows.push_back(editRow("  name      ", m_nameInput));
        rows.push_back(editRow("  universe  ", m_universeInput));
        rows.push_back(editRow("  address   ", m_addressInput));
        rows.push_back(hbox({label("  footprint "), value(std::to_string(fx.footprint))}));

        // Apply / revert. Highlighted while there are uncommitted edits.
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

        // Engine section.
        rows.push_back(text(""));
        rows.push_back(label("  engine"));
        rows.push_back(hbox({text("  "), m_serialBox->Render()}));
        rows.push_back(hbox({text("  "), m_wirelessBox->Render()}));

        return scrolled(vbox(std::move(rows)));
    }

    Element infoListing(SharedState &state,
                        const std::vector<Commands::CommandExecutor::Selection> &selection)
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

            const Client *client = findClient(clients, sel.ip);
            if (!client)
                blocks.push_back(text(" " + sel.ip + " offline") | color(Color::RGB(210, 100, 100)));
            else
                blocks.push_back(fixtureBlock(*client, sel.ip, sel.fixtureId));
        }

        return vbox(std::move(blocks));
    }

}
