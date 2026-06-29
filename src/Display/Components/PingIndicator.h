#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

namespace Test
{

    // A filled square colored by liveness: green when the device has pinged
    // recently, red when it's gone quiet.
    inline ftxui::Element pingSquare(bool online)
    {
        using namespace ftxui;
        return text("■") | color(online ? Color::RGB(90, 200, 110)
                                         : Color::RGB(210, 100, 100));
    }

}
