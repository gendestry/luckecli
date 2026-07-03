#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "ui/Theme.h"

namespace ui
{

    // A filled square colored by liveness: green when the device has pinged
    // recently, red when it's gone quiet.
    inline ftxui::Element pingSquare(bool online)
    {
        using namespace ftxui;
        return text("■") | color(online ? Theme::okColor()
                                         : Theme::errColor());
    }

}
