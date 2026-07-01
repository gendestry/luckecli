#pragma once
#include "ui/Panel.h"
#include "ui/components/homescreen/fixturegrid/FixtureGrid.h"
#include "ui/components/homescreen/infopanel/InfoPanel.h"
#include "ui/components/homescreen/PresetDropdown.h"

#include <atomic>
#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

namespace ftxui { class App; }
namespace core { class SharedState; }
namespace core::commands { class CommandExecutor; }

namespace ui
{


    // The "Online Fixtures" home view, as a Panel: a Menubar (multi-select
    // actions) on top, the FixtureGrid + InfoPanel in the middle, and a StatusBar
    // at the bottom. It does not own a screen or loop — the host (MainView) hosts
    // it in a Container::Tab, runs the loop, and drives sync()/setScreen().
    class GridView : public Panel
    {
    public:
        GridView(core::SharedState &state, core::commands::CommandExecutor *&exec);

        // Panel interface.
        ftxui::Component component() override;
        void sync() override; // rebuild the cards + info/preset panels from state
        const char *title() const override { return "Home"; }

        // The command sink for UI-triggered actions; the host sets it before
        // calling component().
        void setCommandSink(std::function<bool(const std::string &)> sink) { m_command = std::move(sink); }

        // The host's screen, used to post refreshes back onto the UI thread after
        // an off-thread command completes. Set before component(), cleared on exit.
        void setScreen(ftxui::App *screen) { m_screen = screen; }

        // True while a text field in the info panel is focused (so the host knows
        // not to steal digit keystrokes for view switching).
        bool editing() const;

        // Number of fixtures seen online at the last rebuild (for the status bar).
        int onlineCount() const;

    private:
        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;
        FixtureGrid m_grid;
        InfoPanel m_info;
        PresetDropdown m_presets;

        ftxui::App *m_screen = nullptr;                        // host screen (for posting refreshes)
        std::function<void()> m_refresh;                       // posts a rebuild+repaint on the UI thread
        std::function<bool(const std::string &)> m_command;    // command sink, set by the host
        std::atomic<bool> m_busy{false};                       // a command is running off-thread
        bool m_modifier = false;                               // ctrl/shift held on the last mouse event
        bool m_multiMode = false;                              // sticky multi-select toggle (every click adds)
        bool m_showUniverse = false;                           // universe-viewer popup open?
        bool m_armReset = false;                               // factory-reset button armed (2-click confirm)
    };

}
