#pragma once
#include "ui/View.h"
#include "ui/FixtureGrid.h"
#include "ui/components/infopanel/InfoPanel.h"
#include "ui/components/PresetDropdown.h"

#include <atomic>
#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

namespace core { class SharedState; }
namespace core::commands { class CommandExecutor; }

namespace ui
{


    // The "Online Fixtures" screen. Pure composition: a Menubar (multi-select
    // actions) on top, the FixtureGrid + InfoPanel in the middle, and a StatusBar
    // at the bottom. GridView owns the screen/event loop and the 1s ping ticker;
    // the pieces it lays out each manage their own slice of the UI.
    class GridView : public View
    {
    public:
        GridView(core::SharedState &state, core::commands::CommandExecutor *&exec);

        void run(std::function<bool(const std::string &)> &onCommand) override;
        void onStateChanged() override; // rebuild the cards in place, then redraw

    private:
        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;
        FixtureGrid m_grid;
        InfoPanel m_info;
        PresetDropdown m_presets;

        std::function<void()> m_refresh;                       // valid only while run() is looping
        std::function<bool(const std::string &)> m_command;    // command sink, copied in run()
        std::atomic<bool> m_busy{false};                       // a command is running off-thread
        bool m_modifier = false;                               // ctrl/shift held on the last mouse event
        bool m_multiMode = false;                              // sticky multi-select toggle (every click adds)
        bool m_showUniverse = false;                           // universe-viewer popup open?
        bool m_armReset = false;                               // factory-reset button armed (2-click confirm)
    };

}
