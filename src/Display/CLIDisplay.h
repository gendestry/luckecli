#pragma once
#include "Display.h"

namespace Test
{

    // Plain stdin/stdout frontend: prints the prompt, reads a line, hands it to
    // the command callback, and writes log entries straight to stdout.
    class CLIDisplay : public Display
    {
        std::string m_prompt = "> ";
        bool m_quit = false;

    public:
        void run(std::function<bool(const std::string &)> onCommand) override;
        void setPrompt(const std::string &prompt) override;
        void onLog(const Loggable &entry) override;
        void quit() override;
    };

}
