#pragma once
#include <vector>

#include <ftxui/component/component.hpp>

#include "ui/components/menubar/Menubar.h"

namespace ui
{

    // A vertical stack of action buttons rendered down the right edge of the
    // homescreen. Reuses MenubarItem (label / onClick / enabled / active /
    // labelFn); a disabled item is dimmed and clicking it is a no-op — used so
    // fixture actions gray out when nothing is selected. Labels are meant to be
    // short (abbreviations) since the column is narrow.
    ftxui::Component Toolbar(std::vector<MenubarItem> items);

}
