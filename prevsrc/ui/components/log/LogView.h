#pragma once
#include "ui/Panel.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace core { class SharedState; }
namespace core::commands { class CommandExecutor; }

namespace ui
{

    // The device log (firmware >= 0.99) of the selected fixture's device.
    // Polls `logs` with `since` while visible, so only new lines are fetched.
    class LogView : public Panel
    {
    public:
        LogView(core::SharedState &state, core::commands::CommandExecutor *&exec);

        ftxui::Component component() override;
        const char *title() const override { return "Log"; }

        // Fetch new lines for the selected device. Call periodically while the
        // view is shown; returns at once if a fetch is still in flight.
        void poll();

        // Called (from a network thread) when new lines arrived.
        void setOnUpdate(std::function<void()> fn) { m_onUpdate = std::move(fn); }

    private:
        std::string selectedIp() const;

        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;
        std::function<void()> m_onUpdate;

        mutable std::mutex m_mutex;   // guards everything below
        std::string m_ip;             // device the lines belong to
        uint32_t m_next = 0;          // `since` for the next fetch
        std::vector<std::string> m_lines;
        std::string m_status;         // "no log", errors; empty when fine

        std::atomic<bool> m_inFlight{false};
        int m_scroll = 0;             // lines up from the bottom; 0 follows new lines

        // Network callbacks hold a weak_ptr to this; it dies with the view, so a
        // reply arriving during shutdown is ignored.
        std::shared_ptr<int> m_alive = std::make_shared<int>(0);
    };

}
