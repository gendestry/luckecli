#pragma once
#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "Commands/CommandExecutor.h"

namespace Test
{

    class SharedState;

    // Detail panel for the currently-selected fixture. With a single fixture
    // selected it renders editable fields (name / universe / address) plus an
    // editable engine section (serial & wireless report toggles); the remaining
    // read-only values (type, ip, footprint, engine version) are read live each
    // frame so they track cache updates. Name/universe/address commit on Enter;
    // the toggles commit on change — each fires the matching shell command
    // through onCommand.
    //
    // Multi-selection editing is not wired up yet: with zero or several fixtures
    // selected this falls back to the read-only listing (and isn't focusable).
    //
    // The ftxui inputs/checkboxes are built once over member-backed storage;
    // sync() refreshes that storage from the current selection (UI thread only),
    // taking care not to clobber a half-typed edit on the same fixture.
    class InfoPanel
    {
    public:
        InfoPanel(SharedState &state, Commands::CommandExecutor *&exec);

        // The component to mount in the layout tree (gate its focus with active()).
        ftxui::Component component() const { return m_container; }

        // Refresh the editable fields from the current single selection.
        void sync();

        // True when the editable form should be shown/focusable.
        bool active() const { return m_active; }

        // The info section element (editable form, read-only listing, or empty).
        ftxui::Element render() const;

        // Scroll the panel content. Positive scrolls down; clamped to [0,1].
        void scroll(int delta);

        // Runs a shell-style command string (same sink as the REPL).
        void setOnCommand(std::function<void(const std::string &)> fn) { m_onCommand = std::move(fn); }

    private:
        SharedState &m_state;
        Commands::CommandExecutor *&m_exec;

        ftxui::Component m_container;
        ftxui::Component m_nameInput, m_universeInput, m_addressInput;
        ftxui::Component m_serialBox, m_wirelessBox;
        ftxui::Component m_applyBtn, m_revertBtn;

        // Send any edited fields (name/universe/address) as commands, then adopt
        // the typed values as the new baseline. Reset the fields to the baseline.
        void applyEdits();
        void revertEdits();

        // Backing storage referenced by the components (UI thread only).
        std::string m_name, m_universe, m_address;
        // Last-known committed values, used to detect edits and to revert to.
        std::string m_baseName, m_baseUniverse, m_baseAddress;

        // Relative scroll position of the panel content, in [0,1].
        float m_scroll = 0.f;
        bool m_serial = false;
        bool m_wireless = false;

        // The fixture the editable fields currently mirror. Used so the 1s refresh
        // doesn't overwrite a half-typed edit on the same target.
        std::string m_ip;
        int m_fixtureId = -1;

        bool m_active = false; // single selection on a live fixture

        std::function<void(const std::string &)> m_onCommand;
    };

    // Read-only fixture listing for zero/multi selection (and the fallback inside
    // the editable panel). Read live from the cached client state.
    ftxui::Element infoListing(SharedState &state,
                               const std::vector<Commands::CommandExecutor::Selection> &selection);

}
