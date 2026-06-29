#include "Test/Commands/CommandExecutor.h"
#include "Test/SharedState.h"
#include "Test/Heartbeat.h"
#include "Test/ESPClient.h"
#include "Test/Network/ResponseListener.h"
#include "JSONtemp.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#include "Diary/Log.h"
// #include "Display/CLIDisplay.h"
// #include "Display/FTXUIDisplay.h"

// #include <memory>

int main()
{
    Test::SharedState state;
    Log log("main");

    // auto display = std::make_unique<CLIDisplay>();
    // auto display = std::make_unique<FTXUIDisplay>(state);

    // Log::setGlobalCallback([&display](const Loggable &l)
    //                        { display->onLog(l); });

    // state.onChangeCallback = [&display]()
    // {
    //     display->onStateChanged();
    // };

    Test::Heartbeat heartbeat(state);
    Test::Network::ResponseListener listener(state);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    Test::Commands::CommandExecutor exec(state);
    exec.run();

    listener.stop();

    return 0;
}
