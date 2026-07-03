#include "ui/components/homescreen/ToolbarButton.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <utility>

namespace ui
{
    using namespace ftxui;

    ftxui::Component ToolbarButton(MenubarItem item, int width)
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

            auto e = text(txt);
            if (lit)
                e = e | bold;

            // Center the label in a fixed-width box using fillers on both
            // sides, so shorter labels (e.g. "RBT") sit centered rather than
            // left-aligned. Uniform width keeps a vertical stack lined up.
            auto box = hbox({filler(), e, filler()}) |
                       size(WIDTH, EQUAL, width) | border | color(fg);
            if (s.focused && on)
                box = box | bgcolor(Color::RGB(40, 40, 55));
            return box;
        };

        return Button(opt);
    }

}
