#include "DetailView.h"
#include "Diary/Loggable.h"
#include "CommandExecutor.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>
#include <thread>

using namespace ftxui;

DetailView::DetailView(CommandExecutor*& exec) : m_exec(exec) {}

void DetailView::run(std::function<bool(const std::string&)>& onCommand) {
    auto screen = App::Fullscreen();
    m_app = &screen;

    {
        std::lock_guard lock(m_mutex);
        m_status.clear();
        m_statusError = false;
    }

    // --- Data ---
    std::string fix_name, fix_type;
    std::string name_val, universe_val, address_val, preset_val;
    std::string num_presets_str, footprint_val;
    bool serial_on = false, wifi_on = false, anim_on = false;

    // Fetch config + engine state OFF the UI thread so the detail view renders
    // instantly instead of blocking on two network round-trips. The worker
    // computes plain strings, then marshals them back via screen.Post() — the
    // only safe place to touch the values bound to the Input components. The
    // worker is always joined before this function returns (see after Loop),
    // so the captured stack references can never dangle.
    std::thread worker;
    auto joinWorker = [&]() { if (worker.joinable()) worker.join(); };

    auto refreshData = [&]() {
        joinWorker();  // at most one fetch in flight
        worker = std::thread([&, scr = &screen]() {
            std::string nm = m_exec ? m_exec->getSelectedName() : "";
            std::string ty = m_exec ? m_exec->getSelectedType() : "";

            std::string name_v, uni_v, addr_v, preset_v, npresets_v, foot_v = "-";
            bool ser = false, wifi = false, anim = false;

            auto cfg_resp = m_exec ? m_exec->fetchFixtureConfigJson() : "";
            if (!cfg_resp.empty()) {
                try {
                    auto data = nlohmann::json::parse(cfg_resp);
                    if (!data.contains("err") && data.contains("val")) {
                        auto& v = data["val"];
                        name_v = v.value("name", "");
                        uni_v = std::to_string(v.value("universe", 0));
                        addr_v = std::to_string(v.value("address", 0));
                        preset_v = std::to_string(v.value("presetIndex", 0));
                        npresets_v = std::to_string(v.value("numPresets", 0));
                        foot_v = v.contains("footprint")
                            ? std::to_string(v["footprint"].get<int>()) : "-";
                    }
                } catch (...) {}
            }

            auto desc_resp = m_exec ? m_exec->fetchDescribeJson() : "";
            if (!desc_resp.empty()) {
                try {
                    auto data = nlohmann::json::parse(desc_resp);
                    if (!data.contains("err")) {
                        ser = data.value("serial_report_task", false);
                        wifi = data.value("wireless_report_task", false);
                        anim = data.value("wifi_animation", false);
                    }
                } catch (...) {}
            }

            // Marshal onto the UI thread. Targets captured by reference (alive
            // until joinWorker() below); fetched values + screen ptr by value.
            scr->Post([scr, &fix_name, &fix_type, &name_val, &universe_val,
                       &address_val, &preset_val, &num_presets_str, &footprint_val,
                       &serial_on, &wifi_on, &anim_on,
                       nm, ty, name_v, uni_v, addr_v, preset_v, npresets_v, foot_v,
                       ser, wifi, anim]() {
                fix_name = nm; fix_type = ty;
                name_val = name_v; universe_val = uni_v; address_val = addr_v;
                preset_val = preset_v; num_presets_str = npresets_v;
                footprint_val = foot_v;
                serial_on = ser; wifi_on = wifi; anim_on = anim;
                scr->PostEvent(ftxui::Event::Custom);
            });
        });
    };

    // Any command may mutate or destroy m_client / the response cache. The
    // fetch worker also touches m_client, so it MUST be finished before a
    // command runs — otherwise e.g. "back" resets m_client mid-fetch (bad_alloc).
    auto runCmd = [&](const std::string& c) {
        joinWorker();
        return onCommand(c);
    };

    refreshData();

    // --- Editable input fields ---
    auto makeField = [&](std::string& val, const std::string& cmd) -> Component {
        auto opt = InputOption::Default();
        opt.multiline = false;
        opt.on_enter = [&val, &runCmd, &refreshData, cmd]() {
            runCmd(cmd + " " + val);
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
        opt.on_change = [&val, &runCmd, cmd]() {
            runCmd(cmd + " " + (val ? "true" : "false"));
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
        runCmd("back");
        screen.Exit();
    }, makeBtn(true));

    auto reboot_btn = Button("Reboot", [&]() {
        runCmd("reboot");
        screen.Exit();
    }, makeBtn());

    auto highlight_btn = Button("Highlight", [&]() {
        runCmd("highlight");
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

        Element status_el = text("");
        {
            std::lock_guard lock(m_mutex);
            if (!m_status.empty()) {
                auto col = m_statusError ? Color::RGB(220, 120, 120)
                                         : Color::RGB(120, 200, 140);
                status_el = text(" " + m_status) | color(col) | center;
            }
        }

        return vbox({
            header,
            text(""),
            config_section,
            text(""),
            engine_section,
            filler(),
            status_el,
            buttons_row,
            text(""),
        }) | flex;
    });

    // Escape → back
    auto with_escape = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Escape) {
            runCmd("back");
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(with_escape);
    joinWorker();   // ensure the fetch worker can't outlive this frame's stack
    m_app = nullptr;
}

void DetailView::onLog(const Loggable& entry) {
    // Surface command feedback (e.g. "Set name to ...", errors) as a status
    // line. Debug noise is ignored.
    if (entry.type == Loggable::PRINT || entry.type == Loggable::ERROR) {
        std::string msg = stripAnsi(entry.message);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' '))
            msg.pop_back();
        if (!msg.empty()) {
            std::lock_guard lock(m_mutex);
            m_status = msg;
            m_statusError = (entry.type == Loggable::ERROR);
        }
    }
    redraw();
}
