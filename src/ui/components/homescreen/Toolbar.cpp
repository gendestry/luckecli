#include "ui/components/homescreen/Toolbar.h"

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
        {
            const std::string label = item.label;
            auto enabled = item.enabled; // copied into the closures below
            auto active = item.active;
            auto labelFn = item.labelFn;
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
                const bool lit = active && active(); // toggle / armed state
                const Color fg = !on        ? Color::RGB(80, 80, 90)     // disabled
                                 : lit       ? Color::RGB(90, 200, 110)   // engaged
                                 : s.focused ? Color::RGB(190, 160, 230)  // focused
                                             : Color::RGB(180, 180, 190); // idle
                const std::string txt = labelFn ? labelFn() : label;
                // Square, bordered buttons stacked vertically: fixed 5x3 cells
                // with the label centered inside the box.
                auto e = text(txt);
                if (lit)
                    e = e | bold;
                // Tight: just a border hugging the label, uniform 4-wide so the
                // stack lines up (fits the longest label, e.g. "RST?").
                auto box = e | hcenter | size(WIDTH, EQUAL, 4) | border | color(fg);
                if (s.focused && on)
                    box = box | bgcolor(Color::RGB(40, 40, 55));
                return box;
            };

            buttons.push_back(Button(opt));
        }

        return Container::Vertical(std::move(buttons));
    }

}
