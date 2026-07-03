#pragma once
#include "Utils/Colors/Font.h"

#include <ftxui/screen/color.hpp>

#include <cstddef>
#include <string>

namespace Theme {
    // ── ANSI (SGR) colors, for CLI / log strings built with Utils::Font ────────
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

    // ── FTXUI colors, for TUI components ───────────────────────────────────────
    // Everything the UI draws pulls its color from here; no component should hold
    // a raw Color::RGB literal. Names are semantic (role), not by hue.

    // Foreground / text.
    inline ftxui::Color text()     { return ftxui::Color::RGB(200, 200, 210); } // primary body text
    inline ftxui::Color textIdle()  { return ftxui::Color::RGB(180, 180, 190); } // idle/secondary text
    inline ftxui::Color muted()     { return ftxui::Color::RGB(130, 130, 130); } // de-emphasised
    inline ftxui::Color faint()     { return ftxui::Color::RGB(110, 110, 120); } // very de-emphasised
    inline ftxui::Color disabled()  { return ftxui::Color::RGB(80, 80, 90); }    // greyed-out controls
    inline ftxui::Color black()     { return ftxui::Color::RGB(0, 0, 0); }       // text on light thumbs

    // Semantic accents (mirror the ANSI roles above).
    inline ftxui::Color windowTitle() { return ftxui::Color::RGB(100, 150, 220); } // == heading()
    inline ftxui::Color accent()      { return ftxui::Color::RGB(190, 160, 230); } // focus/name accent
    inline ftxui::Color okColor()     { return ftxui::Color::RGB(90, 200, 110); }
    inline ftxui::Color errColor()    { return ftxui::Color::RGB(210, 100, 100); }
    inline ftxui::Color errStrong()   { return ftxui::Color::RGB(220, 80, 80); }
    inline ftxui::Color warnColor()   { return ftxui::Color::RGB(220, 160, 90); }
    inline ftxui::Color valColor()    { return ftxui::Color::RGB(220, 190, 100); }
    inline ftxui::Color ipColor()     { return ftxui::Color::RGB(140, 170, 210); }
    inline ftxui::Color lblColor()    { return ftxui::Color::RGB(80, 190, 190); }
    inline ftxui::Color debugColor()  { return ftxui::Color::RGB(140, 140, 140); }

    // Backgrounds, dark → light.
    inline ftxui::Color bgPopup()    { return ftxui::Color::RGB(20, 20, 28); }  // modal / overlay
    inline ftxui::Color bgPanel()    { return ftxui::Color::RGB(24, 24, 30); }  // panel fill
    inline ftxui::Color bgBar()      { return ftxui::Color::RGB(30, 30, 40); }  // menubars / status bar
    inline ftxui::Color bgFocus()    { return ftxui::Color::RGB(40, 40, 55); }  // focused control
    inline ftxui::Color bgHover()    { return ftxui::Color::RGB(45, 45, 60); }  // hovered row
    inline ftxui::Color bgRaised()   { return ftxui::Color::RGB(50, 50, 60); }  // raised surface
    inline ftxui::Color bgSelected() { return ftxui::Color::RGB(60, 60, 85); }  // selected item
    inline ftxui::Color border()     { return ftxui::Color::RGB(60, 60, 70); }  // subtle separators
    inline ftxui::Color trackDim()   { return ftxui::Color::RGB(90, 90, 100); } // slider track (empty)
    inline ftxui::Color trackLit()   { return ftxui::Color::RGB(185, 185, 195); } // slider track (filled)

    // Distinct, stable color per log scope (source tag in the terminal). Hash the
    // scope name into this palette so a given source keeps the same hue.
    inline ftxui::Color scopeColor(const std::string &s) {
        static const ftxui::Color palette[] = {
            ftxui::Color::RGB(120, 180, 250),
            ftxui::Color::RGB(120, 210, 140),
            ftxui::Color::RGB(230, 180, 100),
            ftxui::Color::RGB(210, 140, 210),
            ftxui::Color::RGB(110, 210, 210),
            ftxui::Color::RGB(230, 150, 130),
        };
        std::size_t h = 0;
        for (char c : s)
            h = h * 31 + static_cast<unsigned char>(c);
        return palette[h % (sizeof(palette) / sizeof(palette[0]))];
    }
}
