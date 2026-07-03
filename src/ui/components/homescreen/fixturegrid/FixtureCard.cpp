#include "ui/components/homescreen/fixturegrid/FixtureCard.h"
#include "ui/Theme.h"
#include "ui/components/homescreen/fixturegrid/PingIndicator.h"

#include <ftxui/dom/elements.hpp>

namespace ui {
using namespace ftxui;

ftxui::Component FixtureCard(const FixtureCardData &data,
                             std::function<void()> onClick) {
  auto opt = ButtonOption::Simple();
  opt.on_click = std::move(onClick);

  // Capture by value: the grid rebuilds cards on every refresh, so each
  // card's transform should keep rendering the data it was built with.
  const int index = data.index;
  const std::string name = data.name;
  const std::string type = data.type;
  const std::string ip = data.ip;
  const int universe = data.universe;
  const bool online = data.online;
  const bool selected = data.selected;

  opt.transform = [=](const EntryState &s) {
    Elements lines;
    lines.push_back(hbox({
        pingSquare(online),
        text(" "),
        text(name) | bold | color(Theme::accent()),
        filler(),
        text(std::to_string(index)) | color(Theme::faint()),
    }));
    if (!type.empty())
      lines.push_back(text(type) | color(Theme::muted()));
    lines.push_back(filler());
    lines.push_back(hbox({
        text(ip) | color(Theme::ipColor()),
        filler(),
        text("Uni: " + std::to_string(universe)) |
            color(Theme::muted()),
    }));

    auto card = vbox(std::move(lines)) | size(WIDTH, EQUAL, 28) |
                size(HEIGHT, EQUAL, 5);

    // Selection (green) is the only card highlight; the keyboard focus ring is
    // intentionally not drawn, so no card looks picked before the user acts.
    const Color border = selected ? Theme::okColor()
                                   : Theme::bgRaised();
    return card | borderStyled(ROUNDED, border);
  };

  return Button(opt);
}

} // namespace ui
