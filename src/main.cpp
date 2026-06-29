#include "Commands/CommandExecutor.h"
#include "SharedState.h"
#include "Heartbeat.h"
#include "Network/ResponseListener.h"
#include "Display/Display.h"
#include "Diary/Log.h"

#include <string_view>
#include <thread>
#include <chrono>

namespace
{
    // Pick the frontend from the command line: `luckecli cli` / `luckecli ftxui`
    // (also accepts --cli / --tui). Defaults to the TUI.
    Test::DisplayType pickDisplay(int argc, char **argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            while (arg.starts_with('-'))
                arg.remove_prefix(1);

            if (arg == "cli")
                return Test::DisplayType::CLI;
            if (arg == "ftxui" || arg == "tui")
                return Test::DisplayType::FTXUI;
        }
        return Test::DisplayType::FTXUI;
    }
}

int main(int argc, char **argv)
{
    Test::SharedState state;
    Log log("main");

    // auto display = Test::makeDisplay(Test::DisplayType::CLI, state);
    auto display = Test::makeDisplay(pickDisplay(argc, argv), state);

    // Route all log output to the chosen display.
    Log::setGlobalCallback([&display](const Loggable &l)
                           { display->onLog(l); });

    Test::Heartbeat heartbeat(state);
    Test::Network::ResponseListener listener(state);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    Test::Commands::CommandExecutor exec(state, *display);
    exec.run();

    listener.stop();

    return 0;
}
