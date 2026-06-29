#pragma once
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "Commands/CommandExecutor.h"

namespace Test
{

    class SharedState;

    // A 16×32 map of a single DMX universe's 512 channels. Channels occupied by
    // the selected fixture(s) — the span [address, address + footprint) — are
    // filled in the fixture's colour; overlaps show red. The universe shown is
    // the one the first selected fixture sits on. Read-only for now.
    ftxui::Element universeGrid(SharedState &state,
                                const std::vector<Commands::CommandExecutor::Selection> &selection);

}
