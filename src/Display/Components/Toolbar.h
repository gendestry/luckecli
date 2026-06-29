#pragma once
#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace Test
{

    // One clickable action in the toolbar. `enabled` is consulted every frame so
    // buttons can dim themselves when they don't apply to the current selection
    // (e.g. "Clear" with nothing selected); leave it null for always-on.
    struct ToolbarItem
    {
        std::string label;
        std::function<void()> onClick;
        std::function<bool()> enabled; // null == always enabled
        std::function<bool()> active;  // null == plain button; else a toggle, lit when true
    };

    // A horizontal row of action buttons. The returned component owns focus
    // navigation across the buttons; the host frames it (border, title, etc.).
    // Clicking a disabled button is a no-op.
    ftxui::Component Toolbar(std::vector<ToolbarItem> items);

}
