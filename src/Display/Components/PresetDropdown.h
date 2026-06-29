#pragma once
#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace Test
{

    class SharedState;
    namespace Commands
    {
        class CommandExecutor;
    }

    // The preset picker for the Info panel. When exactly one fixture is selected
    // and its preset names are cached (fetched by `describe`), this renders an
    // interactive collapsible whose header shows the active preset; expanding it
    // reveals the list and picking another one fires onApply(index). Without
    // cached names it falls back to a compact "preset i / n" line, and with
    // no/multi selection it renders nothing.
    //
    // The ftxui Collapsible (header + radiobox) is built once over member-backed
    // storage; sync() just mutates that storage from the current selection (UI
    // thread only).
    class PresetDropdown
    {
    public:
        PresetDropdown(SharedState &state, Commands::CommandExecutor *&exec);

        // The component to mount in the layout tree (gate its focus with active()).
        ftxui::Component component() const { return m_collapsible; }

        // Refresh entries/selection from the current single selection.
        void sync();

        // True when the interactive collapsible should be shown/focusable.
        bool active() const { return m_active; }

        // The preset section element for the Info panel (collapsible, fallback, or empty).
        ftxui::Element render() const;

        // Called with the chosen preset index when the user picks a new one.
        void setOnApply(std::function<void(int)> fn) { m_onApply = std::move(fn); }

    private:
        SharedState &m_state;
        Commands::CommandExecutor *&m_exec;

        ftxui::Component m_collapsible;
        std::vector<std::string> m_entries; // backing storage referenced by the radiobox
        std::string m_label;                // collapsible header (referenced live)
        int m_selected = 0;                 // committed preset (radiobox `selected`)
        int m_focused = 0;                  // navigation cursor (radiobox `focused_entry`)
        int m_applied = -1;                 // last index we actually sent, to skip no-ops
        bool m_open = false;

        bool m_single = false; // exactly one fixture selected
        bool m_active = false; // m_single && preset names cached
        int m_numPresets = 0;  // for the fallback line when names aren't cached

        std::function<void(int)> m_onApply;
    };

}
