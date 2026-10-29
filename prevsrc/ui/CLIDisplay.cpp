#include "ui/CLIDisplay.h"
#include "core/commands/CommandExecutor.h"
#include "support/log/Loggable.h"

#include <iostream>
#include <string>
#include <vector>

#include <termios.h>
#include <unistd.h>

namespace ui
{
    // Read one line with up/down history navigation and backspace. The terminal
    // is in raw mode (char-by-char, no echo) but keeps output post-processing, so
    // the cursor stays at the end of the line — no left/right editing yet, which
    // means the (coloured) prompt needs no width accounting. Returns false on EOF
    // (Ctrl-D on an empty line / closed stdin).
    bool CLIDisplay::readLine(std::string &out, const std::vector<std::string> &history)
    {
        std::string buffer;
        std::size_t histIdx = history.size(); // index of the "new" (not-yet-saved) line
        std::string pending;                  // the in-progress line, saved while browsing

        std::cout << m_prompt << std::flush;

        auto redraw = [&]
        { std::cout << "\r\x1b[K" << m_prompt << buffer << std::flush; };

        for (;;)
        {
            char c;
            if (::read(STDIN_FILENO, &c, 1) <= 0)
                return false; // stdin closed
            if (c == '\r' || c == '\n')
            {
                std::cout << "\n"
                          << std::flush;
                out = buffer;
                return true;
            }
            if (c == 127 || c == 8) // backspace / delete
            {
                if (!buffer.empty())
                {
                    buffer.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else if (c == 4) // Ctrl-D
            {
                if (buffer.empty())
                    return false;
            }
            else if (c == 27) // escape sequence (arrow keys)
            {
                char seq[2];
                if (::read(STDIN_FILENO, &seq[0], 1) != 1 || ::read(STDIN_FILENO, &seq[1], 1) != 1)
                    continue;
                if (seq[0] != '[')
                    continue;
                if (seq[1] == 'A') // up — older
                {
                    if (histIdx > 0)
                    {
                        if (histIdx == history.size())
                            pending = buffer;
                        buffer = history[--histIdx];
                        redraw();
                    }
                }
                else if (seq[1] == 'B') // down — newer
                {
                    if (histIdx < history.size())
                    {
                        ++histIdx;
                        buffer = (histIdx == history.size()) ? pending : history[histIdx];
                        redraw();
                    }
                }
                // left/right ('C'/'D') and other sequences: ignored for now.
            }
            else if (static_cast<unsigned char>(c) >= 32) // printable
            {
                buffer += c;
                std::cout << c << std::flush;
            }
        }
    }

    void CLIDisplay::run(std::function<bool(const std::string &)> onCommand)
    {
        // Non-interactive input (piped/redirected): plain getline, no editing.
        if (!isatty(STDIN_FILENO))
        {
            std::cout << m_prompt << std::flush;
            std::string line;
            while (!m_quit && std::getline(std::cin, line))
            {
                if (onCommand(line))
                    break;
                std::cout << m_prompt << std::flush;
            }
            return;
        }

        // Char-by-char input: disable canonical mode + echo, but leave output
        // processing (\n -> \r\n) and signals (Ctrl-C) untouched.
        termios orig{};
        tcgetattr(STDIN_FILENO, &orig);
        termios raw = orig;
        raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        // The executor owns and persists history; onCommand appends to it, so we
        // just read the live list for up/down recall.
        static const std::vector<std::string> kEmpty;
        std::string line;
        while (!m_quit && readLine(line, m_exec ? m_exec->history() : kEmpty))
        {
            if (onCommand(line))
                break;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        std::cout << std::flush;
    }

    void CLIDisplay::setPrompt(const std::string &prompt)
    {
        m_prompt = prompt;
    }

    void CLIDisplay::onLog(const Loggable &entry)
    {
        std::cout << entry.toString();
    }

    void CLIDisplay::quit()
    {
        m_quit = true;
    }

}
