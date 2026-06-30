#pragma once
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "Commands/CommandExecutor.h"

namespace Test
{

    class SharedState;

    // Read-only fixture listing for zero/multi selection (and the fallback inside
    // the editable panel). Read live from the cached client state.
    ftxui::Element fixtureListing(SharedState &state,
                                  const std::vector<Commands::CommandExecutor::Selection> &selection);

}
