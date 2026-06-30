#pragma once
#include "ui/Display.h"
#include "ui/GridView.h"

#include <mutex>
#include <string>

namespace core { class SharedState; }

namespace ui
{


    // Coordinator for the FTXUI frontend. For now it owns and runs a single view
    // (the online-fixtures grid); the thread-facing Display callbacks
    // (onStateChanged/onLog/quit) are routed to whichever view is active. A
    // detail view can be slotted into run() later without touching this surface.
    class FTXUIDisplay : public Display
    {
    public:
        explicit FTXUIDisplay(core::SharedState &state);

        void run(std::function<bool(const std::string &)> onCommand) override;
        void setPrompt(const std::string &prompt) override;
        void onLog(const Loggable &entry) override;
        void quit() override;
        void onStateChanged() override;
        void bindCommandSource(core::commands::CommandExecutor *exec) override;

    private:
        void setActive(View *v);

        core::SharedState &m_state;
        core::commands::CommandExecutor *m_exec = nullptr; // set via bindCommandSource()
        GridView m_grid;

        std::mutex m_routeMutex; // guards m_active
        View *m_active = nullptr;
        bool m_quit = false;
        std::string m_prompt;
    };

}
