#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/commands/CommandExecutor.h"

namespace core { class SharedState; }

namespace ui
{


    // Detail panel for the current selection.
    //
    // Single fixture: editable fields (name / universe / address), the preset
    // picker, and the engine / wifi sections — all grouped under collapsible
    // section headers. Read-only values (type, ip, footprint, version) are read
    // live each frame. Edits commit through onCommand.
    //
    // Multiple fixtures: a read-only listing where each fixture is a collapsible
    // block (click its arrow to fold/unfold) so a big selection stays scannable.
    //
    // The ftxui inputs/checkboxes are built once over member-backed storage;
    // sync() refreshes that storage from the current selection (UI thread only),
    // taking care not to clobber a half-typed edit on the same fixture.
    class InfoPanel
    {
    public:
        InfoPanel(core::SharedState &state, core::commands::CommandExecutor *&exec);

        // The component to mount in the layout tree (gate its focus with focusable()).
        ftxui::Component component() const { return m_container; }

        // Refresh the editable fields / multi-fixture headers from the selection.
        void sync();

        // True when the single-fixture editable form is shown.
        bool active() const { return m_active; }

        // True when the panel has interactive content (single form or the multi
        // listing's fold headers) and should join the focus/event tree.
        bool focusable() const { return m_active || m_multi; }

        // The info section element (editable form, collapsible listing, or empty).
        ftxui::Element render() const;

        // Scroll the panel content. Positive scrolls down; clamped to [0,1].
        void scroll(int delta);

        // Runs a shell-style command string (same sink as the REPL).
        void setOnCommand(std::function<void(const std::string &)> fn) { m_onCommand = std::move(fn); }

        // Embed the preset picker inside the Fixture section: its focusable
        // component is spliced into the fixture sub-tree, and its element is drawn
        // among the fixture fields. `active` gates whether it shows at all.
        void setPresetSection(ftxui::Component comp,
                              std::function<ftxui::Element()> render,
                              std::function<bool()> active);

    private:
        core::SharedState &m_state;
        core::commands::CommandExecutor *&m_exec;

        ftxui::Component m_container;       // top level: single form OR multi listing
        ftxui::Component m_singleContainer; // the single-fixture editable form
        ftxui::Component m_multiContainer;  // holds one fold-header button per selected fixture
        ftxui::Component m_nameInput, m_universeInput, m_addressInput;
        ftxui::Component m_serialBox, m_wirelessBox, m_animationBox;
        ftxui::Component m_applyBtn, m_revertBtn;
        ftxui::Component m_ssidInput, m_passwordInput, m_wifiApplyBtn, m_wifiRevertBtn;

        // Collapsible-section headers (toggle the matching open flag on click/Enter).
        ftxui::Component m_fixtureHdr, m_engineHdr, m_wifiHdr;

        // The preset picker embedded in the Fixture section (set by the host).
        ftxui::Component m_presetComp;
        std::function<ftxui::Element()> m_presetRender;
        std::function<bool()> m_presetActive;

        // (Re)assemble m_container from the parts; called once the preset section
        // is known so the fixture sub-tree includes the preset component.
        void buildContainer();

        // Rebuild the per-fixture fold-header buttons when the multi-selection
        // identity changes (so each block can be collapsed independently).
        void rebuildMulti(const std::vector<core::commands::CommandExecutor::Selection> &sel);
        static std::string keyOf(const std::string &ip, int id);

        // Send any edited fields (name/universe/address) as commands, then adopt
        // the typed values as the new baseline. Reset the fields to the baseline.
        void applyEdits();
        void revertEdits();
        // WiFi credential edits (ssid/password) — committed via `setwifi`.
        void applyWifi();
        void revertWifi();

        // Backing storage referenced by the components (UI thread only).
        std::string m_name, m_universe, m_address;
        // Last-known committed values, used to detect edits and to revert to.
        std::string m_baseName, m_baseUniverse, m_baseAddress;
        std::string m_ssid, m_password, m_baseSsid, m_basePassword;

        // Per-section fold state. Fixture/engine open by default; wifi collapsed.
        bool m_openFixture = true;
        bool m_openEngine = true;
        bool m_openWifi = false;

        // Relative scroll position of the panel content, in [0,1].
        float m_scroll = 0.f;
        bool m_serial = false;
        bool m_wireless = false;
        bool m_animation = false;

        // The fixture the editable fields currently mirror. Used so the 1s refresh
        // doesn't overwrite a half-typed edit on the same target.
        std::string m_ip;
        int m_fixtureId = -1;

        bool m_active = false; // single selection on a live fixture
        bool m_multi = false;  // two or more fixtures selected (collapsible listing)

        // Multi-selection state: the fold-header buttons (parallel to the current
        // selection) and the per-fixture collapsed flags (keyed by ip#id, sticky
        // across refreshes).
        std::vector<ftxui::Component> m_multiHeaders;
        std::vector<std::string> m_multiKeys; // identity of the selection m_multiHeaders was built for
        std::unordered_map<std::string, bool> m_collapsed;

        std::function<void(const std::string &)> m_onCommand;
    };

    // Read-only fixture listing for the zero-selection fallback. Read live from
    // the cached client state.
    ftxui::Element infoListing(core::SharedState &state,
                               const std::vector<core::commands::CommandExecutor::Selection> &selection);

}
