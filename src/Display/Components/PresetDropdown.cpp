#include "PresetDropdown.h"
#include "SharedState.h"
#include "Commands/CommandExecutor.h"

#include <ftxui/component/component_options.hpp>

#include <algorithm>

namespace Test
{
    using namespace ftxui;

    PresetDropdown::PresetDropdown(SharedState &state, Commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec)
    {
        DropdownOption opt;
        opt.open = &m_open;
        opt.radiobox.entries = &m_entries; // pointer: tracks live mutations to m_entries
        opt.radiobox.selected = &m_selected;

        // Fire the apply hook only on a real user pick. Programmatic updates in
        // sync() write m_selected directly (not through the radiobox), so they
        // don't trip this.
        opt.radiobox.on_change = [this]
        {
            if (m_onApply)
                m_onApply(m_selected);
        };

        // Highlight the active preset (green ●); show focus with a subtle bg.
        opt.radiobox.transform = [](const EntryState &s)
        {
            auto e = text((s.state ? " ● " : "   ") + s.label);
            e = e | color(s.state ? Color::RGB(90, 200, 110) : Color::RGB(185, 185, 195));
            if (s.state)
                e = e | bold;
            if (s.active)
                e = e | bgcolor(Color::RGB(40, 40, 55));
            return e;
        };

        // Collapsed header: the current preset name with an open/close caret.
        opt.radiobox.focused_entry = &m_selected;
        opt.transform = [](bool open, Element checkbox, Element radiobox)
        {
            if (open)
                return vbox({checkbox, radiobox | border}) | size(WIDTH, GREATER_THAN, 24);
            return checkbox;
        };

        m_dropdown = Dropdown(opt);
    }

    void PresetDropdown::sync()
    {
        m_single = m_exec && m_exec->selection().size() == 1;
        m_active = false;
        m_numPresets = 0;
        if (!m_single)
        {
            m_open = false;
            return;
        }

        const auto &sel = m_exec->selection().front();
        const auto clients = m_state.snapshot();
        const Client *client = nullptr;
        for (const auto &[ip, c] : clients)
            if (ip == sel.ip)
            {
                client = &c;
                break;
            }

        if (!client || sel.fixtureId < 0 ||
            sel.fixtureId >= static_cast<int>(client->fixtures.size()))
        {
            m_open = false;
            return;
        }

        const auto &fx = client->fixtures[sel.fixtureId];
        m_numPresets = fx.numPresets;
        m_selected = fx.presetIndex; // raw index (used by the fallback line too)

        if (fx.presets.empty())
        {
            // Names not fetched yet → fall back to the numeric line.
            m_open = false;
            return;
        }

        m_entries = fx.presets;
        m_selected = std::clamp(fx.presetIndex, 0, static_cast<int>(m_entries.size()) - 1);
        m_active = true;
    }

    Element PresetDropdown::render() const
    {
        auto label = [](const std::string &s)
        { return text(s) | color(Color::RGB(80, 190, 190)); };

        if (m_active)
            return vbox({label("  preset"), hbox({text("  "), m_dropdown->Render()})});

        if (m_single && m_numPresets > 0)
            return hbox({label("  preset    "),
                         text(std::to_string(m_selected) + " / " + std::to_string(m_numPresets)) |
                             color(Color::RGB(220, 190, 100)),
                         text("   describe to list") | dim});

        return text("");
    }

}
