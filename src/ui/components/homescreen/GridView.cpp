#include "ui/components/homescreen/GridView.h"
#include "ui/components/homescreen/infopanel/InfoPanel.h"
#include "ui/components/menubar/Menubar.h"
#include "ui/components/homescreen/Toolbar.h"
#include "ui/components/homescreen/UniverseGrid.h"
#include "core/domain/SharedState.h"
#include "core/commands/CommandExecutor.h"
#include "ui/Theme.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <thread>

namespace ui
{
    using namespace ftxui;

    // A function (not a namespace-scope constant): Color::RGB queries FTXUI's
    // terminal globals, which aren't constructed yet during static init.
    static Color accent() { return Theme::windowTitle(); }

    GridView::GridView(core::SharedState &state, core::commands::CommandExecutor *&exec)
        : m_state(state), m_exec(exec), m_grid(state, exec), m_info(state, exec), m_presets(state, exec) {}

    void GridView::sync()
    {
        m_grid.rebuild();
        m_info.sync();
        m_presets.sync();
    }

    bool GridView::editing() const { return m_info.editing(); }

    int GridView::onlineCount() const { return m_grid.onlineCount(); }

    ftxui::Component GridView::component()
    {
        // Schedule a card rebuild + repaint on the UI thread. Posted (not run
        // inline) so a click handler never swaps the cards out from under itself.
        m_refresh = [this]()
        {
            if (m_screen)
                m_screen->Post([this]()
                        {
                    sync();
                    m_screen->PostEvent(Event::Custom); });
        };

        // Commands triggered from the UI (preset pick, field edit) do blocking
        // network I/O — running them inline would freeze the FTXUI loop. Dispatch
        // them on a detached thread instead, gated by m_busy so overlapping
        // requests can't race on the shared ESPClient queue, and post a refresh
        // once the device responds. m_command is set by the host before component().
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

        // Embed the preset picker inside the Info panel's Fixture section, rather
        // than as a separate panel: its focusable component joins the fixture
        // sub-tree and its element renders among the fixture fields.
        m_info.setPresetSection(m_presets.component(),
                                [this] { return m_presets.render(); },
                                [this] { return m_presets.active(); });

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
        sync();

        // Selection-aware predicates shared by the menubar buttons.
        auto hasSelection = [this]
        { return m_exec && m_exec->isSelected(); };

        auto menubar = Menubar({
            {"Multi", [this] { m_multiMode = !m_multiMode; if (m_refresh) m_refresh(); }, nullptr,
             [this] { return m_multiMode; }},
            {"Select All", [this] { if (m_busy) return; if (m_exec) m_exec->selectAll(); if (m_refresh) m_refresh(); }, nullptr, nullptr},
            {"Clear", [this] { if (m_busy) return; if (m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"Sort", [this] { m_grid.setSortByName(!m_grid.sortByName()); m_grid.rebuild(); if (m_refresh) m_refresh(); }, nullptr,
             [this] { return m_grid.sortByName(); }},
        });

        // Vertical action toolbar on the right. Every button targets the whole
        // current selection (highlight/reboot/factoryreset all iterate the
        // selection), so they share `hasSelection` and gray out when nothing is
        // picked. New fixture actions should follow the same pattern.
        auto toolbar = Toolbar({
            {"HL", [this] { m_command("highlight"); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            {"RBT", [this] { m_command("reboot"); if (m_refresh) m_refresh(); }, hasSelection, nullptr},
            // Factory reset is destructive: first click arms (label becomes
            // "RST?" and lights up), second click fires. Disarmed clicks never
            // wipe anything.
            {"RST",
             [this] {
                 if (!m_armReset) { m_armReset = true; return; }
                 m_command("factoryreset confirm");
                 m_armReset = false;
                 if (m_refresh) m_refresh();
             },
             hasSelection,
             [this] { return m_armReset; },
             [this] { return m_armReset ? std::string("RST?") : std::string("RST"); }},
            // Blackout targets every fixture (all universes), not the selection,
            // so it stays enabled regardless of what's picked.
            {"BLK", [this] { m_command("blackout"); if (m_refresh) m_refresh(); }, nullptr, nullptr},
        });

        // The info panel only joins the focus path when it has interactive content
        // (a single live fixture's form, or the multi-selection fold headers);
        // otherwise Tab/arrows skip over it. The preset picker is nested inside the
        // info panel now, so it's no longer a separate slot.
        auto infoSlot = Maybe(m_info.component(), [this]
                              { return m_info.focusable(); });

        auto layout = Container::Vertical({
            menubar,
            Container::Horizontal({m_grid.component(), infoSlot, toolbar}),
        });

        auto renderer = Renderer(layout, [this, menubar, toolbar]
                                 {
            auto bar = menubar->Render() | bgcolor(Color::RGB(30, 30, 40));

            auto fixtures = window(
                text(" Online Fixtures ") | bold | color(accent()),
                m_grid.component()->Render() | center | flex);

            auto actions = toolbar->Render();

            Element body;
            if (m_exec && m_exec->isSelected())
            {
                auto info = window(
                    text(" Info ") | bold | color(accent()),
                    m_info.render() | flex);
                body = hbox({actions, fixtures | flex, info | size(WIDTH, EQUAL, 40)}) | flex;
            }
            else
            {
                body = hbox({actions, fixtures | flex}) | flex;
            }

            return vbox({
                       bar,
                       body | flex,
                   }) |
                   flex; });

        // The universe-viewer popup: a centered, bordered window over the grid.
        // Modal() handles the centering/overlay; this just draws the content.
        auto universePopup = Renderer([this]
                                      {
            std::vector<core::commands::CommandExecutor::Selection> sel;
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

        // Home-view-local shortcuts. These only fire while the Home tab is active,
        // since Container::Tab only routes events to the focused child. Global
        // concerns (exit, view switching) live in the host.
        auto with_keys = CatchEvent(root, [this](Event event)
                                    {
            // Track ctrl/shift so card clicks can tell single- from multi-select
            // (runs before the cards handle the click). Either modifier works,
            // since terminals vary in which one they forward.
            if (event.is_mouse())
            {
                m_modifier = event.mouse().shift || event.mouse().control;

                // Mouse wheel scrolls the info panel content.
                if (event.mouse().button == Mouse::WheelUp) { m_info.scroll(-1); return true; }
                if (event.mouse().button == Mouse::WheelDown) { m_info.scroll(1); return true; }
            }

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

            // Keyboard shortcuts for the multi-select menubar actions. Selection
            // changes are frozen while a command runs off-thread (m_busy).
            if (event == Event::Character('m')) { m_multiMode = !m_multiMode;          if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('a') || event == Event::CtrlA) { if (!m_busy && m_exec) m_exec->selectAll();      if (m_refresh) m_refresh(); return true; }
            if (event == Event::Character('c')) { if (!m_busy && m_exec) m_exec->clearSelection(); if (m_refresh) m_refresh(); return true; }

            return false; });

        return with_keys;
    }

}
