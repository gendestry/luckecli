#pragma once
#include <ftxui/component/component.hpp>

#include "ui/components/menubar/Menubar.h"

namespace ui
{

    // A single square, bordered toolbar button built from a MenubarItem
    // (label / onClick / enabled / active / labelFn). The label is centered in a
    // fixed-width box so a vertical stack of these lines up regardless of label
    // length. `width` is the inner label width in cells.
    ftxui::Component ToolbarButton(MenubarItem item, int width = 4);

}
