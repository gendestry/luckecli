#include "FTXUIDisplay.h"
#include "Diary/Loggable.h"
#include "SharedState.h"
#include "CommandExecutor.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>

static std::string stripAnsi(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool in_esc = false;
    for (char c : s) {
        if (in_esc) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                in_esc = false;
        } else if (c == '\033') {
            in_esc = true;
        } else {
            out += c;
        }
    }
    return out;
}

FTXUIDisplay::FTXUIDisplay(SharedState& state) : m_state(state) {}

void FTXUIDisplay::bindCommandSource(CommandExecutor* exec) {
    m_exec = exec;
}

// =============================================================================
// Main loop
// =============================================================================

void FTXUIDisplay::run(std::function<bool(const std::string&)> onCommand) {
    m_quit = false;
    while (!m_quit) {
        runGridView(onCommand);
        if (m_quit) break;
        if (m_exec && m_exec->isSelected()) {
            runDetailView(onCommand);
        }
    }
}

// =============================================================================
// Grid view: fixture cards
// =============================================================================

void FTXUIDisplay::runGridView(std::function<bool(const std::string&)>& onCommand) {
    using namespace ftxui;

    while (!m_quit && !(m_exec && m_exec->isSelected())) {
        m_stateChanged = false;

        auto screen = App::Fullscreen();
        m_app = &screen;

        struct FixtureEntry {
            int id;
            std::string name, type, ip, version;
            int num_leds;
        };
        std::vector<FixtureEntry> fixtures;
        {
            const auto& clients = m_state.getClients();
            int idx = 0;
            for (const auto& c : clients) {
                fixtures.push_back({idx++, c.description.name, c.description.type,
                                    c.ip, c.version, c.description.num_leds});
            }
        }

        Components buttons;
        for (auto& f : fixtures) {
            auto opt = ButtonOption::Simple();
            int fid = f.id;
            std::string fname = f.name, ftype = f.type, fip = f.ip;
            int fleds = f.num_leds;
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
                if (fleds > 0)
                    lines.push_back(text(std::to_string(fleds) + " leds") | color(Color::RGB(120, 190, 130)));

                auto card = vbox(lines) | size(WIDTH, EQUAL, 28) | size(HEIGHT, EQUAL, 5);
                if (s.focused) {
                    return card | borderStyled(ROUNDED, Color::RGB(190, 160, 230));
                }
                return card | borderStyled(ROUNDED, Color::RGB(50, 50, 60));
            };
            buttons.push_back(Button(opt));
        }

        Component grid_container;
        if (buttons.empty()) {
            grid_container = Renderer([] {
                return text("  waiting for devices...") | dim | center;
            });
        } else {
            const int COLS = 3;
            Components rows;
            for (int i = 0; i < (int)buttons.size(); i += COLS) {
                Components row;
                for (int j = i; j < std::min((int)buttons.size(), i + COLS); j++)
                    row.push_back(buttons[j]);
                rows.push_back(Container::Horizontal(std::move(row)));
            }
            grid_container = Container::Vertical(std::move(rows));
        }

        auto renderer = Renderer(grid_container, [&] {
            return window(
                text(" Online Fixtures ") | bold | color(Color::RGB(100, 150, 220)),
                grid_container->Render() | center | flex
            ) | flex;
        });

        auto with_keys = CatchEvent(renderer, [&](Event event) {
            if (event == Event::Escape) {
                onCommand("exit");
                m_quit = true;
                screen.Exit();
                return true;
            }
            return false;
        });

        screen.Loop(with_keys);
        m_app = nullptr;

        if (!m_stateChanged) break;
    }
}

// =============================================================================
// Detail view: modern dashboard — editable fields, toggles, action buttons
// =============================================================================

void FTXUIDisplay::runDetailView(std::function<bool(const std::string&)>& onCommand) {
    using namespace ftxui;

    auto screen = App::Fullscreen();
    m_app = &screen;

    // --- Data ---
    std::string fix_name, fix_type;
    std::string name_val, universe_val, address_val, preset_val;
    std::string num_presets_str, footprint_val, leds_val;
    bool serial_on = false, wifi_on = false, anim_on = false;

    auto refreshData = [&]() {
        fix_name = m_exec ? m_exec->getSelectedName() : "";
        fix_type = m_exec ? m_exec->getSelectedType() : "";
        int nl = m_exec ? m_exec->getSelectedNumLeds() : 0;
        leds_val = nl > 0 ? std::to_string(nl) : "-";

        auto cfg_resp = m_exec ? m_exec->fetchFixtureConfigJson() : "";
        if (!cfg_resp.empty()) {
            try {
                auto data = nlohmann::json::parse(cfg_resp);
                if (!data.contains("err") && data.contains("val")) {
                    auto& v = data["val"];
                    name_val = v.value("name", "");
                    universe_val = std::to_string(v.value("universe", 0));
                    address_val = std::to_string(v.value("address", 0));
                    preset_val = std::to_string(v.value("presetIndex", 0));
                    num_presets_str = std::to_string(v.value("numPresets", 0));
                    footprint_val = v.contains("footprint")
                        ? std::to_string(v["footprint"].get<int>()) : "-";
                }
            } catch (...) {}
        }

        auto desc_resp = m_exec ? m_exec->fetchDescribeJson() : "";
        if (!desc_resp.empty()) {
            try {
                auto data = nlohmann::json::parse(desc_resp);
                if (!data.contains("err")) {
                    serial_on = data.value("serial_report_task", false);
                    wifi_on = data.value("wireless_report_task", false);
                    anim_on = data.value("wifi_animation", false);
                }
            } catch (...) {}
        }
    };

    refreshData();

    // --- Editable input fields ---
    auto makeField = [&](std::string& val, const std::string& cmd) -> Component {
        auto opt = InputOption::Default();
        opt.multiline = false;
        opt.on_enter = [&val, &onCommand, &refreshData, cmd]() {
            onCommand(cmd + " " + val);
            refreshData();
        };
        opt.transform = [](InputState s) {
            s.element |= color(Color::RGB(220, 190, 100));
            if (s.focused) {
                s.element |= bgcolor(Color::RGB(35, 35, 50));
            }
            return s.element;
        };
        return Input(&val, opt);
    };

    auto name_input = makeField(name_val, "name");
    auto universe_input = makeField(universe_val, "universe");
    auto address_input = makeField(address_val, "address");
    auto preset_input = makeField(preset_val, "preset");

    // --- Toggle switches ---
    auto makeToggle = [&](bool& val, const std::string& cmd) -> Component {
        auto opt = CheckboxOption::Simple();
        opt.on_change = [&val, &onCommand, cmd]() {
            onCommand(cmd + " " + (val ? "true" : "false"));
        };
        opt.transform = [&val](const EntryState&) {
            if (val) {
                return text("  ON  ") | bold
                    | bgcolor(Color::RGB(35, 90, 50)) | color(Color::RGB(120, 230, 140))
                    | borderStyled(ROUNDED, Color::RGB(50, 120, 70));
            }
            return text("  OFF ")
                | bgcolor(Color::RGB(70, 35, 35)) | color(Color::RGB(180, 120, 120))
                | borderStyled(ROUNDED, Color::RGB(90, 50, 50));
        };
        return Checkbox("", &val, opt);
    };

    auto serial_toggle = makeToggle(serial_on, "serialprint");
    auto wifi_toggle = makeToggle(wifi_on, "wirelessprint");
    auto anim_toggle = makeToggle(anim_on, "wifianimation");

    // --- Action buttons ---
    auto makeBtn = [](bool accent = false) {
        auto opt = ButtonOption::Simple();
        opt.transform = [accent](const EntryState& s) {
            auto e = text(" " + s.label + " ");
            if (s.focused) {
                return e | bold | bgcolor(Color::RGB(60, 60, 100)) | color(Color::White)
                    | borderStyled(ROUNDED, Color::RGB(120, 120, 200));
            }
            if (accent) {
                return e | color(Color::RGB(190, 160, 230))
                    | borderStyled(ROUNDED, Color::RGB(80, 70, 100));
            }
            return e | color(Color::RGB(160, 160, 160))
                | borderStyled(ROUNDED, Color::RGB(50, 50, 60));
        };
        return opt;
    };

    auto back_btn = Button("← Back", [&]() {
        onCommand("back");
        screen.Exit();
    }, makeBtn(true));

    auto reboot_btn = Button("Reboot", [&]() {
        onCommand("reboot");
        screen.Exit();
    }, makeBtn());

    auto highlight_btn = Button("Highlight", [&]() {
        onCommand("highlight");
    }, makeBtn());

    auto refresh_btn = Button("Refresh", [&]() {
        refreshData();
    }, makeBtn());

    // --- Component tree ---
    auto fields = Container::Vertical({
        name_input, universe_input, address_input, preset_input,
    });
    auto toggles = Container::Vertical({serial_toggle, wifi_toggle, anim_toggle});
    auto btns = Container::Horizontal({back_btn, reboot_btn, highlight_btn, refresh_btn});
    auto all = Container::Vertical({fields, toggles, btns});

    // --- Renderer ---
    auto renderer = Renderer(all, [&] {
        auto lbl = [](const std::string& s) {
            return text(s) | color(Color::RGB(80, 190, 190));
        };
        auto ro = [](const std::string& s) {
            return text(s) | color(Color::RGB(220, 190, 100));
        };
        auto dim_text = [](const std::string& s) {
            return text(s) | color(Color::RGB(100, 100, 100));
        };

        int fw = 12;

        auto header = hbox({
            text(" " + fix_name + " ") | bold | color(Color::RGB(190, 160, 230)),
            filler(),
            text(" " + fix_type + " ") | color(Color::RGB(130, 130, 130)),
        }) | borderStyled(ROUNDED, Color::RGB(60, 50, 80));

        auto config_section = window(
            text(" Fixture ") | bold | color(Color::RGB(100, 150, 220)),
            vbox({
                hbox({lbl("  name       "), name_input->Render() | flex}),
                text(""),
                hbox({
                    lbl("  universe   "), universe_input->Render() | size(WIDTH, EQUAL, fw),
                    text("      "),
                    lbl("address   "), address_input->Render() | size(WIDTH, EQUAL, fw),
                }),
                text(""),
                hbox({
                    lbl("  preset     "), preset_input->Render() | size(WIDTH, EQUAL, fw),
                    dim_text(" / " + num_presets_str),
                }),
                text(""),
                hbox({
                    lbl("  footprint  "), ro(footprint_val),
                    text("      "),
                    lbl("leds      "), ro(leds_val),
                }),
            })
        ) | borderStyled(ROUNDED, Color::RGB(40, 40, 55));

        auto engine_section = window(
            text(" Engine ") | bold | color(Color::RGB(100, 150, 220)),
            vbox({
                hbox({lbl("  Serial print    "), serial_toggle->Render()}),
                hbox({lbl("  WiFi print      "), wifi_toggle->Render()}),
                hbox({lbl("  WiFi animation  "), anim_toggle->Render()}),
            })
        ) | borderStyled(ROUNDED, Color::RGB(40, 40, 55));

        auto buttons_row = hbox({
            back_btn->Render(), text("  "),
            reboot_btn->Render(), text("  "),
            highlight_btn->Render(), text("  "),
            refresh_btn->Render(),
        }) | center;

        return vbox({
            header,
            text(""),
            config_section,
            text(""),
            engine_section,
            filler(),
            buttons_row,
            text(""),
        }) | flex;
    });

    // Escape → back
    auto with_escape = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Escape) {
            onCommand("back");
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(with_escape);
    m_app = nullptr;
}

// =============================================================================
// Display interface
// =============================================================================

void FTXUIDisplay::setPrompt(const std::string& prompt) {
    m_prompt = stripAnsi(prompt);
}

void FTXUIDisplay::onLog(const Loggable& entry) {
    // Log entries are silently accumulated (no console in modern UI)
    if (m_app) {
        m_app->PostEvent(ftxui::Event::Custom);
    }
}

void FTXUIDisplay::quit() {
    m_quit = true;
    if (m_app) m_app->Exit();
}

void FTXUIDisplay::onStateChanged() {
    if (m_app) {
        if (m_exec && !m_exec->isSelected()) {
            // Grid view: exit and rebuild to pick up new devices
            m_app->Post([this]() {
                m_stateChanged = true;
                m_app->Exit();
            });
        } else {
            // Detail view: just trigger redraw
            m_app->PostEvent(ftxui::Event::Custom);
        }
    }
}
