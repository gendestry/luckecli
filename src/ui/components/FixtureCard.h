#pragma once
#include <functional>
#include <string>

#include <ftxui/component/component.hpp>

namespace ui
{

    // Everything the card needs to render itself; gathered by the grid from the
    // shared-state snapshot.
    struct FixtureCardData
    {
        int index = 0;       // flat selection index (what `select <n>` expects)
        std::string name;    // fixture name
        std::string type;    // fixture type
        std::string ip;      // owning device IP
        bool online = false; // device pinged recently → green square
        bool selected = false;
    };

    // A focusable card button for one fixture: a ping square, the name, type and
    // IP, framed by a border that reflects focus/selection. `onClick` fires when
    // the card is activated.
    ftxui::Component FixtureCard(const FixtureCardData &data, std::function<void()> onClick);

}
