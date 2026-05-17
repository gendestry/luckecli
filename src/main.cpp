// #define FXTUI

#ifndef FXTUI
#include <iostream>
#include <string>

#include "Utils/Logging/Logger.h"
#include "CommandExecutor.h"

int main() {
    Utils::Logger logger("Main");
    logger.toggleScope();

    CommandExecutor exec;
    logger.print("> ");

    std::string line;
    while (std::getline(std::cin, line)) {
        auto resolved = exec.resolveCommand(line);
        if (exec.shouldQuit()) {
            break;
        }
    }

    return 0;
}

#else

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "CommandExecutor.h"


#include <string>
#include <vector>

using namespace ftxui;

int main() {
    auto screen = ScreenInteractive::Fullscreen();
    CommandExecutor exec;
  // --- fake data (clients) ---
  std::vector<std::string> clients = {
    "miza (192.168.0.140)",
    "debug (192.168.0.143)",
    "pojsla-side (192.168.0.8)",
  };

  // --- console content ---
  std::vector<std::string> log = {
    "Console started...",
    "Waiting for connections..."
  };

  // --- input state ---
  std::string input;

  // ========== RIGHT PANEL (clients) ==========
  auto clients_component = Renderer([&] {
    Elements items;
    for (auto &c : clients)
      items.push_back(text(c));

    return vbox({
      text("ONLINE CLIENTS") | bold,
      separator(),
      vbox(items) | border,
    });
  });

  // ========== LEFT TOP (console) ==========
  auto console_component = Renderer([&] {
    Elements lines;
    for (auto &l : log)
      lines.push_back(text(l));

    return vbox(lines) | flex | border;
  });

  // ========== LEFT BOTTOM (input) ==========
  auto input_component = Input(&input, "type command...");

  // when Enter pressed → append to console
  input_component |= CatchEvent([&](Event e) {
    if (e == Event::Return) {
      log.push_back("> " + input);
      input.clear();
      return true;
    }
    return false;
  });

  // ========== LEFT PANEL (console + input) ==========
  auto left_panel = Container::Vertical({
    console_component,
    input_component,
  });

  auto left_renderer = Renderer(left_panel, [&] {
    return vbox({
      console_component->Render() | flex,
      separator(),
      input_component->Render(),
    });
  });

  // ========== RIGHT PANEL ==========
  auto right_panel = Container::Vertical({
    clients_component
  });

  auto right_renderer = Renderer(right_panel, [&] {
    return clients_component->Render() | flex | size(WIDTH, EQUAL, 30);
  });

  // ========== MAIN LAYOUT ==========
  auto main_container = Container::Horizontal({
    left_panel,
    right_panel
  });

  auto screen_renderer = Renderer(main_container, [&] {
    return hbox({
      left_renderer->Render() | flex_grow,   // 2/3-ish
      separator(),
      right_renderer->Render() | size(WIDTH, GREATER_THAN, 25),
    });
  });

  screen.Loop(screen_renderer);
}

// #include <ftxui/ftxui.hpp>
// using namespace ftxui;
//
// int main() {
//     auto document =
//       vbox({
//         hbox({
//           text("one") | border,
//           text("two") | border | flex,
//           text("three") | border | flex,
//         }),
//
//         gauge(0.25) | color(Color::Red),
//         gauge(0.50) | color(Color::White),
//         gauge(0.75) | color(Color::Blue),
//       });
//
//     auto screen = Screen::Create(Dimension::Full());
//     Render(screen, document);
//     screen.Print();
//
//     return 0;
// }

/*
int main() {
    Utils::Logger logger("Main");
    logger.toggleScope();
    // logger.setLoggerLevel(Utils::Logger::DEBUGGING);

    Heartbeat heartbeat;
    std::shared_ptr<ESPClient> client;
    CommandExecutor exec;

    auto printOnline = [&]() {
        logger.println("Fixtures online:");
        for (ClientInfo* clientinfo : heartbeat.getClients()) {
            Utils::Text::Stream stream;
            stream << clientinfo->descriptions[0].name;
            for (int i = 1; i < clientinfo->descriptions.size(); i++) {
                stream << ", " << clientinfo->descriptions[i].name;
            }
            logger.println(" - {}: {} {}({}){}", clientinfo->index, stream.end(), Utils::Font::colorItalic, clientinfo->ip, Utils::Font::colorReset);
        }
    };

    bool selected = false;
    bool firstime = true;
    logger.print("> ");

    std::string line;
    while (std::getline(std::cin, line)) {
        exec.resolveCommand(line);
        // if (!firstime) {
        //     if (selected) {
        //         logger.print("{}{}>{} ", Utils::Font::colorYellow, client->getIP(), Utils::Font::colorReset);
        //     }
        //     else {
        //         logger.print("> ");
        //     }
        // }
        // else {
        //     firstime = false;
        // }

        auto args = Utils::String::split(Utils::String::normalize_spaces(line), " ");
        uint8_t argsize = args.size();
        if (argsize == 0) {
            printOnline();
            continue;
        }

        const auto& cmd = args[0];
        if (cmd == "exit") {
            logger.debug("Exiting");
            if (client) client.reset();
            ESPClient::stop();
            break;
        }

        // auto spfirst = sp.begin();


        if (!selected) {
            if (cmd == "list") {
                printOnline();
            }
            else if (cmd == "select") {
                // auto sp = Utils::String::split(line, " ");
                if (argsize == 2) {
                    const auto i = std::stoi(args[1]);
                    const auto size = heartbeat.size();
                    if (i >= size) {
                        logger.error("Index out of bounds, size: {}, index: {}", size, args[1]);
                    }
                    else {
                        const auto ip = heartbeat.getClients()[i]->ip;
                        logger.debug("Selected: {} with ip {}", args[1], ip);
                        selected = true;
                        client = std::make_shared<ESPClient>(ip);
                    }
                }
                else {
                    logger.error("Required inputs: 2, given: {}", args.size());
                }
            }
            else if (Utils::String::isInt(cmd)) {
                const auto i = std::stoi(cmd);
                const auto size = heartbeat.size();
                if (i >= size) {
                    logger.error("Index out of bounds, size: {}, index: {}", size, i);
                }
                else {
                    const auto ip = heartbeat.getClients()[i]->ip;
                    logger.debug("Selected: {} with ip {}", cmd, ip);
                    selected = true;
                    client = std::make_shared<ESPClient>(ip);
                }
            }
            else if (cmd == "selectip") {
                // auto sp = Utils::String::split(line, " ");
                if (argsize == 2) {
                    logger.debug("Selected: {}", args[1]);
                    selected = true;
                    client = std::make_unique<ESPClient>(Utils::String::strip(args[1], "\""));
                }
                else {
                    logger.error("Required inputs: 2, given: {}", args.size());
                }
            }
            else {
                logger.error("Invalid command: {}", line);
            }
        }
        else {

            if (cmd =="back") {
                client.reset();
                selected = false;
            }
            else if (cmd =="describe") {
                client->run(JSONtemp::stringify("describe"), true);
            }
            else if (cmd =="fixtures") {
                client->run(JSONtemp::stringify("fixtures"), true);
            }
            else if (cmd =="inputs") {
                client->run(JSONtemp::stringify("inputs"), true);
            }
            else if (cmd =="presets") {
                client->run(JSONtemp::stringify("presets"), true);
            }
            else if (cmd =="ip") {
                client->run(JSONtemp::stringify("ip"));
            }
            else if (cmd =="factoryreset") {
                client->sendRequest(JSONtemp::stringify("factory_reset"));
            }
            else if (cmd =="reboot") {
                client->run(JSONtemp::stringify("reboot"));
                client.reset();
                selected = false;
            }

            else if (cmd =="wifianimation") {
                if (args.size() == 1) {
                    client->run(JSONtemp::stringify("wifi_animation"));
                }
                else {
                    client->run(JSONtemp::stringify("wifi_animation", "value", args[1]));
                }
            }
            else if (cmd =="serialprint") {
                if (args.size() == 1) {
                    client->run(JSONtemp::stringify("serial_report_task"));
                }
                else {
                    client->run(JSONtemp::stringify("serial_report_task", "value", args[1]));
                }
            }
            else if (cmd =="wirelessprint") {
                if (args.size() == 1) {
                    client->run(JSONtemp::stringify("wireless_report_task"));
                }
                else {
                    client->run(JSONtemp::stringify("wireless_report_task", "value", args[1]));
                }
            }

            else if (cmd =="setwifi") {
                nlohmann::json r;
                r["request"] = "wifi";
                for (int i = 1; i < argsize; i++) {
                    if ((argsize - i % 2) == 1) {
                        logger.error("Invalid argument number for setwifi");
                        continue;
                    }
                    const auto& field = args[i++];
                    auto val = Utils::String::strip(args[i], "\"");
                    if (field == "pass" || field == "password" || field == "p") {
                        r["password"] = val;
                    }
                    else if (field == "ssid" || field == "s") {
                        r["ssid"] = val;
                    }
                }
                // logger.println(r.dump());
                client->run(r.dump()); // TODO: untested
                // client->run(JSONtemp::stringify("wifi", "ssid", sp[1], "password", sp[2]));
            }
            else if (cmd =="universe") {
                uint32_t id = argsize == 2 ? std::stoi(args[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "universe"), true);
            }
            else if (cmd =="address") {
                uint32_t id = argsize == 2 ? std::stoi(args[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "address"), true);
            }
            else if (cmd =="preset") {
                uint32_t id = argsize == 2 ? std::stoi(args[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "preset"), true);
            }
            else if (cmd =="name") {
                uint32_t id = argsize == 2 ? std::stoi(args[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "name"), true);
            }
            else if (cmd =="type") {
                uint32_t id = argsize == 2 ? std::stoi(args[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "type"), true);
            }
            else if (cmd =="setuniverse") {
                uint32_t id = argsize == 3 ? std::stoi(args[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "universe", std::stoi(args[1])));
            }
            else if (cmd =="setaddress") {
                uint32_t id = argsize == 3 ? std::stoi(args[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "address", std::stoi(args[1])));
            }
            else if (cmd =="setpreset") {
                uint32_t id = argsize == 3 ? std::stoi(args[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "presetIndex", std::stoi(args[1])));
            }
            else if (cmd =="highlight") {
                uint32_t id = argsize == 3 ? std::stoi(args[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "highlight", args[1]));
            }
            else if (cmd =="setname") {
                uint32_t id = argsize == 3 ? std::stoi(args[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "name", args[1]));
            }
            else {
                logger.error("Invalid command: {}", line);
            }
        }
        if (selected) {
            logger.print("{}{}>{} ", Utils::Font::colorYellow, client->getIP(), Utils::Font::colorReset);
        }
        else {
            logger.print("> ");
        }
    }

    return 0;
}
*/

#endif