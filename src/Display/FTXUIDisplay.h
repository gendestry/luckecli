#pragma once
#include "Display.h"
#include <vector>
#include <mutex>
#include <functional>

namespace ftxui { class App; }

class SharedState;
class CommandExecutor;

class FTXUIDisplay : public Display {
    std::string m_prompt = "> ";
    std::vector<std::string> m_log_lines;
    std::mutex m_mutex;
    ftxui::App* m_app = nullptr;
    SharedState& m_state;
    CommandExecutor* m_exec = nullptr;
    bool m_quit = false;
    bool m_stateChanged = false;

    void runGridView(std::function<bool(const std::string&)>& onCommand);
    void runDetailView(std::function<bool(const std::string&)>& onCommand);

public:
    FTXUIDisplay(SharedState& state);

    void run(std::function<bool(const std::string&)> onCommand) override;
    void setPrompt(const std::string& prompt) override;
    void onLog(const Loggable& entry) override;
    void quit() override;
    void onStateChanged() override;
    void bindCommandSource(CommandExecutor* exec) override;
};
