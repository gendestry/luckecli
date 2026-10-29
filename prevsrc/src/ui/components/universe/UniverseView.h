#pragma once
#include "ui/Panel.h"

namespace core { class SharedState; }

namespace ui
{

    // Placeholder for the DMX universe overview. Implemented later; for now it just
    // draws a stub so the view switcher has something to select.
    class UniverseView : public Panel
    {
    public:
        explicit UniverseView(core::SharedState &state);

        ftxui::Component component() override;
        const char *title() const override { return "Universe"; }

    private:
        core::SharedState &m_state;
    };

}
