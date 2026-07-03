#include "ui/components/terminal/TerminalView.h"
#include "core/commands/CommandExecutor.h"
#include "ui/Theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <thread>
#include <utility>
#include <vector>

namespace ui
{
    using namespace ftxui;

    // Keep the scrollback bounded so a long session doesn't grow without limit.
    static constexpr std::size_t kMaxLines = 2000;

    // Render one log line, translating the theme's ANSI SGR codes (RGB colors,
    // bold/italic/dim, resets) into styled FTXUI segments so the terminal matches
    // the CLI's coloring. `def` is the fallback color when no color is set / reset.
    static Element parseAnsiLine(const std::string &s, Color def)
    {
        Elements segs;
        Color fg = def;
        bool bold_ = false, italic_ = false, dim_ = false;
        std::string cur;

        auto flush = [&]()
        {
            if (cur.empty())
                return;
            auto e = text(cur) | color(fg);
            if (bold_)
                e = e | bold;
            if (italic_)
                e = e | italic;
            if (dim_)
                e = e | dim;
            segs.push_back(e);
            cur.clear();
        };

        for (std::size_t i = 0; i < s.size();)
        {
            if (s[i] == '\x1B' && i + 1 < s.size() && s[i + 1] == '[')
            {
                flush();
                std::size_t j = i + 2;
                std::vector<int> params;
                int num = 0;
                bool has = false;
                while (j < s.size() && s[j] != 'm' &&
                       ((s[j] >= '0' && s[j] <= '9') || s[j] == ';'))
                {
                    if (s[j] == ';') { params.push_back(num); num = 0; has = false; }
                    else { num = num * 10 + (s[j] - '0'); has = true; }
                    ++j;
                }
                if (has)
                    params.push_back(num);
                if (params.empty())
                    params.push_back(0); // "\x1B[m" is a reset

                if (j < s.size() && s[j] == 'm')
                {
                    for (std::size_t k = 0; k < params.size(); ++k)
                    {
                        const int p = params[k];
                        if (p == 0) { fg = def; bold_ = italic_ = dim_ = false; }
                        else if (p == 1) bold_ = true;
                        else if (p == 2) dim_ = true;
                        else if (p == 3) italic_ = true;
                        else if (p == 22) { bold_ = dim_ = false; }
                        else if (p == 23) italic_ = false;
                        else if (p == 30) fg = Color::RGB(0, 0, 0);
                        else if (p == 31) fg = Color::RGB(210, 100, 100);
                        else if (p == 32) fg = Color::RGB(90, 200, 110);
                        else if (p == 33) fg = Color::RGB(220, 190, 100);
                        else if (p == 34) fg = Color::RGB(140, 170, 210);
                        else if (p == 35) fg = Color::RGB(190, 160, 230);
                        else if (p == 39) fg = def;
                        else if ((p == 38 || p == 48) && k + 4 < params.size() && params[k + 1] == 2)
                        {
                            Color c = Color::RGB(params[k + 2], params[k + 3], params[k + 4]);
                            if (p == 38)
                                fg = c; // background (48) is ignored in the scrollback
                            k += 4;
                        }
                        else if ((p == 38 || p == 48) && k + 2 < params.size() && params[k + 1] == 5)
                            k += 2; // 256-color palette: unsupported, skip
                    }
                    i = j + 1;
                    continue;
                }
                // Not an SGR we recognise — drop the escape and keep going.
                i = (j < s.size()) ? j + 1 : s.size();
                continue;
            }
            cur += s[i++];
        }
        flush();
        if (segs.empty())
            segs.push_back(text(""));
        return hbox(std::move(segs));
    }

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
        // Keep the ANSI intact (it carries the theme colors) and split on newlines
        // so each line renders on its own row; parsing happens at render time.
        const std::string &msg = entry.message;
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
                    Color def = l.type == Loggable::ERROR ? Color::RGB(210, 100, 100)
                                : l.type == Loggable::DEBUG ? Color::RGB(140, 140, 140)
                                                            : Color::RGB(200, 200, 210);
                    lines.push_back(parseAnsiLine(l.text, def));
                }
            }
            if (lines.empty())
                lines.push_back(text(" no output yet") | dim);

            auto history = vbox(std::move(lines)) |
                           focusPositionRelative(0.f, 1.f) | yframe | vscroll_indicator | flex;

            return vbox({
                       window(text(" Terminal ") | bold | color(Theme::windowTitle()),
                              history | flex) |
                           flex,
                       hbox({
                           text(" > ") | bold | color(Theme::windowTitle()),
                           input->Render() | flex,
                       }),
                   }) |
                   flex; });
    }

}
