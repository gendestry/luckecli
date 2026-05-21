#include <iostream>
#include <string>

#include "CommandExecutor.h"
#include "SharedState.h"
#include "Diary/Log.h"
#include "Heartbeat.h"


int main() {
    SharedState state;
    Heartbeat heartbeat(state);
    CommandExecutor exec(state);
    exec.run();

    return 0;
}
