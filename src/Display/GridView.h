#pragma once
#include "View.h"
#include "FixtureGrid.h"
#include "Components/PresetDropdown.h"

#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

namespace Test
{

    class SharedState;
    namespace Commands
    {
        class CommandExecutor;
    }

    // The "Online Fixtures" screen. Pure composition: a Toolbar (multi-select
    // actions) on top, the FixtureGrid + InfoPanel in the middle, and a StatusBar
    // at the bottom. GridView owns the screen/event loop and the 1s ping ticker;
    // the pieces it lays out each manage their own slice of the UI.
    class GridView : public View
    {
    public:
        GridView(SharedState &state, Commands::CommandExecutor *&exec);

        void run(std::function<bool(const std::string &)> &onCommand) override;
        void onStateChanged() override; // rebuild the cards in place, then redraw

    private:
        SharedState &m_state;
        Commands::CommandExecutor *&m_exec;
        FixtureGrid m_grid;
        PresetDropdown m_presets;

        std::function<void()> m_refresh; // valid only while run() is looping
        bool m_modifier = false;         // ctrl/shift held on the last mouse event
        bool m_multiMode = false;        // sticky multi-select toggle (every click adds)
    };

}
