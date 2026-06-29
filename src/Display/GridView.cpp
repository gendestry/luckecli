#include "GridView.h"
#include "Components/InfoPanel.h"
#include "Components/StatusBar.h"
#include "Components/Toolbar.h"
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
        : m_state(state), m_exec(exec), m_grid(state, exec), m_presets(state, exec) {}

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
                    m_presets.sync();
                    s->PostEvent(Event::Custom); });
        };

        // Picking a preset in the dropdown applies it to the single selected
        // fixture, then refreshes so the cache-backed views catch up.
        m_presets.setOnApply([&onCommand, this](int index)
                             { onCommand("setpreset " + std::to_string(index)); if (m_refresh) m_refresh(); });

        // Wire the grid to the view: a click adds to the selection when a modifier
        // is held OR sticky multi-mode is on; it asks for a refresh whenever a
        // click changes the selection.
        m_grid.setAdditiveProvider([this]
                                   { return m_modifier || m_multiMode; });
        m_grid.setOnSelect([this]
                           { if (m_refresh) m_refresh(); });
        m_grid.rebuild();
        m_presets.sync();

        // Selection-aware predicates shared by the toolbar buttons.
        auto hasSelection = [this]
        { return m_exec && m_exec->isSelected(); };
        auto singleSelection = [this]
        { return m_exec && m_exec->selection().size() == 1; };

        auto toolbar = Toolbar({
            {"Multi", [this] { m_multiMode = !m_multiMode; if (m_refresh) m_refresh(); }, nullptr,
             [this] { return m_multiMode; }},
            {"Select All", [this] { if (m_exec) m_exec->selectAll(); if (m_refresh) m_refresh(); }, nullptr, nullptr},
            {"Clear", [this] { if (m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"Identify", [&onCommand, this] { onCommand("highlight"); if (m_refresh) m_refresh(); }, singleSelection, nullptr},
            {"Reboot", [&onCommand, this] { onCommand("reboot"); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"Exit", [&onCommand, this] { onCommand("exit"); requestExit(); }, nullptr, nullptr},
        });

        // The preset dropdown only joins the focus path when it's active (a single
        // fixture with cached presets); otherwise Tab/arrows skip over it.
        auto presetSlot = Maybe(m_presets.component(), [this]
                                { return m_presets.active(); });
        auto layout = Container::Vertical({
            toolbar,
            Container::Horizontal({m_grid.component(), presetSlot}),
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
                        infoPanel(m_state, m_exec->selection()),
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

        auto with_keys = CatchEvent(renderer, [&](Event event)
                                    {
            // Track ctrl/shift so card clicks can tell single- from multi-select
            // (runs before the cards handle the click). Either modifier works,
            // since terminals vary in which one they forward.
            if (event.is_mouse())
                m_modifier = event.mouse().shift || event.mouse().control;

            // Keyboard shortcuts for the multi-select toolbar actions.
            if (event == Event::Character('m')) { m_multiMode = !m_multiMode;          if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('a')) { if (m_exec) m_exec->selectAll();      if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('c')) { if (m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); return true; }

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
                m_presets.sync();
                s->PostEvent(Event::Custom); });
        }
    }

}
