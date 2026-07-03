#include "ui/components/control/ControlView.h"

#include "core/commands/CommandExecutor.h"
#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>

namespace ui {
using namespace ftxui;

ftxui::Component ControlView::component() {
    m_picker = std::make_shared<HSVPicker>();
    m_pickerB = std::make_shared<HSVPicker>(0.f); // fan end defaults to red

    auto rgbStr = [](const Utils::Colors::RGB &c) {
        return std::to_string(c.r) + " " + std::to_string(c.g) + " " +
               std::to_string(c.b);
    };
    // "255,0,0" — for human-readable log lines (cast: RGB channels are uint8_t,
    // which std::format would otherwise print as characters).
    auto rgbLog = [](const Utils::Colors::RGB &c) {
        return std::format("{},{},{}", static_cast<int>(c.r),
                           static_cast<int>(c.g), static_cast<int>(c.b));
    };

    // Push the current color(s) to the fixtures WITHOUT the executor's per-send
    // confirmation — we log our own start→end summary instead. In solid mode a
    // single `setcolor` fills every targeted fixture; in fan mode `setfan`
    // spreads a gradient from color A to B. With no selection, both apply to all.
    auto sendQuiet = [this, rgbStr] {
        if (!m_command)
            return;
        auto *exec = m_exec ? *m_exec : nullptr;
        if (exec)
            exec->setColorEcho(false);
        if (m_mode == 1)
            m_command("setfan " + rgbStr(m_picker->rgb()) + " " +
                      rgbStr(m_pickerB->rgb()));
        else
            m_command("setcolor " + rgbStr(m_picker->rgb()));
        if (exec)
            exec->setColorEcho(true);
    };

    // One-shot log for discrete changes (mode switch, Apply button): just the
    // resulting color(s), no start value.
    auto logCurrent = [this, rgbLog](const char *verb) {
        if (m_mode == 1)
            m_log.println("{} {} → {}", verb, rgbLog(m_picker->rgb()),
                          rgbLog(m_pickerB->rgb()));
        else
            m_log.println("{} {}", verb, rgbLog(m_picker->rgb()));
    };

    // Live-apply as any slider moves; the drag's start→end is logged on release
    // (see the CatchEvent wrapper below), so no logging here.
    m_picker->onChange(sendQuiet);
    m_pickerB->onChange(sendQuiet);

    // Solid / Fan mode toggle; re-applies on switch so the output matches.
    std::vector<std::string> modes = {"Solid", "Fan"};
    auto modeOpt = RadioboxOption();
    modeOpt.on_change = [sendQuiet, logCurrent] { sendQuiet(); logCurrent("mode"); };
    auto modeBox = Radiobox(modes, &m_mode, modeOpt);

    auto applyOpt = ButtonOption::Simple();
    applyOpt.on_click = [sendQuiet, logCurrent] { sendQuiet(); logCurrent("apply"); };
    applyOpt.transform = [](const EntryState &s) {
        auto e = text(" Apply ") | border | color(Theme::okColor());
        if (s.focused)
            e = e | bgcolor(Theme::bgFocus()) | bold;
        return e;
    };
    auto applyBtn = Button(applyOpt);

    auto pickerComp = m_picker->component();
    auto pickerBComp = m_pickerB->component();
    auto layout =
        Container::Vertical({modeBox, pickerComp, pickerBComp, applyBtn});

    auto view = Renderer(layout, [this, modeBox, pickerComp, pickerBComp, applyBtn] {
        Elements pickers = {vbox({text(m_mode == 1 ? " Color A " : " Color ") |
                                      color(Theme::windowTitle()),
                                  pickerComp->Render()}) |
                            flex};
        if (m_mode == 1)
            pickers.push_back(vbox({text(" Color B ") |
                                        color(Theme::windowTitle()),
                                    pickerBComp->Render()}) |
                              flex);

        auto body = vbox({modeBox->Render(), separator(),
                          hbox(std::move(pickers)) | flex});

        return window(text(" Control ") | bold | color(Theme::windowTitle()),
                      body | flex) |
               flex;
    });

    // A slider drag is the natural start/end boundary. On mouse-down we snapshot
    // the starting color; the sliders keep sending live (quiet) as they move; on
    // release we log a single start→end line. Returns false so the sliders still
    // handle the events. Sliders are mouse-only, so this covers every drag.
    auto rgbLogM = [](const Utils::Colors::RGB &c) {
        return std::format("{},{},{}", static_cast<int>(c.r),
                           static_cast<int>(c.g), static_cast<int>(c.b));
    };
    return CatchEvent(view, [this, rgbLogM](Event e) {
        if (!e.is_mouse())
            return false;
        const auto &m = e.mouse();
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed && !m_dragging) {
            m_dragging = true;
            m_dragStartA = m_picker->rgb();
            m_dragStartB = m_pickerB->rgb();
        } else if (m.motion == Mouse::Released && m_dragging) {
            m_dragging = false;
            const auto endA = m_picker->rgb();
            const auto endB = m_pickerB->rgb();
            auto same = [](const Utils::Colors::RGB &x, const Utils::Colors::RGB &y) {
                return x.r == y.r && x.g == y.g && x.b == y.b;
            };
            const bool changed = m_mode == 1
                                     ? (!same(endA, m_dragStartA) || !same(endB, m_dragStartB))
                                     : (!same(endA, m_dragStartA));
            if (changed) {
                if (m_mode == 1)
                    m_log.println("fan {} → {} | {} → {}", rgbLogM(m_dragStartA),
                                  rgbLogM(endA), rgbLogM(m_dragStartB), rgbLogM(endB));
                else
                    m_log.println("color {} → {}", rgbLogM(m_dragStartA), rgbLogM(endA));
            }
        }
        return false;
    });
}

} // namespace ui
