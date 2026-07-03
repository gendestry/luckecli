#pragma once
#include "ui/Panel.h"

#include "ui/components/util_components/HSVPicker.h"
#include "support/log/Log.h"

#include <Utils/Colors/Colors.h>

#include <functional>
#include <memory>
#include <string>

namespace core::commands { class CommandExecutor; }

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

        // The executor, so the view can mute the per-send confirmation while it
        // drives its own start→end logging. Stored as a pointer-to-pointer since
        // the host holds the executor by reference and creates it lazily.
        void setExec(core::commands::CommandExecutor *&exec) { m_exec = &exec; }

    private:
        std::function<bool(const std::string &)> m_command;
        core::commands::CommandExecutor **m_exec = nullptr;
        Log m_log{"control"};                 // routes summary lines to the terminal
        std::shared_ptr<HSVPicker> m_picker;  // color A (solid, or fan start)
        std::shared_ptr<HSVPicker> m_pickerB; // color B (fan end)
        int m_mode = 0;                       // 0 = solid, 1 = fan

        bool m_dragging = false;              // a slider drag is in progress
        Utils::Colors::RGB m_dragStartA{};    // color A when the drag began
        Utils::Colors::RGB m_dragStartB{};    // color B when the drag began
    };

}
