#include "View.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>

namespace Test
{

    void View::requestExit()
    {
        if (auto *s = m_app.load())
            s->Exit();
    }

    void View::redraw()
    {
        if (auto *s = m_app.load())
            s->PostEvent(ftxui::Event::Custom);
    }

    std::string stripAnsi(const std::string &s)
    {
        std::string out;
        out.reserve(s.size());
        bool in_esc = false;
        for (char c : s)
        {
            if (in_esc)
            {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                    in_esc = false;
            }
            else if (c == '\033')
            {
                in_esc = true;
            }
            else
            {
                out += c;
            }
        }
        return out;
    }

}
