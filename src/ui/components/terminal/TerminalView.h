#pragma once
#include "ui/Panel.h"
#include "support/log/Loggable.h"

#include <ftxui/screen/box.hpp>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace core::commands { class CommandExecutor; }

namespace ui
{

    // A CLI-style view: a scrollable scrollback on top and a single input line at
    // the bottom. Typed commands are echoed, run off-thread (so the UI stays
    // responsive), and their replies — routed here from the log system via
    // onLog() — appear in the scrollback.
    class TerminalView : public Panel
    {
    public:
        explicit TerminalView(core::commands::CommandExecutor *&exec);

        ftxui::Component component() override;
        const char *title() const override { return "Terminal"; }

        // The command sink; the host sets it before calling component().
        void setCommandSink(std::function<bool(const std::string &)> sink) { m_command = std::move(sink); }

        // Append a log entry to the scrollback. Called from arbitrary threads (the
        // log callback), so it is mutex-guarded.
        void onLog(const Loggable &entry);

    private:
        struct Line
        {
            Loggable::Type type;
            std::string text;
        };

        void pushLine(Loggable::Type type, std::string text);

        core::commands::CommandExecutor *&m_exec;
        std::function<bool(const std::string &)> m_command;
        std::string m_input; // backing storage for the input line

        mutable std::mutex m_mutex; // guards m_lines
        std::vector<Line> m_lines;

        // Scrollback offset in lines, counted from the bottom. 0 pins the view to
        // the newest line (and auto-follows new output); >0 scrolls up into history.
        int m_scroll = 0;

        // Last known mouse position (screen coords) and the per-line boxes captured
        // during the previous render, used to highlight the hovered scrollback line.
        int m_mouseX = -1, m_mouseY = -1;
        std::vector<ftxui::Box> m_lineBoxes;
    };

}
