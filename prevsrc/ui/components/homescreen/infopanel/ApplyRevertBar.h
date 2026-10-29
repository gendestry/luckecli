#pragma once
#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui
{

    // The Apply (✓) / Revert (↻) glyph buttons shown under the editable fields.
    // Apply commits the pending edits; Revert discards them. The `dirty` predicate
    // drives the "edited" marker so the bar can tell the user changes are pending.
    class ApplyRevertBar
    {
    public:
        ApplyRevertBar(std::function<void()> onApply,
                       std::function<void()> onRevert,
                       std::function<bool()> dirty);

        // Focusable component (two buttons) to mount in the layout tree.
        ftxui::Component component() const { return m_container; }

        // The rendered row: both buttons plus the "edited" marker when dirty.
        ftxui::Element render() const;

    private:
        ftxui::Component m_apply, m_revert, m_container;
        std::function<bool()> m_dirty;
    };

}
