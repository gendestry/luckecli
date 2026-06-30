#include "ui/components/infopanel/FixtureEditor.h"
#include "ui/components/infopanel/InfoStyle.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>

namespace ui
{
    using namespace ftxui;

    FixtureEditor::FixtureEditor()
    {
        // Editable fields commit on Enter (same as clicking Apply). The cache is
        // patched by the command handler, so sync() will mirror it back.
        InputOption nameOpt;
        nameOpt.content = &m_name;
        nameOpt.multiline = false;
        nameOpt.transform = editBox;
        nameOpt.on_enter = [this]
        { applyEdits(); };
        m_nameInput = Input(nameOpt);

        // Universe / address are numeric — swallow non-digit characters so the
        // command always gets a parseable value.
        auto numericInput = [](StringRef content, std::function<void()> onEnter)
        {
            InputOption opt;
            opt.content = content;
            opt.multiline = false;
            opt.transform = editBox;
            opt.on_enter = std::move(onEnter);
            return CatchEvent(Input(opt), [](Event e)
                              {
                if (e.is_character())
                {
                    const std::string &s = e.character();
                    return s.size() == 1 && (s[0] < '0' || s[0] > '9');
                }
                return false; });
        };

        m_universeInput = numericInput(&m_universe, [this]
                                       { applyEdits(); });
        m_addressInput = numericInput(&m_address, [this]
                                      { applyEdits(); });

        m_bar = std::make_unique<ApplyRevertBar>([this]
                                                 { applyEdits(); },
                                                 [this]
                                                 { revertEdits(); },
                                                 [this]
                                                 { return dirty(); });

        m_container = Container::Vertical({
            m_nameInput,
            m_universeInput,
            m_addressInput,
            m_bar->component(),
            m_engine.component(),
        });
    }

    void FixtureEditor::setOnCommand(std::function<void(const std::string &)> fn)
    {
        m_engine.setOnCommand(fn);
        m_onCommand = std::move(fn);
    }

    bool FixtureEditor::dirty() const
    {
        return m_name != m_baseName || m_universe != m_baseUniverse || m_address != m_baseAddress;
    }

    void FixtureEditor::applyEdits()
    {
        if (!m_onCommand)
            return;
        if (!m_name.empty() && m_name != m_baseName)
            m_onCommand("setname " + m_name);
        if (!m_universe.empty() && m_universe != m_baseUniverse)
            m_onCommand("setuniverse " + m_universe);
        if (!m_address.empty() && m_address != m_baseAddress)
            m_onCommand("setaddress " + m_address);

        // Adopt the typed values; the command handler patches the cache and sync()
        // will keep mirroring it from here.
        m_baseName = m_name;
        m_baseUniverse = m_universe;
        m_baseAddress = m_address;
    }

    void FixtureEditor::revertEdits()
    {
        m_name = m_baseName;
        m_universe = m_baseUniverse;
        m_address = m_baseAddress;
    }

    void FixtureEditor::sync(const core::Client &client, const std::string &ip, int fixtureId)
    {
        // Engine toggles always mirror the live device state.
        m_engine.sync(client.engine.serial_report, client.engine.wireless_report,
                      client.engine.wifi_animation);

        // Only (re)load the editable text when the target fixture changes, so the
        // 1s refresh doesn't overwrite an edit the user is mid-way through typing.
        if (ip == m_ip && fixtureId == m_fixtureId)
            return;

        const auto &fx = client.fixtures[fixtureId];
        m_name = m_baseName = fx.name;
        m_universe = m_baseUniverse = std::to_string(fx.universe);
        m_address = m_baseAddress = std::to_string(fx.address);
        m_ip = ip;
        m_fixtureId = fixtureId;
    }

    Element FixtureEditor::render(const core::Client &client, const std::string &ip, int fixtureId) const
    {
        const auto &fx = client.fixtures[fixtureId];

        auto editRow = [](const std::string &name, const Component &input)
        { return hbox({infoLabel(name), input->Render()}); };

        Elements rows;
        rows.push_back(hbox({
            text(fx.name) | bold | color(Color::RGB(190, 160, 230)),
            text("  "),
            text(fx.type) | color(Color::RGB(130, 130, 130)),
            filler(), // push the engine version to the far right
            text(client.engine.version.empty() ? "?" : client.engine.version) |
                color(Color::RGB(130, 130, 130)),
        }));
        rows.push_back(hbox({infoLabel("  ip        "), text(ip) | color(Color::RGB(140, 170, 210))}));
        rows.push_back(editRow("  name      ", m_nameInput));
        rows.push_back(editRow("  universe  ", m_universeInput));
        rows.push_back(editRow("  address   ", m_addressInput));
        rows.push_back(hbox({infoLabel("  footprint "), infoValue(std::to_string(fx.footprint))}));

        rows.push_back(m_bar->render());

        rows.push_back(text(""));
        rows.push_back(m_engine.render());

        return vbox(std::move(rows));
    }

}
