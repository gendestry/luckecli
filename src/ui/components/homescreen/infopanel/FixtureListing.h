#pragma once
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "core/commands/CommandExecutor.h"

namespace core { class SharedState; }

namespace ui
{


    // Read-only fixture listing for zero/multi selection (and the fallback inside
    // the editable panel). Read live from the cached client state.
    ftxui::Element fixtureListing(core::SharedState &state,
                                  const std::vector<core::commands::CommandExecutor::Selection> &selection);

}
