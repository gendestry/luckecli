#include "ui/components/all/StatusBar.h"

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
            Elements rows{text(title) | bold | color(Color::RGB(130, 130, 130))};
            if (names.empty())
                rows.push_back(text("(none)") | color(Color::RGB(90, 90, 100)));
            for (const auto &n : names)
                rows.push_back(text(n) | color(Color::RGB(190, 160, 230)));
            return vbox(std::move(rows)) | border |
                   bgcolor(Color::RGB(30, 30, 40));
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
                text(" " + std::to_string(online)) | color(Color::RGB(90, 200, 110)),
                text(" online") | color(Color::RGB(130, 130, 130)),
            }); });

        auto selectedSeg = Renderer([model]
                                    {
            const int selected = model.selected ? model.selected() : 0;
            return hbox({
                text(std::to_string(selected)) | color(Color::RGB(220, 190, 100)),
                text(" selected") | color(Color::RGB(130, 130, 130)),
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
                    hintEls.push_back(text(key) | color(Color::RGB(190, 160, 230)));
                    hintEls.push_back(text(" " + what + "  ") | color(Color::RGB(110, 110, 120)));
                }

            return hbox({
                       onlineSeg->Render(),
                       text("  ·  ") | color(Color::RGB(80, 80, 90)),
                       selectedSeg->Render(),
                       filler(),
                       hbox(std::move(hintEls)),
                   }) |
                   bgcolor(Color::RGB(24, 24, 30)); });

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
