#include "CommandExecutor.h"
#include "SharedState.h"
#include "Heartbeat.h"
#include "Diary/Log.h"
#include "Display/CLIDisplay.h"
#include "Display/FTXUIDisplay.h"

#include <memory>

int main()
{
    SharedState state;

    // auto display = std::make_unique<CLIDisplay>();
    auto display = std::make_unique<FTXUIDisplay>(state);

    Log::setGlobalCallback([&display](const Loggable &l)
                           { display->onLog(l); });

    state.onChangeCallback = [&display]()
    {
        display->onStateChanged();
    };

    Heartbeat heartbeat(state);
    CommandExecutor exec(state, *display);
    display->bindCommandSource(&exec);

    exec.run();

    return 0;
}
