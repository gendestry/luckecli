#include "ui/components/terminal/TerminalView.h"
#include "ui/View.h" // stripAnsi
#include "core/commands/CommandExecutor.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <thread>
#include <utility>

namespace ui
{
    using namespace ftxui;

    // Keep the scrollback bounded so a long session doesn't grow without limit.
    static constexpr std::size_t kMaxLines = 2000;

    TerminalView::TerminalView(core::commands::CommandExecutor *&exec) : m_exec(exec) {}

    void TerminalView::pushLine(Loggable::Type type, std::string text)
    {
        std::lock_guard lock(m_mutex);
        m_lines.push_back({type, std::move(text)});
        if (m_lines.size() > kMaxLines)
            m_lines.erase(m_lines.begin(), m_lines.begin() + (m_lines.size() - kMaxLines));
    }

    void TerminalView::onLog(const Loggable &entry)
    {
        // Strip ANSI and split on newlines so each line renders on its own row.
        std::string msg = stripAnsi(entry.message);
        std::size_t start = 0;
        while (start <= msg.size())
        {
            std::size_t nl = msg.find('\n', start);
            std::string piece = msg.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!piece.empty())
                pushLine(entry.type, std::move(piece));
            if (nl == std::string::npos)
                break;
            start = nl + 1;
        }
    }

    ftxui::Component TerminalView::component()
    {
        // Bottom input line: echo + run the command on Enter, then clear it. The
        // command runs on a detached thread so blocking network I/O never freezes
        // the FTXUI loop; the reply arrives asynchronously through onLog().
        InputOption opt;
        opt.content = &m_input;
        opt.placeholder = "type a command…";
        opt.multiline = false;
        opt.on_enter = [this]
        {
            if (m_input.empty())
                return;
            std::string cmd = m_input;
            m_input.clear();
            pushLine(Loggable::PRINT, "> " + cmd);
            if (m_command)
                std::thread([this, cmd = std::move(cmd)]()
                            { m_command(cmd); })
                    .detach();
        };
        auto input = Input(opt);

        // Renderer delegates events to `input` (so it can type/submit) while
        // drawing the scrollback above it. focusPositionRelative(0,1) keeps the
        // newest lines pinned to the bottom of the scroll frame.
        return Renderer(input, [this, input]
                        {
            Elements lines;
            {
                std::lock_guard lock(m_mutex);
                for (const auto &l : m_lines)
                {
                    Color c = l.type == Loggable::ERROR ? Color::RGB(210, 100, 100)
                              : l.type == Loggable::DEBUG ? Color::RGB(140, 140, 140)
                                                          : Color::RGB(200, 200, 210);
                    lines.push_back(text(l.text) | color(c));
                }
            }
            if (lines.empty())
                lines.push_back(text(" no output yet") | dim);

            auto history = vbox(std::move(lines)) |
                           focusPositionRelative(0.f, 1.f) | yframe | vscroll_indicator | flex;

            return vbox({
                       window(text(" Terminal ") | bold | color(Color::RGB(100, 150, 220)),
                              history | flex) |
                           flex,
                       hbox({
                           text(" > ") | bold | color(Color::RGB(100, 150, 220)),
                           input->Render() | flex,
                       }),
                   }) |
                   flex; });
    }

}
