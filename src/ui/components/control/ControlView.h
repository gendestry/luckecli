#pragma once
#include "ui/Panel.h"

#include <functional>
#include <string>

namespace ui
{

    // A simple control surface: an RGB color picker (three sliders + a live
    // preview swatch) whose Apply button sends `setcolor r g b` through the
    // command sink to the selected fixtures (or all of them).
    class ControlView : public Panel
    {
    public:
        ControlView() = default;

        ftxui::Component component() override;
        const char *title() const override { return "Control"; }

        // The command sink; the host sets it before calling component().
        void setCommandSink(std::function<bool(const std::string &)> sink) { m_command = std::move(sink); }

    private:
        std::function<bool(const std::string &)> m_command;
        int m_r = 255, m_g = 255, m_b = 255; // slider-backed color
    };

}
