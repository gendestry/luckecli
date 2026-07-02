#include "ui/components/control/ControlView.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui
{
    using namespace ftxui;

    ftxui::Component ControlView::component()
    {
        // Push the current color to every fixture (setcolor with no selection
        // fills all of them; the handler repeats the R,G,B triplet across each
        // fixture's footprint).
        auto apply = [this]
        {
            if (m_command)
                m_command("setcolor " + std::to_string(m_r) + " " +
                          std::to_string(m_g) + " " + std::to_string(m_b));
        };

        // Sliders drive the color and apply live as you drag them, so moving any
        // channel immediately updates every fixture — no Apply step required.
        auto slider = [&](int &value)
        {
            SliderOption<int> opt;
            opt.value = &value;
            opt.min = 0;
            opt.max = 255;
            opt.increment = 1;
            opt.on_change = apply;
            return Slider(opt);
        };
        auto rs = slider(m_r);
        auto gs = slider(m_g);
        auto bs = slider(m_b);

        auto applyOpt = ButtonOption::Simple();
        applyOpt.on_click = apply;
        applyOpt.transform = [](const EntryState &s)
        {
            auto e = text(" Apply ") | border | color(Color::RGB(90, 200, 110));
            if (s.focused)
                e = e | bgcolor(Color::RGB(40, 40, 55)) | bold;
            return e;
        };
        auto applyBtn = Button(applyOpt);

        auto layout = Container::Vertical({rs, gs, bs, applyBtn});

        return Renderer(layout, [this, rs, gs, bs, applyBtn]
                        {
            auto row = [](const std::string &label, Color c, Component slider, int value)
            {
                return hbox({
                    text(" " + label + " ") | color(c) | bold,
                    slider->Render() | flex,
                    text(" " + std::to_string(value)) | color(Color::RGB(200, 200, 210)),
                });
            };

            auto swatch = text("          ") | bgcolor(Color::RGB(m_r, m_g, m_b));

            return vbox({
                       text(" Color Picker ") | bold | color(Color::RGB(100, 150, 220)),
                       separator(),
                       row("R", Color::RGB(210, 100, 100), rs, m_r),
                       row("G", Color::RGB(90, 200, 110), gs, m_g),
                       row("B", Color::RGB(140, 170, 210), bs, m_b),
                       separator(),
                       hbox({text(" preview ") | dim, swatch}),
                       separator(),
                       applyBtn->Render(),
                       filler(),
                   }) |
                   flex; });
    }

}
