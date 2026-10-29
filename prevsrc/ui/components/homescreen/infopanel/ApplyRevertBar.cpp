#include "ui/components/homescreen/infopanel/ApplyRevertBar.h"
#include "ui/Theme.h"

#include <ftxui/component/component_options.hpp>

#include <string>
#include <utility>

namespace ui
{
    using namespace ftxui;

    namespace
    {
        // A glyph-only button: no label text, just the icon, lit on focus.
        Component iconBtn(const std::string &glyph, Color c, std::function<void()> cb)
        {
            auto opt = ButtonOption::Simple();
            opt.on_click = std::move(cb);
            opt.transform = [glyph, c](const EntryState &s)
            {
                auto e = text(" " + glyph + " ") | color(c);
                if (s.focused)
                    e = e | bgcolor(Theme::bgFocus()) | bold;
                return e;
            };
            return Button(opt);
        }
    }

    ApplyRevertBar::ApplyRevertBar(std::function<void()> onApply,
                                   std::function<void()> onRevert,
                                   std::function<bool()> dirty)
        : m_dirty(std::move(dirty))
    {
        m_apply = iconBtn("✓", Theme::okColor(), std::move(onApply));   // ✓ commit
        m_revert = iconBtn("↻", Theme::warnColor(), std::move(onRevert)); // ↻ discard
        m_container = Container::Horizontal({m_apply, m_revert});
    }

    Element ApplyRevertBar::render() const
    {
        const bool dirty = m_dirty && m_dirty();
        return hbox({
            text("  "),
            m_apply->Render(),
            text("  "),
            m_revert->Render(),
            filler(),
            text(dirty ? "edited " : "") | color(Theme::warnColor()),
        });
    }

}
