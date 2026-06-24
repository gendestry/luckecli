#include "GridView.h"
#include "SharedState.h"
#include "CommandExecutor.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

GridView::GridView(SharedState& state, CommandExecutor*& exec)
    : m_state(state), m_exec(exec) {}

void GridView::run(std::function<bool(const std::string&)>& onCommand) {
    auto screen = App::Fullscreen();
    m_app = &screen;

    // Stable root: a single container whose children (the device cards) are
    // rebuilt in place when the device set changes — no per-heartbeat teardown.
    auto grid_container = Container::Vertical({});

    auto rebuild = [&]() {
        // Pull fresh fixture names, then snapshot the client list.
        if (m_exec) m_exec->syncFixtureNames();

        struct FixtureEntry {
            int id;
            std::string name, type, ip, version;
        };
        std::vector<FixtureEntry> fixtures;
        {
            const auto& clients = m_state.getClients();
            int idx = 0;
            for (const auto& c : clients) {
                fixtures.push_back({idx++, c.description.name, c.description.type,
                                    c.wifi.ip, c.engine.version});
            }
        }

        m_buttons.clear();
        for (auto& f : fixtures) {
            auto opt = ButtonOption::Simple();
            int fid = f.id;
            std::string fname = f.name, ftype = f.type, fip = f.ip;
            opt.on_click = [&, fid]() {
                onCommand("select " + std::to_string(fid));
                screen.Exit();
            };
            opt.transform = [=](const EntryState& s) {
                Elements lines;
                lines.push_back(hbox({
                    text(fname) | bold | color(Color::RGB(190, 160, 230)),
                    filler(),
                    text(std::to_string(fid)) | color(Color::RGB(100, 100, 100)),
                }));
                if (!ftype.empty())
                    lines.push_back(text(ftype) | color(Color::RGB(130, 130, 130)));
                lines.push_back(text(fip) | color(Color::RGB(140, 170, 210)));

                auto card = vbox(lines) | size(WIDTH, EQUAL, 28) | size(HEIGHT, EQUAL, 5);
                if (s.focused) {
                    return card | borderStyled(ROUNDED, Color::RGB(190, 160, 230));
                }
                return card | borderStyled(ROUNDED, Color::RGB(50, 50, 60));
            };
            m_buttons.push_back(Button(opt));
        }

        grid_container->DetachAllChildren();
        if (m_buttons.empty()) {
            grid_container->Add(Renderer([] {
                return text("  waiting for devices...") | dim | center;
            }));
        } else {
            const int COLS = 3;
            for (int i = 0; i < (int)m_buttons.size(); i += COLS) {
                Components row;
                for (int j = i; j < std::min((int)m_buttons.size(), i + COLS); j++)
                    row.push_back(m_buttons[j]);
                grid_container->Add(Container::Horizontal(std::move(row)));
            }
        }
    };

    rebuild();

    // Expose the in-place rebuild to onStateChanged() for the loop's duration.
    // It is only ever invoked on the UI thread (via screen.Post), and cleared
    // before run() returns so its captured references can't dangle.
    m_rebuild = rebuild;

    auto renderer = Renderer(grid_container, [&] {
        return window(
            text(" Online Fixtures ") | bold | color(Color::RGB(100, 150, 220)),
            grid_container->Render() | center | flex
        ) | flex;
    });

    auto with_keys = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Escape) {
            onCommand("exit");   // routes through CommandExecutor → Display::quit()
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(with_keys);

    m_rebuild = nullptr;
    m_app = nullptr;
}

void GridView::onStateChanged() {
    if (auto* s = m_app.load()) {
        s->Post([this, s]() {
            if (m_rebuild) m_rebuild();
            s->PostEvent(ftxui::Event::Custom);
        });
    }
}
