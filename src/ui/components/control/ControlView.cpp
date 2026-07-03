#include "ui/components/control/ControlView.h"

#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {
using namespace ftxui;

ftxui::Component ControlView::component() {
    m_picker = std::make_shared<HSVPicker>();
    m_pickerB = std::make_shared<HSVPicker>(0.f); // fan end defaults to red

    auto rgbStr = [](const Utils::Colors::RGB &c) {
        return std::to_string(c.r) + " " + std::to_string(c.g) + " " +
               std::to_string(c.b);
    };

    // Push the current color(s) to the fixtures. In solid mode a single
    // `setcolor` fills every targeted fixture; in fan mode `setfan` spreads a
    // gradient from color A to color B across them. With no selection, both
    // apply to all fixtures.
    auto apply = [this, rgbStr] {
        if (!m_command)
            return;
        if (m_mode == 1)
            m_command("setfan " + rgbStr(m_picker->rgb()) + " " +
                      rgbStr(m_pickerB->rgb()));
        else
            m_command("setcolor " + rgbStr(m_picker->rgb()));
    };

    // Live-apply as any slider moves.
    m_picker->onChange(apply);
    m_pickerB->onChange(apply);

    // Solid / Fan mode toggle; re-applies on switch so the output matches.
    std::vector<std::string> modes = {"Solid", "Fan"};
    auto modeOpt = RadioboxOption();
    modeOpt.on_change = apply;
    auto modeBox = Radiobox(modes, &m_mode, modeOpt);

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
    auto pickerBComp = m_pickerB->component();
    auto layout =
        Container::Vertical({modeBox, pickerComp, pickerBComp, applyBtn});

    return Renderer(layout, [this, modeBox, pickerComp, pickerBComp, applyBtn] {
        Elements pickers = {vbox({text(m_mode == 1 ? " Color A " : " Color ") |
                                      color(Theme::windowTitle()),
                                  pickerComp->Render()}) |
                            flex};
        if (m_mode == 1)
            pickers.push_back(vbox({text(" Color B ") |
                                        color(Theme::windowTitle()),
                                    pickerBComp->Render()}) |
                              flex);

        auto body = vbox({modeBox->Render(), separator(),
                          hbox(std::move(pickers)) | flex});

        return window(text(" Control ") | bold | color(Theme::windowTitle()),
                      body | flex) |
               flex;
    });
}

} // namespace ui
