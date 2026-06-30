#include "ui/components/infopanel/EngineSection.h"
#include "ui/components/infopanel/InfoStyle.h"

#include <ftxui/component/component_options.hpp>

namespace ui
{
    using namespace ftxui;

    EngineSection::EngineSection()
    {
        CheckboxOption serialOpt;
        serialOpt.label = "serial report";
        serialOpt.checked = &m_serial;
        serialOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_serial ? "serialprint on" : "serialprint off"); };
        m_serialBox = Checkbox(serialOpt);

        CheckboxOption wirelessOpt;
        wirelessOpt.label = "wireless report";
        wirelessOpt.checked = &m_wireless;
        wirelessOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_wireless ? "wirelessprint on" : "wirelessprint off"); };
        m_wirelessBox = Checkbox(wirelessOpt);

        CheckboxOption animationOpt;
        animationOpt.label = "wifi animation";
        animationOpt.checked = &m_animation;
        animationOpt.on_change = [this]
        { if (m_onCommand) m_onCommand(m_animation ? "wifianimation on" : "wifianimation off"); };
        m_animationBox = Checkbox(animationOpt);

        m_container = Container::Vertical({m_serialBox, m_wirelessBox, m_animationBox});
    }

    void EngineSection::sync(bool serial, bool wireless, bool animation)
    {
        m_serial = serial;
        m_wireless = wireless;
        m_animation = animation;
    }

    Element EngineSection::render() const
    {
        return vbox({
            infoLabel("  engine"),
            hbox({text("  "), m_serialBox->Render()}),
            hbox({text("  "), m_wirelessBox->Render()}),
            hbox({text("  "), m_animationBox->Render()}),
        });
    }

}
