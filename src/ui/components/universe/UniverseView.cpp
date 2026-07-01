#include "ui/components/universe/UniverseView.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui
{
    using namespace ftxui;

    UniverseView::UniverseView(core::SharedState &state) : m_state(state) {}

    ftxui::Component UniverseView::component()
    {
        return Renderer([]
                        { return text(" Universe view — coming soon ") | dim | center | flex; });
    }

}
