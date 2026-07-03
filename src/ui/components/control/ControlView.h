#pragma once
#include "ui/Panel.h"

#include "ui/components/util_components/HSVPicker.h"

#include <functional>
#include <memory>
#include <string>

namespace ui
{

    // A simple control surface: an HSV color picker (hue/saturation/value
    // gradient sliders + a live preview swatch) whose Apply button sends
    // `setcolor r g b` through the command sink to the selected fixtures (or
    // all of them).
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
        std::shared_ptr<HSVPicker> m_picker; // HSV color state + sliders
    };

}
