#pragma once
#include <functional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui
{

    // The editable "engine" block of the info panel: the serial- and wireless-
    // report toggles. Each commits on change by firing the matching shell command
    // through onCommand. sync() mirrors the live device state into the checkboxes
    // (which doesn't fire on_change, so a toggle always reflects a real click).
    class EngineSection
    {
    public:
        EngineSection();

        ftxui::Component component() const { return m_container; }
        ftxui::Element render() const;

        // Mirror the live device state into the toggles.
        void sync(bool serial, bool wireless, bool animation);

        void setOnCommand(std::function<void(const std::string &)> fn) { m_onCommand = std::move(fn); }

    private:
        ftxui::Component m_serialBox, m_wirelessBox, m_animationBox, m_container;
        bool m_serial = false;
        bool m_wireless = false;
        bool m_animation = false;
        std::function<void(const std::string &)> m_onCommand;
    };

}
