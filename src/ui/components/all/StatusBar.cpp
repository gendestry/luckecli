#include "ui/components/all/StatusBar.h"
#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>
#include <utility>

namespace ui
{
    using namespace ftxui;

    namespace
    {
        // A bordered vertical list of fixture names, floated above the bar.
        Element namesList(const std::string &title,
                          const std::vector<std::string> &names)
        {
            Elements rows{text(title) | bold | color(Theme::muted())};
            if (names.empty())
                rows.push_back(text("(none)") | color(Theme::trackDim()));
            for (const auto &n : names)
                rows.push_back(text(n) | color(Theme::accent()));
            return vbox(std::move(rows)) | border |
                   bgcolor(Theme::bgBar());
        }
    } // namespace

    StatusBarComponent StatusBar(StatusBarModel model)
    {
        // Hover flags kept alive for the lifetime of the returned component.
        auto onlineHover = std::make_shared<bool>(false);
        auto selectedHover = std::make_shared<bool>(false);

        // The two hoverable count segments on the left.
        auto onlineSeg = Renderer([model]
                                  {
            const int online = model.onlineFixtures ? model.onlineFixtures() : 0;
            return hbox({
                text(" " + std::to_string(online)) | color(Theme::okColor()),
                text(" online") | color(Theme::muted()),
            }); });

        auto selectedSeg = Renderer([model]
                                    {
            const int selected = model.selected ? model.selected() : 0;
            return hbox({
                text(std::to_string(selected)) | color(Theme::valColor()),
                text(" selected") | color(Theme::muted()),
            }); });

        onlineSeg = Hoverable(onlineSeg, onlineHover.get());
        selectedSeg = Hoverable(selectedSeg, selectedHover.get());

        auto container = Container::Horizontal({onlineSeg, selectedSeg});

        auto bar = Renderer(container, [=]
                            {
            Elements hintEls;
            if (model.hints)
                for (const auto &[key, what] : model.hints())
                {
                    hintEls.push_back(text(key) | color(Theme::accent()));
                    hintEls.push_back(text(" " + what + "  ") | color(Theme::faint()));
                }

            return hbox({
                       onlineSeg->Render(),
                       text("  ·  ") | color(Theme::disabled()),
                       selectedSeg->Render(),
                       filler(),
                       hbox(std::move(hintEls)),
                   }) |
                   bgcolor(Theme::bgPanel()); });

        // Floating name list, layered over everything and anchored bottom-left,
        // one row above the bar. Empty when nothing is hovered.
        auto overlay = [=]() -> Element
        {
            Element list;
            if (*onlineHover && model.onlineNames)
                list = namesList("online", model.onlineNames());
            else if (*selectedHover && model.selectedNames)
                list = namesList("selected", model.selectedNames());
            else
                return filler(); // nothing hovered: transparent layer

            return vbox({
                filler(),
                hbox({std::move(list), filler()}),
                text(" "), // spacer so the list sits above the 1-row bar
            });
        };

        return {bar, overlay};
    }

}
