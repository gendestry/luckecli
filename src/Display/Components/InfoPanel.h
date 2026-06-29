#pragma once
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "Commands/CommandExecutor.h"

namespace Test
{

    class SharedState;

    // Read-only detail panel for the currently-selected fixture(s): name, type,
    // IP and DMX config, read live from the cached client state. Rebuilt every
    // frame so it tracks cache updates (e.g. after a describe).
    ftxui::Element infoPanel(SharedState &state,
                             const std::vector<Commands::CommandExecutor::Selection> &selection);

}
