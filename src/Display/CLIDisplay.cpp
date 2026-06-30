#include "CLIDisplay.h"
#include "Diary/Loggable.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <termios.h>
#include <unistd.h>

namespace Test
{
    namespace
    {
        constexpr std::size_t kMaxHistory = 1000;

        std::string historyPath()
        {
            const char *home = std::getenv("HOME");
            return (home ? std::string(home) : std::string(".")) + "/.luckecli_history";
        }

        std::vector<std::string> loadHistory(const std::string &path)
        {
            std::vector<std::string> hist;
            std::ifstream in(path);
            std::string line;
            while (std::getline(in, line))
                if (!line.empty())
                    hist.push_back(line);
            if (hist.size() > kMaxHistory)
                hist.erase(hist.begin(), hist.end() - kMaxHistory);
            return hist;
        }

        void saveHistory(const std::string &path, const std::vector<std::string> &hist)
        {
            std::ofstream out(path, std::ios::trunc);
            for (const auto &h : hist)
                out << h << '\n';
        }
    }

    // Read one line with up/down history navigation and backspace. The terminal
    // is in raw mode (char-by-char, no echo) but keeps output post-processing, so
    // the cursor stays at the end of the line — no left/right editing yet, which
    // means the (coloured) prompt needs no width accounting. Returns false on EOF
    // (Ctrl-D on an empty line / closed stdin).
    bool CLIDisplay::readLine(std::string &out, std::vector<std::string> &history)
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

        const std::string histPath = historyPath();
        std::vector<std::string> history = loadHistory(histPath);

        // Char-by-char input: disable canonical mode + echo, but leave output
        // processing (\n -> \r\n) and signals (Ctrl-C) untouched.
        termios orig{};
        tcgetattr(STDIN_FILENO, &orig);
        termios raw = orig;
        raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        std::string line;
        while (!m_quit && readLine(line, history))
        {
            if (!line.empty() && (history.empty() || history.back() != line))
            {
                history.push_back(line);
                if (history.size() > kMaxHistory)
                    history.erase(history.begin());
                saveHistory(histPath, history);
            }
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
