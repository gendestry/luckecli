#pragma once
#include <string>

#include <ftxui/dom/elements.hpp>

namespace ui
{

    // The bottom bar: live counts on the left, key hints on the right. Pure
    // element, rebuilt every frame by the view's renderer with current numbers.
    inline ftxui::Element statusBar(int onlineFixtures, int selected)
    {
        using namespace ftxui;

        auto counts = hbox({
            text(" " + std::to_string(onlineFixtures)) | color(Color::RGB(90, 200, 110)),
            text(" online") | color(Color::RGB(130, 130, 130)),
            text("  ·  ") | color(Color::RGB(80, 80, 90)),
            text(std::to_string(selected)) | color(Color::RGB(220, 190, 100)),
            text(" selected") | color(Color::RGB(130, 130, 130)),
        });

        auto hint = [](const std::string &key, const std::string &what)
        {
            return hbox({
                text(key) | color(Color::RGB(190, 160, 230)),
                text(" " + what + "  ") | color(Color::RGB(110, 110, 120)),
            });
        };

        auto hints = hbox({
            hint("click", "select"),
            hint("ctrl/shift+click", "add"),
            hint("m", "multi"),
            hint("a", "all"),
            hint("c", "clear"),
            hint("esc", "exit"),
        });

        return hbox({counts, filler(), hints}) | bgcolor(Color::RGB(24, 24, 30));
    }

}
