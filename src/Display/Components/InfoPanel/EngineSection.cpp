#include "Display/Components/InfoPanel/EngineSection.h"
#include "Display/Components/InfoPanel/InfoStyle.h"

#include <ftxui/component/component_options.hpp>

namespace Test
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

        m_container = Container::Vertical({m_serialBox, m_wirelessBox});
    }

    void EngineSection::sync(bool serial, bool wireless)
    {
        m_serial = serial;
        m_wireless = wireless;
    }

    Element EngineSection::render() const
    {
        return vbox({
            infoLabel("  engine"),
            hbox({text("  "), m_serialBox->Render()}),
            hbox({text("  "), m_wirelessBox->Render()}),
        });
    }

}
