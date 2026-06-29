#include "GridView.h"
#include "Components/InfoPanel.h"
#include "Components/StatusBar.h"
#include "Components/Toolbar.h"
#include "Components/UniverseGrid.h"
#include "SharedState.h"
#include "Commands/CommandExecutor.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace Test
{
    using namespace ftxui;

    // A function (not a namespace-scope constant): Color::RGB queries FTXUI's
    // terminal globals, which aren't constructed yet during static init.
    static Color accent() { return Color::RGB(100, 150, 220); }

    GridView::GridView(SharedState &state, Commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec), m_grid(state, exec), m_info(state, exec), m_presets(state, exec) {}

    void GridView::run(std::function<bool(const std::string &)> &onCommand)
    {
        auto screen = App::Fullscreen();
        m_app = &screen;

        // Schedule a card rebuild + repaint on the UI thread. Posted (not run
        // inline) so a click handler never swaps the cards out from under itself.
        m_refresh = [this]()
        {
            if (auto *s = m_app.load())
                s->Post([this, s]()
                        {
                    m_grid.rebuild();
                    m_info.sync();
                    m_presets.sync();
                    s->PostEvent(Event::Custom); });
        };

        // Commands triggered from the UI (preset pick, field edit) do blocking
        // network I/O — running them inline would freeze the FTXUI loop. Dispatch
        // them on a detached thread instead, gated by m_busy so overlapping
        // requests can't race on the shared ESPClient queue, and post a refresh
        // once the device responds. m_command is a copy of the sink (onCommand is
        // a stack reference here and the thread outlives this call).
        m_command = onCommand;
        auto dispatch = [this](std::string cmd)
        {
            if (m_busy.exchange(true))
                return; // an operation is already in flight; drop this one
            std::thread([this, cmd = std::move(cmd)]()
                        {
                m_command(cmd);
                m_busy = false;
                if (m_refresh) m_refresh(); })
                .detach();
        };

        // Picking a preset applies it to the single selected fixture; editing a
        // field in the Info panel runs the matching command. Both refresh the
        // cache-backed views when the (off-thread) command completes.
        m_presets.setOnApply([dispatch](int index)
                             { dispatch("setpreset " + std::to_string(index)); });
        m_info.setOnCommand([dispatch](const std::string &cmd)
                            { dispatch(cmd); });

        // Wire the grid to the view: a click adds to the selection when a modifier
        // is held OR sticky multi-mode is on; it asks for a refresh whenever a
        // click changes the selection.
        m_grid.setAdditiveProvider([this]
                                   { return m_modifier || m_multiMode; });
        m_grid.setOnSelect([this]
                           { if (m_refresh) m_refresh(); });
        // Freeze the selection while a command runs off-thread, so the background
        // command reads a stable selection (only concurrent reads remain).
        m_grid.setSelectableProvider([this]
                                     { return !m_busy.load(); });
        m_grid.rebuild();
        m_info.sync();
        m_presets.sync();

        // Selection-aware predicates shared by the toolbar buttons.
        auto hasSelection = [this]
        { return m_exec && m_exec->isSelected(); };
        auto singleSelection = [this]
        { return m_exec && m_exec->selection().size() == 1; };

        auto toolbar = Toolbar({
            {"Multi", [this] { m_multiMode = !m_multiMode; if (m_refresh) m_refresh(); }, nullptr,
             [this] { return m_multiMode; }},
            {"Select All", [this] { if (m_busy) return; if (m_exec) m_exec->selectAll(); if (m_refresh) m_refresh(); }, nullptr, nullptr},
            {"Clear", [this] { if (m_busy) return; if (m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"Identify", [&onCommand, this] { onCommand("highlight"); if (m_refresh) m_refresh(); }, singleSelection, nullptr},
            {"Reboot", [&onCommand, this] { onCommand("reboot"); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"Universe", [this] { m_showUniverse = !m_showUniverse; }, hasSelection,
             [this] { return m_showUniverse; }},
            {"Exit", [&onCommand, this] { onCommand("exit"); requestExit(); }, nullptr, nullptr},
        });

        // The info form and preset dropdown only join the focus path when active
        // (a single live fixture); otherwise Tab/arrows skip over them.
        auto infoSlot = Maybe(m_info.component(), [this]
                              { return m_info.active(); });
        auto presetSlot = Maybe(m_presets.component(), [this]
                                { return m_presets.active(); });
        auto layout = Container::Vertical({
            toolbar,
            Container::Horizontal({m_grid.component(), infoSlot, presetSlot}),
        });

        auto renderer = Renderer(layout, [&]
                                 {
            auto bar = toolbar->Render() | bgcolor(Color::RGB(30, 30, 40));

            auto fixtures = window(
                text(" Online Fixtures ") | bold | color(accent()),
                m_grid.component()->Render() | center | flex);

            Element body;
            if (m_exec && m_exec->isSelected())
            {
                auto info = window(
                    text(" Info ") | bold | color(accent()),
                    vbox({
                        m_info.render(),
                        m_presets.render(),
                    }) | flex);
                body = hbox({fixtures | flex, info | size(WIDTH, EQUAL, 40)}) | flex;
            }
            else
            {
                body = fixtures | flex;
            }

            const int selected = m_exec ? static_cast<int>(m_exec->selection().size()) : 0;
            return vbox({
                       bar,
                       body | flex,
                       statusBar(m_grid.onlineCount(), selected),
                   }) |
                   flex; });

        // The universe-viewer popup: a centered, bordered window over the grid.
        // Modal() handles the centering/overlay; this just draws the content.
        auto universePopup = Renderer([this]
                                      {
            std::vector<Commands::CommandExecutor::Selection> sel;
            if (m_exec)
                sel = m_exec->selection();
            return window(
                       text(" Universe Viewer ") | bold | color(accent()),
                       vbox({
                           universeGrid(m_state, sel),
                           text(" u / esc to close") | dim | center,
                       })) |
                   bgcolor(Color::RGB(20, 20, 28)); });

        auto root = renderer | Modal(universePopup, &m_showUniverse);

        auto with_keys = CatchEvent(root, [&](Event event)
                                    {
            // Track ctrl/shift so card clicks can tell single- from multi-select
            // (runs before the cards handle the click). Either modifier works,
            // since terminals vary in which one they forward.
            if (event.is_mouse())
                m_modifier = event.mouse().shift || event.mouse().control;

            // Universe popup: 'u' toggles it; while open, Escape just closes it.
            if (event == Event::Character('u') && m_exec && m_exec->isSelected())
            {
                m_showUniverse = !m_showUniverse;
                return true;
            }
            if (m_showUniverse && event == Event::Escape)
            {
                m_showUniverse = false;
                return true;
            }

            // Keyboard shortcuts for the multi-select toolbar actions. Selection
            // changes are frozen while a command runs off-thread (m_busy).
            if (event == Event::Character('m')) { m_multiMode = !m_multiMode;          if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('a')) { if (!m_busy && m_exec) m_exec->selectAll();      if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('c')) { if (!m_busy && m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); return true; }

            if (event == Event::Escape)
            {
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
        m_refresh = nullptr;
        m_app = nullptr;
    }

    void GridView::onStateChanged()
    {
        if (auto *s = m_app.load())
        {
            s->Post([this, s]()
                    {
                m_grid.rebuild();
                m_info.sync();
                m_presets.sync();
                s->PostEvent(Event::Custom); });
        }
    }

}
