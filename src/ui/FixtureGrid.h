#pragma once
#include <functional>

#include <ftxui/component/component.hpp>

namespace core { class SharedState; }
namespace core::commands { class CommandExecutor; }

namespace ui
{


    // The scrollable grid of fixture cards. Owns a stable root container whose
    // children (the FixtureCard buttons) are rebuilt in place from the latest
    // shared-state snapshot + selection; only the cards churn, the root persists
    // so focus survives a refresh. Pure UI: it reads the executor's selection and
    // calls selectIndex() on click, but holds no selection state of its own.
    class FixtureGrid
    {
    public:
        FixtureGrid(core::SharedState &state, core::commands::CommandExecutor *&exec);

        // The component to mount in the view's layout tree.
        ftxui::Component component() const { return m_container; }

        // Rebuild the cards from the current snapshot + selection. UI thread only.
        void rebuild();

        // How many of the laid-out fixtures are currently online (valid after the
        // last rebuild) — fed to the status bar.
        int onlineCount() const { return m_onlineCount; }

        // Host-supplied hooks: whether the next click should add to the selection
        // (multi-select) rather than replace it, and what to run after a click
        // changes the selection (schedule a redraw).
        void setAdditiveProvider(std::function<bool()> fn) { m_additive = std::move(fn); }
        void setOnSelect(std::function<void()> fn) { m_onSelect = std::move(fn); }

        // Optional gate: when set and it returns false, clicks are ignored (the
        // selection can't change). Used to freeze the selection while a command is
        // running off-thread against it.
        void setSelectableProvider(std::function<bool()> fn) { m_canSelect = std::move(fn); }

        void setColumns(int cols) { m_cols = cols; }

    private:
        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;

        ftxui::Component m_container; // stable root (Container::Vertical of rows)
        std::function<bool()> m_additive;
        std::function<void()> m_onSelect;
        std::function<bool()> m_canSelect;
        int m_cols = 3;
        int m_onlineCount = 0;
    };

}
