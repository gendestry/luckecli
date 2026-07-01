#include "ui/MainView.h"
#include "ui/components/homescreen/Toolbar.h"
#include "ui/components/homescreen/StatusBar.h"
#include "core/commands/CommandExecutor.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

namespace ui
{
    using namespace ftxui;

    MainView::MainView(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec), m_home(state, exec), m_universe(state), m_terminal(exec) {}

    bool MainView::typingText() const
    {
        // The terminal is all input; the home view is typing only while an info
        // field is focused. The universe view has no inputs.
        if (m_view == 2)
            return true;
        if (m_view == 0)
            return m_home.editing();
        return false;
    }

    void MainView::run(std::function<bool(const std::string &)> &onCommand)
    {
        auto screen = App::Fullscreen();
        m_app = &screen;

        // Hand the panels what they need before building their components.
        m_home.setScreen(&screen);
        m_home.setCommandSink(onCommand);
        m_terminal.setCommandSink(onCommand);

        auto home = m_home.component();
        auto universe = m_universe.component();
        auto terminal = m_terminal.component();

        // Only the child at m_view renders and receives events; all stay alive, so
        // each panel keeps its state across switches.
        auto tabs = Container::Tab({home, universe, terminal}, &m_view);

        // View switcher down the far-right edge; the active view's button lights
        // up. Exit sits at the bottom, separated from the view buttons.
        auto viewButtons = Toolbar({
            {"Home", [this] { m_view = 0; }, nullptr, [this] { return m_view == 0; }},
            {"Uni", [this] { m_view = 1; }, nullptr, [this] { return m_view == 1; }},
            {"Term", [this] { m_view = 2; }, nullptr, [this] { return m_view == 2; }},
        });
        auto exitButton = Toolbar({
            {"Exit", [&onCommand, &screen] { onCommand("exit"); screen.Exit(); }, nullptr, nullptr},
        });
        auto viewBar = Container::Vertical({viewButtons, exitButton});

        auto layout = Container::Horizontal({tabs, viewBar});
        auto renderer = Renderer(layout, [this, tabs, viewButtons, exitButton]
                                 {
            // View buttons at the top, Exit pushed to the bottom of the column.
            auto bar = vbox({
                viewButtons->Render(),
                filler(),
                exitButton->Render(),
            });

            // Status bar shown under every view: live counts on the left, the
            // shortcuts for the current view on the right.
            const int online = m_home.onlineCount();
            const int selected = m_exec ? static_cast<int>(m_exec->selection().size()) : 0;
            std::vector<std::pair<std::string, std::string>> hints{
                {"1/2/3", "views"},
            };
            if (m_view == 0)
            {
                hints.push_back({"click", "select"});
                hints.push_back({"shift+click", "add"});
                hints.push_back({"m", "multi"});
                hints.push_back({"a", "all"});
                hints.push_back({"c", "clear"});
            }
            else if (m_view == 2)
            {
                hints.push_back({"enter", "run"});
            }
            hints.push_back({"esc", m_view == 0 ? "exit" : "back"});

            return vbox({
                       hbox({tabs->Render() | flex, bar}) | flex,
                       statusBar(online, selected, hints),
                   }) |
                   flex; });

        auto with_keys = CatchEvent(renderer, [this, &onCommand, &screen](Event event)
                                    {
            // Digit keys switch views, but only when the active panel isn't taking
            // text input (so you can still type digits in the terminal / fields).
            if (event.is_character() && !typingText())
            {
                if (event == Event::Character('1')) { m_view = 0; return true; }
                if (event == Event::Character('2')) { m_view = 1; return true; }
                if (event == Event::Character('3')) { m_view = 2; return true; }
            }

            // Escape backs out to Home; from Home it exits the app.
            if (event == Event::Escape)
            {
                if (m_view != 0) { m_view = 0; return true; }
                onCommand("exit"); // routes through CommandExecutor → Display::quit()
                screen.Exit();
                return true;
            }
            return false; });

        // Keep ping squares (and newly-discovered devices) fresh while up.
        std::atomic<bool> ticking{true};
        std::thread ticker([&]()
                           {
            while (ticking.load())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                onStateChanged();
            } });

        screen.Loop(with_keys);

        ticking = false;
        ticker.join();
        m_home.setScreen(nullptr);
        m_app = nullptr;
    }

    void MainView::onLog(const Loggable &entry)
    {
        m_terminal.onLog(entry);
        redraw(); // thread-safe: posts a Custom event to repaint
    }

    void MainView::onStateChanged()
    {
        if (auto *s = m_app.load())
        {
            s->Post([this, s]()
                    {
                m_home.sync();
                m_universe.sync();
                m_terminal.sync();
                s->PostEvent(Event::Custom); });
        }
    }

}
