#include <iostream>
#include <string>

#include "Utils/Logging/Logger.h"
#include "CommandExecutor.h"
#include "SharedState.h"

int main() {
    Utils::Logger logger("Main");
    logger.toggleScope();

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