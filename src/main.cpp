#include "CommandExecutor.h"
#include "SharedState.h"
#include "Heartbeat.h"

int main() {
    SharedState state;
    Heartbeat heartbeat(state);
    CommandExecutor exec(state);

    exec.run();

    return 0;
}
