#pragma once
#include <string>
#include <utility>
#include <vector>

#include <ftxui/dom/elements.hpp>

namespace ui
{

    // The bottom bar: live counts on the left, key hints on the right. Pure
    // element, rebuilt every frame by the host with current numbers. `hints` is a
    // list of (key, description) pairs, so each view can supply its own shortcuts.
    inline ftxui::Element statusBar(int onlineFixtures, int selected,
                                    const std::vector<std::pair<std::string, std::string>> &hints)
    {
        using namespace ftxui;

        auto counts = hbox({
            text(" " + std::to_string(onlineFixtures)) | color(Color::RGB(90, 200, 110)),
            text(" online") | color(Color::RGB(130, 130, 130)),
            text("  ·  ") | color(Color::RGB(80, 80, 90)),
            text(std::to_string(selected)) | color(Color::RGB(220, 190, 100)),
            text(" selected") | color(Color::RGB(130, 130, 130)),
        });

        Elements hintEls;
        for (const auto &[key, what] : hints)
        {
            hintEls.push_back(text(key) | color(Color::RGB(190, 160, 230)));
            hintEls.push_back(text(" " + what + "  ") | color(Color::RGB(110, 110, 120)));
        }

        return hbox({counts, filler(), hbox(std::move(hintEls))}) | bgcolor(Color::RGB(24, 24, 30));
    }

}
