#include "ui/components/Toolbar.h"

#include <ftxui/dom/elements.hpp>

#include <utility>

namespace ui
{
    using namespace ftxui;

    ftxui::Component Toolbar(std::vector<ToolbarItem> items)
    {
        Components buttons;
        buttons.reserve(items.size());

        for (auto &item : items)
        {
            const std::string label = item.label;
            auto enabled = item.enabled;       // copied into the closures below
            auto active = item.active;
            auto action = std::move(item.onClick);

            auto opt = ButtonOption::Simple();
            opt.on_click = [enabled, action]()
            {
                if (action && (!enabled || enabled()))
                    action();
            };

            opt.transform = [=](const EntryState &s)
            {
                const bool on = !enabled || enabled();
                const bool lit = active && active(); // toggle currently engaged
                const Color fg = !on        ? Color::RGB(80, 80, 90)     // disabled
                                 : lit       ? Color::RGB(90, 200, 110)   // toggled on
                                 : s.focused ? Color::RGB(190, 160, 230)  // focused
                                             : Color::RGB(180, 180, 190); // idle
                auto e = text(" " + std::string(lit ? "● " : "") + label + " ") | color(fg);
                if (lit)
                    e = e | bold;
                if (s.focused && on)
                    e = e | bgcolor(Color::RGB(40, 40, 55));
                return e;
            };

            buttons.push_back(Button(opt));
        }

        return Container::Horizontal(std::move(buttons));
    }

}
