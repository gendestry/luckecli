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
        // A dimmed tooltip line listing fixture names, shown above the bar while
        // a count is hovered.
        Element namesTooltip(const std::string &title,
                             const std::vector<std::string> &names)
        {
            Elements els{text(" " + title + ": ") | color(Color::RGB(130, 130, 130))};
            if (names.empty())
                els.push_back(text("(none)") | color(Color::RGB(90, 90, 100)));
            for (std::size_t i = 0; i < names.size(); ++i)
            {
                if (i)
                    els.push_back(text(", ") | color(Color::RGB(80, 80, 90)));
                els.push_back(text(names[i]) | color(Color::RGB(190, 160, 230)));
            }
            return hbox(std::move(els)) | bgcolor(Color::RGB(30, 30, 40));
        }
    } // namespace

    ftxui::Component StatusBar(StatusBarModel model)
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

        return Renderer(container, [=]
                        {
            Elements hintEls;
            if (model.hints)
                for (const auto &[key, what] : model.hints())
                {
                    hintEls.push_back(text(key) | color(Color::RGB(190, 160, 230)));
                    hintEls.push_back(text(" " + what + "  ") | color(Color::RGB(110, 110, 120)));
                }

            auto bar = hbox({
                           onlineSeg->Render(),
                           text("  ·  ") | color(Color::RGB(80, 80, 90)),
                           selectedSeg->Render(),
                           filler(),
                           hbox(std::move(hintEls)),
                       }) |
                       bgcolor(Color::RGB(24, 24, 30));

            // A hovered count reveals its fixture names in a tooltip above the bar.
            if (*onlineHover && model.onlineNames)
                return vbox({namesTooltip("online", model.onlineNames()), bar});
            if (*selectedHover && model.selectedNames)
                return vbox({namesTooltip("selected", model.selectedNames()), bar});
            return bar; });
    }

}
