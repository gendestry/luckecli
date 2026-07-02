#pragma once
#include "ui/View.h"
#include "ui/components/homescreen/GridView.h"
#include "ui/components/universe/UniverseView.h"
#include "ui/components/terminal/TerminalView.h"
#include "ui/components/control/ControlView.h"

#include <functional>
#include <string>

namespace core { class SharedState; }
namespace core::commands { class CommandExecutor; }

namespace ui
{

    // Top-level FTXUI view: owns the single screen/event loop and the 1s ping
    // ticker, and switches between the Home / Universe / Terminal panels via a
    // Container::Tab and a right-edge view switcher. Each panel is its own
    // component and keeps its state across switches.
    class MainView : public View
    {
    public:
        MainView(core::SharedState &state, core::commands::CommandExecutor *&exec);

        void run(std::function<bool(const std::string &)> &onCommand) override;
        void onStateChanged() override; // re-sync the panels, then redraw
        void onLog(const Loggable &entry) override; // feed the terminal scrollback

    private:
        // True when the active panel is consuming text input, so the host should
        // not steal digit keys for view switching.
        bool typingText() const;

        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;

        GridView m_home;
        UniverseView m_universe;
        TerminalView m_terminal;
        ControlView m_control;

        int m_view = 0; // 0=Home, 1=Universe, 2=Terminal, 3=Control
    };

}
