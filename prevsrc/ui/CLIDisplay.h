#pragma once
#include "ui/Display.h"

#include <string>
#include <vector>

namespace ui
{

    // Plain stdin/stdout frontend: prints the prompt, reads a line, hands it to
    // the command callback, and writes log entries straight to stdout. On a TTY
    // it reads in raw mode for up/down command history (persisted to
    // ~/.luckecli_history); on piped input it falls back to plain getline.
    class CLIDisplay : public Display
    {
        std::string m_prompt = "> ";
        bool m_quit = false;

        // Command history is owned by the executor (shared with the TUI and
        // persisted there); the CLI only reads it for up/down recall.
        core::commands::CommandExecutor *m_exec = nullptr;

        // Read a line with up/down history navigation; false on EOF. UI thread only.
        bool readLine(std::string &out, const std::vector<std::string> &history);

    public:
        void run(std::function<bool(const std::string &)> onCommand) override;
        void setPrompt(const std::string &prompt) override;
        void onLog(const Loggable &entry) override;
        void quit() override;
        void bindCommandSource(core::commands::CommandExecutor *exec) override { m_exec = exec; }
    };

}
