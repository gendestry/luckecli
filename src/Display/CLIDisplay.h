#pragma once
#include "Display.h"

class CLIDisplay : public Display {
    std::string m_prompt = "> ";
    bool m_quit = false;

public:
    void run(std::function<bool(const std::string&)> onCommand) override;
    void setPrompt(const std::string& prompt) override;
    void onLog(const Loggable& entry) override;
    void quit() override;
};
