#pragma once
#include <functional>
#include <memory>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/domain/Client.h"
#include "ui/components/infopanel/ApplyRevertBar.h"
#include "ui/components/infopanel/EngineSection.h"

namespace ui
{

    // The editable detail view for a single fixture: name / universe / address
    // inputs, an Apply/Revert bar, and the engine toggles. Edits stay local until
    // Apply commits them (or Enter in any field), at which point the changed fields
    // are sent as shell commands and the typed values become the new baseline;
    // Revert restores the baseline.
    //
    // The ftxui inputs are built once over member-backed storage; sync() refreshes
    // that storage from the current fixture (UI thread only), taking care not to
    // clobber a half-typed edit on the same target.
    class FixtureEditor
    {
    public:
        FixtureEditor();

        ftxui::Component component() const { return m_container; }

        // Reload the editable fields from `client.fixtures[fixtureId]` (only when
        // the target changes) and mirror the live engine state every frame.
        void sync(const core::Client &client, const std::string &ip, int fixtureId);

        // Render the form for `client.fixtures[fixtureId]`.
        ftxui::Element render(const core::Client &client, const std::string &ip, int fixtureId) const;

        void setOnCommand(std::function<void(const std::string &)> fn);

    private:
        void applyEdits();
        void revertEdits();
        bool dirty() const;

        ftxui::Component m_nameInput, m_universeInput, m_addressInput, m_container;
        std::unique_ptr<ApplyRevertBar> m_bar;
        EngineSection m_engine;

        // Backing storage referenced by the inputs (UI thread only).
        std::string m_name, m_universe, m_address;
        // Last-known committed values, used to detect edits and to revert to.
        std::string m_baseName, m_baseUniverse, m_baseAddress;

        // The fixture the fields currently mirror, so the 1s refresh doesn't
        // overwrite a half-typed edit on the same target.
        std::string m_ip;
        int m_fixtureId = -1;

        std::function<void(const std::string &)> m_onCommand;
    };

}
