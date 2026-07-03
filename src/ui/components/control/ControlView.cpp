#include "ui/components/control/ControlView.h"

#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {
using namespace ftxui;

ftxui::Component ControlView::component() {
    m_picker = std::make_shared<HSVPicker>();

    // Push the current color to every fixture (setcolor with no selection
    // fills all of them; the handler repeats the R,G,B triplet across each
    // fixture's footprint).
    auto apply = [this] {
        if (!m_command)
            return;
        Utils::Colors::RGB c = m_picker->rgb();
        m_command("setcolor " + std::to_string(c.r) + " " +
                  std::to_string(c.g) + " " + std::to_string(c.b));
    };

    // The picker applies live as you drag any slider, so moving hue,
    // saturation or value immediately updates every fixture.
    m_picker->onChange(apply);

    auto applyOpt = ButtonOption::Simple();
    applyOpt.on_click = apply;
    applyOpt.transform = [](const EntryState &s) {
        auto e = text(" Apply ") | border | color(Color::RGB(90, 200, 110));
        if (s.focused)
            e = e | bgcolor(Color::RGB(40, 40, 55)) | bold;
        return e;
    };
    auto applyBtn = Button(applyOpt);

    auto pickerComp = m_picker->component();
    auto layout = Container::Vertical({pickerComp, applyBtn});

    return Renderer(layout, [this, pickerComp, applyBtn] {
        auto swatch = text("          ") | bgcolor(m_picker->color());

        return window(text(" Control ") | bold | color(Theme::windowTitle()),
                      pickerComp->Render() | flex) |
               flex;
    });
}

} // namespace ui
