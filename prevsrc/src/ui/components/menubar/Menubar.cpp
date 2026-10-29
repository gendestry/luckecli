#include "ui/components/menubar/Menubar.h"
#include "ui/Theme.h"

#include <ftxui/dom/elements.hpp>

#include <utility>

namespace ui
{
    using namespace ftxui;

    ftxui::Component Menubar(std::vector<MenubarItem> items)
    {
        Components buttons;
        buttons.reserve(items.size());

        for (auto &item : items)
        {
            const std::string label = item.label;
            auto enabled = item.enabled;       // copied into the closures below
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
                const bool lit = active && active(); // toggle currently engaged
                const Color fg = !on        ? Theme::disabled()     // disabled
                                 : lit       ? Theme::okColor()   // toggled on
                                 : s.focused ? Theme::accent()  // focused
                                             : Theme::textIdle(); // idle
                const std::string txt = labelFn ? labelFn() : label;
                auto e = text(" " + std::string(lit ? "● " : "") + txt + " ") | color(fg);
                if (lit)
                    e = e | bold;
                if (s.focused && on)
                    e = e | bgcolor(Theme::bgFocus());
                return e;
            };

            buttons.push_back(Button(opt));
        }

        return Container::Horizontal(std::move(buttons));
    }

}
