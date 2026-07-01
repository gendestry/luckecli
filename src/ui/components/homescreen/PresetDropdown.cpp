#include "ui/components/homescreen/PresetDropdown.h"
#include "core/domain/SharedState.h"
#include "core/commands/CommandExecutor.h"

#include <ftxui/component/component_options.hpp>

#include <algorithm>

namespace ui
{
    using namespace ftxui;

    PresetDropdown::PresetDropdown(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec)
    {
        RadioboxOption opt;
        opt.entries = &m_entries; // pointer: tracks live mutations to m_entries
        opt.selected = &m_selected;

        // Apply only when the committed selection actually changes. on_change also
        // fires on mere navigation (arrow keys move focused_entry), and sync()
        // updates m_selected programmatically — m_applied filters both so we don't
        // re-send the preset the fixture is already on.
        opt.on_change = [this]
        {
            if (m_selected == m_applied)
                return;
            m_applied = m_selected;
            if (m_onApply)
                m_onApply(m_selected);
        };

        // Highlight the active preset (green ●); show focus with a subtle bg.
        opt.transform = [](const EntryState &s)
        {
            auto e = text((s.state ? " ● " : "   ") + s.label);
            e = e | color(s.state ? Color::RGB(90, 200, 110) : Color::RGB(185, 185, 195));
            if (s.state)
                e = e | bold;
            if (s.active)
                e = e | bgcolor(Color::RGB(40, 40, 55));
            return e;
        };

        // focused_entry is the cursor — kept separate from `selected` so browsing
        // the list doesn't commit (and re-apply) a preset.
        opt.focused_entry = &m_focused;

        // The collapsible's header label (m_label) tracks the active preset name;
        // expanding it reveals the radiobox list. m_open holds the toggle state.
        m_collapsible = Collapsible(&m_label, Radiobox(opt), &m_open);
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
        const core::Client *client = nullptr;
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
        // Keep cursor and the "already applied" marker in step with the cache so
        // this programmatic update can't trigger on_change → a redundant setpreset.
        m_focused = m_selected;
        m_applied = m_selected;
        // Header shows the active preset name (referenced live by the collapsible).
        m_label = m_entries[m_selected];
        m_active = true;
    }

    Element PresetDropdown::render() const
    {
        auto label = [](const std::string &s)
        { return text(s) | color(Color::RGB(80, 190, 190)); };

        if (m_active)
            return vbox({label("  preset"), hbox({text("  "), m_collapsible->Render()})});

        if (m_single && m_numPresets > 0)
            return hbox({label("  preset    "),
                         text(std::to_string(m_selected) + " / " + std::to_string(m_numPresets)) |
                             color(Color::RGB(220, 190, 100)),
                         text("   describe to list") | dim});

        return text("");
    }

}
