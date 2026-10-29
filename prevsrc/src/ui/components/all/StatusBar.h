#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui
{

    // What the status bar shows, re-read every frame so the counts and shortcuts
    // stay live as the selection / active view change.
    struct StatusBarModel
    {
        std::function<int()> onlineFixtures; // count shown on the left
        std::function<int()> selected;       // count shown on the left
        // Fixture names revealed when hovering the matching count. Optional.
        std::function<std::vector<std::string>()> onlineNames;
        std::function<std::vector<std::string>()> selectedNames;
        // (key, description) pairs shown on the right; each view supplies its own.
        std::function<std::vector<std::pair<std::string, std::string>>()> hints;
    };

    // The bottom bar plus its hover overlay. `component` is the interactive bar
    // (place it in the layout); `overlay` returns a floating fixture-name list to
    // be layered over everything else (empty when nothing is hovered).
    struct StatusBarComponent
    {
        ftxui::Component component;
        std::function<ftxui::Element()> overlay;
    };

    // The bottom bar: live counts on the left, key hints on the right. Hovering a
    // count reveals that group's fixture names via the overlay.
    StatusBarComponent StatusBar(StatusBarModel model);

}
