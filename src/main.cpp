#include "core/commands/CommandExecutor.h"
#include "core/domain/SharedState.h"
#include "core/net/Heartbeat.h"
#include "core/net/ResponseListener.h"
#include "ui/Display.h"
#include "support/log/Log.h"

#include <string_view>
#include <thread>
#include <chrono>

namespace
{
    // Pick the frontend from the command line: `luckecli cli` / `luckecli ftxui`
    // (also accepts --cli / --tui). Defaults to the TUI.
    ui::DisplayType pickDisplay(int argc, char **argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            while (arg.starts_with('-'))
                arg.remove_prefix(1);

            if (arg == "cli")
                return ui::DisplayType::CLI;
            if (arg == "ftxui" || arg == "tui")
                return ui::DisplayType::FTXUI;
        }
        return ui::DisplayType::FTXUI;
    }
}

int main(int argc, char **argv)
{
    core::SharedState state;
    Log log("main");

    // auto display = ui::makeDisplay(ui::DisplayType::CLI, state);
    auto display = ui::makeDisplay(pickDisplay(argc, argv), state);

    // Route all log output to the chosen display.
    Log::setGlobalCallback([&display](const Loggable &l)
                           { display->onLog(l); });

    core::Heartbeat heartbeat(state);
    core::net::ResponseListener listener(state);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    core::commands::CommandExecutor exec(state, *display);
    exec.run();

    // Detach the log sink from `display` before teardown: background worker
    // threads (still draining describe responses) log through this callback, and
    // `display` is destroyed below — without this they'd call into freed memory.
    Log::setGlobalCallback();

    listener.stop();

    return 0;
}
