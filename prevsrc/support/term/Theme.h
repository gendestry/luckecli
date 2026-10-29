#pragma once
#include "Utils/Colors/Font.h"

// ANSI (SGR) color roles for CLI / log strings built with Utils::Font.
//
// This is the engine-side half of the theme: no TUI dependency, so it can be
// used from core/ without pulling in ftxui. The ftxui color roles (same
// `Theme` namespace) live in ui/Theme.h, which includes this header.
namespace Theme {
    inline auto dim()     { return Utils::Font::colorByRGB(130, 130, 130); }
    inline auto lbl()     { return Utils::Font::colorByRGB(80, 190, 190); }
    inline auto val()     { return Utils::Font::colorByRGB(220, 190, 100); }
    inline auto name()    { return Utils::Font::colorByRGB(190, 160, 230); }
    inline auto heading() { return Utils::Font::colorByRGB(100, 150, 220); }
    inline auto ip()      { return Utils::Font::colorByRGB(140, 170, 210); }
    inline auto ver()     { return Utils::Font::colorByRGB(120, 190, 130); }
    inline auto ok()      { return Utils::Font::colorByRGB(90, 200, 110); }
    inline auto err()     { return Utils::Font::colorByRGB(210, 100, 100); }
    inline auto r()       { return Utils::Font::colorReset; }
}
