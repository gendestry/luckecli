#include "ui/components/homescreen/Toolbar.h"
#include "ui/components/homescreen/ToolbarButton.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <utility>

namespace ui
{
    using namespace ftxui;

    ftxui::Component Toolbar(std::vector<MenubarItem> items)
    {
        Components buttons;
        buttons.reserve(items.size());

        for (auto &item : items)
            buttons.push_back(ToolbarButton(std::move(item)));

        auto container = Container::Vertical(std::move(buttons));

        // Top-align the button stack (horizontally centered), pushing empty
        // space below with a filler.
        return Renderer(container, [container]
                        { return vbox({
                                     container->Render(),
                                     filler(),
                                 }) |
                                 hcenter; });
    }

}
