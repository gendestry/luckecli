#include <iostream>
#include <string>

#include "CommandExecutor.h"
#include "SharedState.h"
#include "Diary/Log.h"
#include "Heartbeat.h"


int main() {
    Log logger("Main");
    SharedState state;
    Heartbeat heartbeat(state);
    CommandExecutor exec(state);

    logger.print("> ");
    std::string line;
    while (std::getline(std::cin, line)) {
        auto resolved = exec.resolveCommand(line);
        if (exec.shouldQuit()) {
            break;
        }
    }

    return 0;
}
