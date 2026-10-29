#include "ui/components/homescreen/ToolbarButton.h"
#include "ui/Theme.h"

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
            const Color fg = !on        ? Theme::disabled()     // disabled
                             : lit       ? Theme::okColor()   // engaged
                             : s.focused ? Theme::accent()  // focused
                                         : Theme::textIdle(); // idle
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
                box = box | bgcolor(Theme::bgFocus());
            return box;
        };

        return Button(opt);
    }

}
