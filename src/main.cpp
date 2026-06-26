// #include "Commands/CommandExecutor.h"
#include "Test/SharedState.h"
#include "Test/Heartbeat.h"
// #include "Diary/Log.h"
// #include "Display/CLIDisplay.h"
// #include "Display/FTXUIDisplay.h"

// #include <memory>

int main()
{
    Test::SharedState state;

    // auto display = std::make_unique<CLIDisplay>();
    // auto display = std::make_unique<FTXUIDisplay>(state);

    // Log::setGlobalCallback([&display](const Loggable &l)
    //                        { display->onLog(l); });

    // state.onChangeCallback = [&display]()
    // {
    //     display->onStateChanged();
    // };

    Test::Heartbeat heartbeat(state);
    int counter = 0;
    while (true)
    {
        counter++;
        if (counter == 100)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // CommandExecutor exec(state, *display);
    // display->bindCommandSource(&exec);

    // exec.run();

    return 0;
}
