#pragma once
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/domain/Client.h"
#include "ui/Theme.h"

// Small rendering helpers shared by the Info panel's sub-components (the editor,
// the engine section, and the read-only listing). Kept header-only since they're
// trivial and used across several translation units.
namespace ui
{
    inline ftxui::Element infoLabel(const std::string &s)
    {
        return ftxui::text(s) | ftxui::color(Theme::lblColor());
    }

    inline ftxui::Element infoValue(const std::string &s)
    {
        return ftxui::text(s) | ftxui::color(Theme::valColor());
    }

    // The live client for `ip` in a fresh snapshot, or nullptr if it's offline.
    inline const core::Client *findClient(const std::vector<std::pair<std::string, core::Client>> &clients,
                                    const std::string &ip)
    {
        for (const auto &[cip, c] : clients)
            if (cip == ip)
                return &c;
        return nullptr;
    }

    // Style an editable field: a subtle filled box that brightens on focus.
    inline ftxui::Element editBox(ftxui::InputState s)
    {
        using namespace ftxui;
        auto e = s.element;
        if (s.is_placeholder)
            e = e | dim;
        e = e | color(Theme::valColor());
        e = e | bgcolor(s.focused ? Theme::bgSelected() : Theme::bgFocus());
        return e | size(WIDTH, GREATER_THAN, 10);
    }
}
