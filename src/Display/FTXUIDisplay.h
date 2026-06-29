#pragma once
#include "Display.h"
#include "GridView.h"

#include <mutex>
#include <string>

namespace Test
{

    class SharedState;

    // Coordinator for the FTXUI frontend. For now it owns and runs a single view
    // (the online-fixtures grid); the thread-facing Display callbacks
    // (onStateChanged/onLog/quit) are routed to whichever view is active. A
    // detail view can be slotted into run() later without touching this surface.
    class FTXUIDisplay : public Display
    {
    public:
        explicit FTXUIDisplay(SharedState &state);

        void run(std::function<bool(const std::string &)> onCommand) override;
        void setPrompt(const std::string &prompt) override;
        void onLog(const Loggable &entry) override;
        void quit() override;
        void onStateChanged() override;
        void bindCommandSource(Commands::CommandExecutor *exec) override;

    private:
        void setActive(View *v);

        SharedState &m_state;
        Commands::CommandExecutor *m_exec = nullptr; // set via bindCommandSource()
        GridView m_grid;

        std::mutex m_routeMutex; // guards m_active
        View *m_active = nullptr;
        bool m_quit = false;
        std::string m_prompt;
    };

}
