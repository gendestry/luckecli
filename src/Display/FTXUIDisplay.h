#pragma once
#include "Display.h"
#include "GridView.h"
#include "DetailView.h"
#include <mutex>
#include <string>

class SharedState;
class CommandExecutor;

// Coordinator. Owns the grid + detail views and alternates between them in
// run(). The thread-facing Display callbacks (onStateChanged/onLog/quit) are
// routed to whichever view is currently active.
class FTXUIDisplay : public Display {
public:
    explicit FTXUIDisplay(SharedState& state);

    void run(std::function<bool(const std::string&)> onCommand) override;
    void setPrompt(const std::string& prompt) override;
    void onLog(const Loggable& entry) override;
    void quit() override;
    void onStateChanged() override;
    void bindCommandSource(CommandExecutor* exec) override;

private:
    void setActive(View* v);

    SharedState& m_state;
    CommandExecutor* m_exec = nullptr;   // set via bindCommandSource()
    GridView m_grid;
    DetailView m_detail;

    std::mutex m_routeMutex;             // guards m_active
    View* m_active = nullptr;            // view currently running its loop
    bool m_quit = false;
    std::string m_prompt = "> ";
};
