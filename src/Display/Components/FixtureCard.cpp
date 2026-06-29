#include "FixtureCard.h"
#include "PingIndicator.h"

#include <ftxui/dom/elements.hpp>

namespace Test
{
    using namespace ftxui;

    ftxui::Component FixtureCard(const FixtureCardData &data, std::function<void()> onClick)
    {
        auto opt = ButtonOption::Simple();
        opt.on_click = std::move(onClick);

        // Capture by value: the grid rebuilds cards on every refresh, so each
        // card's transform should keep rendering the data it was built with.
        const int index = data.index;
        const std::string name = data.name;
        const std::string type = data.type;
        const std::string ip = data.ip;
        const bool online = data.online;
        const bool selected = data.selected;

        opt.transform = [=](const EntryState &s)
        {
            Elements lines;
            lines.push_back(hbox({
                pingSquare(online),
                text(" "),
                text(name) | bold | color(Color::RGB(190, 160, 230)),
                filler(),
                text(std::to_string(index)) | color(Color::RGB(100, 100, 100)),
            }));
            if (!type.empty())
                lines.push_back(text(type) | color(Color::RGB(130, 130, 130)));
            lines.push_back(text(ip) | color(Color::RGB(140, 170, 210)));

            auto card = vbox(std::move(lines)) | size(WIDTH, EQUAL, 28) | size(HEIGHT, EQUAL, 5);

            const Color border = s.focused ? Color::RGB(190, 160, 230)
                                 : selected ? Color::RGB(90, 200, 110)
                                            : Color::RGB(50, 50, 60);
            return card | borderStyled(ROUNDED, border);
        };

        return Button(opt);
    }

}
