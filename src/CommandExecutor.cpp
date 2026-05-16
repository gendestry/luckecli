//
// Created by bobi on 10. 05. 26.
//

#include "CommandExecutor.h"
#include "Utils/Text/String.h"
#include "JSONtemp.h"
#include <iostream>
#include <ranges>
#include <nlohmann/json.hpp>

CommandExecutor::CommandExecutor()
    : logger("Command") {
    logger.toggleScope();

    bindcommands();
}

bool CommandExecutor::resolveCommand(const std::string& line) {
    auto args = Utils::String::split(Utils::String::normalize_spaces(line), " ");
    const std::string& cmd = args.empty() ? "" : args[0];

    bool result = false;

    auto anytime = cmdlist.anytime(cmd);
    if (anytime) {
        result = anytime.value()(args);
    }
    else {
        if (!selected)
        {
            auto fun = cmdlist.unselected(cmd);
            if (cmd.empty()) {
                result = list(args);
            }
            else if (Utils::String::isInt(cmd)) {
                result = select(args);
            }
            else if (fun) {
                result = fun.value()(args);
            }
            else {
                logger.error("Invalid command: {}", line);
            }
        }
        else {
            auto fun = cmdlist.selected(cmd);
            if (cmd.empty()) {
                result = get_fixture_config();
            }
            else if (fun) {
                result = fun.value()(args);
            }
            else {
                logger.error("Invalid command: {}", line);
            }
        }
    }

    if (selected) {
        logger.print("{}{}@{}>{} ", Utils::Font::colorYellow, heartbeat[client->getIP()].descriptions[0].name, client->getIP(), Utils::Font::colorReset);
    }
    else {
        logger.print("> ");
    }

    return result;
}

void CommandExecutor::bindcommands() {
    cmdlist.registerCommand(Command("exit", "Exits the program", "exit",
[this](const std::vector<std::string>& args) {
        return exit(args);
    }), CommandList::Usable::ANYTIME);

    cmdlist.registerCommand(Command("help", "Prints help", "help",
    [this](const std::vector<std::string>& args) {
        return help(args);
    }), CommandList::Usable::ANYTIME);

    cmdlist.registerCommand(Command("list", "Lists available items", "list or <ENTER>",
    [this](const std::vector<std::string>& args) {
        return this->list();
    }), CommandList::Usable::UNSELECTED);

    cmdlist.registerCommand(Command("select", "Select an item", "select <number> or <number>",
    [this](const std::vector<std::string>& args) {
        return select(args);
    }), CommandList::Usable::UNSELECTED);

    cmdlist.registerCommand(Command("config", "Prints all config", "config",
    [this](const std::vector<std::string>& args) {
        return all_config();
    }), CommandList::Usable::UNSELECTED);

    auto task_config_cmd = [this](const std::vector<std::string>& args) {
        return task_config(args);
    };

    auto get_info_cmd = [this](const std::vector<std::string>& args) {
        return get_info(args);
    };

    auto get_fixture_info_cmd = [this](const std::vector<std::string>& args) {
        return get_fixture_info(args);
    };

    auto set_fixture_info_cmd = [this](const std::vector<std::string>& args) {
        return set_fixture_info(args);
    };

    // ---------- selected commands ----------
    cmdlist.registerCommand(Command("wifianimation", "Configure WiFi animation task", "wifianimation <true/false>?",
    task_config_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("serialprint", "Print or configure serial print", "serialprint <true/false>?",
    task_config_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("wirelessprint", "Print or configure wireless print", "wirelessprint <true/false>?",
    task_config_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("setwifi", "Configure WiFi settings", "setwifi ssid <value> pass <value>",
    [this](const std::vector<std::string>& args) {
        return setwifi(args);
    }), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("back", "Return to previous menu", "back",
    [this](const std::vector<std::string>& args) {
        return deselect();
    }), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("reboot", "Reboot device", "reboot",
    [this](const std::vector<std::string>& args) {
        return reboot(args);
    }), CommandList::Usable::SELECTED);


    // ---------- grouped info commands ----------
    cmdlist.registerCommand(Command("describe", "Show info", "describe",
    get_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("fixtures", "Show fixtures info", "fixtures",
    get_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("inputs", "Show inputs info", "inputs",
    get_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("presets", "Show presets info", "presets",
    [this](const std::vector<std::string>& args) {
        return presets(args);
    }), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("ip", "Show IP info", "",
    [this](const std::vector<std::string>& args) {
        client->run(JSONtemp::stringify(args[0]), false);
        return true;
    }), CommandList::Usable::SELECTED);


    // ---------- fixture getters ----------
    cmdlist.registerCommand(Command("name", "Get fixture name", "name <number=0>?",
    get_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("universe", "Get fixture universe", "universe <number=0>?",
    get_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("address", "Get fixture address", "address <number=0>?",
    get_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("preset", "Get fixture preset", "preset <number=0>?",
    get_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("highlight", "Highlight fixture", "highlight <number=0>?",
    get_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("config", "Get full fixture config", "config <number=0>?",
    [this](const std::vector<std::string>& args) {
        return get_fixture_config(args);
    }), CommandList::Usable::SELECTED);


    // ---------- fixture setters ----------
    cmdlist.registerCommand(Command("setname", "Set fixture name", "setname <string> <number=0>?",
    set_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("setuniverse", "Set fixture universe", "setuniverse <number> <number=0>?",
    set_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("setaddress", "Set fixture address", "setaddress <number> <number=0>?",
    set_fixture_info_cmd), CommandList::Usable::SELECTED);

    cmdlist.registerCommand(Command("setpreset", "Set fixture preset", "setpreset <number> <number=0>?",
    set_fixture_info_cmd), CommandList::Usable::SELECTED);
}

bool CommandExecutor::exit(const std::vector<std::string>&) {
    logger.debug("Exiting");
    if (client) client.reset();
    ESPClient::stop();
    exitRequest = true;
    return true;
}

bool CommandExecutor::help(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        auto info = cmdlist.get(args[1]);
        if (info) {
            logger.println(" Help for command {}", info->get().name);
            logger.println(" - description: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info->get().desc, Utils::Font::colorReset);
            logger.println(" - usage: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info->get().usage, Utils::Font::colorReset);
            return true;
        }
        logger.println("Command {} does not exist", args[1]);
        return false;
    }

    logger.println("Help:");
    auto& anytime    = cmdlist.commandsMap[CommandList::Usable::ANYTIME];
    auto& specific   = cmdlist.commandsMap[selected
                            ? CommandList::Usable::SELECTED
                            : CommandList::Usable::UNSELECTED];
//     auto combined = std::views::concat(
//     anytime | std::views::values,
//     specific | std::views::values
// );

    for (auto& value : anytime | std::views::values)
    {
        logger.println(
            " - {}: {}{}{}{}",
            value.name,
            Utils::Font::colorByRGB(100, 100, 100),
            Utils::Font::colorItalic,
            value.desc,
            Utils::Font::colorReset
        );
    }

    for (auto& value : specific | std::views::values)
    {
        logger.println(
            " - {}: {}{}{}{}",
            value.name,
            Utils::Font::colorByRGB(100, 100, 100),
            Utils::Font::colorItalic,
            value.desc,
            Utils::Font::colorReset
        );
    }
    // const auto& submap = cmdlist.commandsMap[selected ? CommandList::Usable::SELECTED : CommandList::Usable::UNSELECTED];
    // for (auto& [_, value] : submap) {
    //     logger.println(" - {}: {}{}{}{}", value.name, Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, value.desc, Utils::Font::colorReset);
    // }
    // for (auto& [_, cmdinfo] : selected ? commandsSelected : commands) {
    //     logger.println(" - {}: {}{}{}{}", cmdinfo.name, Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, cmdinfo.desc, Utils::Font::colorReset);
    // }
    return true;
}

bool CommandExecutor::reboot(const std::vector<std::string>&) {
    client->run(JSONtemp::stringify("reboot"));
    client.reset();
    selected = false;
    return true;
}

bool CommandExecutor::list(const std::vector<std::string>&) {
    logger.println("Fixtures online:");
    for (auto clientinfo : heartbeat.getClients()) {
        Utils::Text::Stream stream;
        stream << clientinfo->descriptions[0].name;
        for (int i = 1; i < clientinfo->descriptions.size(); i++) {
            stream << ", " << clientinfo->descriptions[i].name;
        }
        logger.print(" - {}: {} {}({}){}", clientinfo->index, stream.end(), Utils::Font::colorItalic, clientinfo->ip, Utils::Font::colorReset);
        logger.println("{}{} v{}{}", Utils::Font::colorGreen, Utils::Font::colorItalic, clientinfo->version, Utils::Font::colorReset);
    }
    return true;
}

bool CommandExecutor::all_config(const std::vector<std::string>&) {
    const auto& allclients = heartbeat.getClients();
    for (int i = 0; i < allclients.size(); i++) {
        auto& client = allclients[i];
        select({std::to_string(i)});
        get_fixture_config();
        deselect();
    }

    return true;
}

bool CommandExecutor::select(const std::vector<std::string>& args) {
    int i = 0;
    if (Utils::String::isInt(args[0])) {
        i = std::stoi(args[0]);
    }
    else if (args[0] == "select" && args.size() == 2 && Utils::String::isInt(args[1])) {
        i = std::stoi(args[1]);
    }

    else {
        return false;
    }
    const auto size = heartbeat.size();
    if (i >= size) {
        logger.error("Index out of bounds, size: {}, index: {}", size, args[0]);
    }
    else {
        // selectIndex(i);
        const auto ip = heartbeat.getClients()[i]->ip;
        logger.debug("Selected: {} with ip {}", i, ip);
        selected = true;
        client = std::make_unique<ESPClient>(ip);
    }
    return true;
        // logger.error("Required inputs: 2, given: {}", args.size());
}

bool CommandExecutor::selectIP(const std::vector<std::string>& args) {
    return true;
}


bool CommandExecutor::deselect(const std::vector<std::string>& args) {
    client.reset();
    selected = false;
    return true;
}

bool CommandExecutor::task_config(const std::vector<std::string>& args) {
    std::string cmd = args[0];
    std::unordered_map<std::string, std::string> mapp;
    mapp["wifianimation"] ="wifi_animation";
    mapp["serialprint"] = "serial_report_task";
    mapp["wirelessprint"] = "wireless_report_task";

    if (mapp.contains(cmd)) {
        logger.debug("{} exists in array", cmd);
        if (args.size() == 2) {
            bool val = args[1] == "true" ? true : false;
            client->run(JSONtemp::stringify(mapp[cmd], "value", val), false);
        }
        else {
            client->run(JSONtemp::stringify(mapp[cmd]), false);
        }
        return true;
    }

    return false;
}

bool CommandExecutor::get_info(const std::vector<std::string>& args) {
    std::string cmd = args[0];
    logger.debug("{} exists in array", cmd);
    client->run(JSONtemp::stringify(args[0]), true);
    return true;
}

bool CommandExecutor::presets(const std::vector<std::string>& args) {
    auto response = client->runStr(JSONtemp::stringify(args[0]));
    if (response.empty()) {
        return false;
    }

    logger.println("Listing presets: ");
    Utils::Text::Stream streamSelected;
    Utils::Text::Stream streamAll;

    using nlohmann::json;
    json respj = json::parse(response);
    std::string respdata = respj["response"];
    json j = json::parse(respdata);

    for (const auto& preset : j["presets"]) {
        for (auto& [key, value] : preset.items()) {
            logger.println("Fixture name: {}", key);

            int current = value.value("selected", -1);
            if (!value.contains("presets")) continue;

            const auto& innerPresets = value["presets"];

            if (!innerPresets.contains("groups")) continue;

            const auto& groups = innerPresets["groups"];
            uint8_t c = 0;
            for (const auto& group : groups) {
                std::string name = group["name"];

                if (c == current) {
                    streamSelected << Utils::String::format("[{}]: \"{}\"\n", c, name);
                }
                streamAll << Utils::String::format("[{}]: \"{}\"\n", c, name);

                for (const auto& setting : group["settings"]) {

                    std::string pretty;
                    // {
                    //     auto dump = setting.dump(2);
                    //     Utils::Text::Stream stream;
                    //     auto spl = Utils::String::split(dump, "\n");
                    //     auto pad = Utils::String::pad(2, " ");
                    //     for (auto t : spl) {
                    //         stream << Utils::String::concat(pad, t, "\n");
                    //     }
                    //
                    //     pretty = stream.end();
                    // }
                    pretty = setting.dump(2);

                    if (c == current) {
                        streamSelected << pretty << "\n";
                    }
                    streamAll << pretty << "\n";
                }
                c++;
            }
        }
    }

    logger.println("Selected preset:\n{}", streamSelected.end());
    logger.println("All presets:\n{}", streamAll.end());

    return true;
}

bool CommandExecutor::get_fixture_info(const std::vector<std::string>& args) {
    std::string cmd = args[0];
    std::vector<std::string> arr = {"name", "universe", "address", "highlight", "config"};
    if (std::ranges::find(arr, cmd) != arr.end() || cmd == "preset") {
        if (cmd == "preset") cmd = "presetIndex";
        logger.debug("{} exists in array", cmd);
        uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
        client->run(JSONtemp::stringify("getfixture", "id", id, "value", cmd), false);
        return true;
    }
    return false;
}

struct FixtureConfig {
    int id{};
    std::string name;
    std::string type;
    int universe{};
    int address{};
    int presetIndex{};
    int numPresets{};
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    FixtureConfig, id, name, type, universe, address, presetIndex, numPresets
)

bool CommandExecutor::get_fixture_config(const std::vector<std::string>& args) {
    uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
    auto ret = client->runStr(JSONtemp::stringify("getfixture", "id", id, "value", "config"), false);
    using json = nlohmann::json;

    json data = json::parse(ret);

    json respjson = json::parse(data["response"].get<std::string>());
    auto cfg = respjson["value"].get<FixtureConfig>();

    logger.println("Fixture '{}' info:", cfg.name);
    logger.println(" - universe: {}", cfg.universe);
    logger.println(" - address: {}", cfg.address);
    logger.println(" - selected preset: {}", cfg.presetIndex);
    logger.println(" - number of presets: {}", cfg.numPresets);
    logger.println(" - type: '{}'", cfg.type);
    return true;
}

bool CommandExecutor::set_fixture_info(const std::vector<std::string> & args) {
    std::string cmd = args[0];
    std::unordered_map<std::string, std::string> mapp;
    mapp["setname"] ="name";
    mapp["setuniverse"] = "universe";
    mapp["setaddress"] = "address";
    mapp["setpreset"] = "presetIndex";

    if (mapp.contains(cmd)) {
        logger.debug("{} exists in array", cmd);
        uint32_t id = args.size() == 3 ? std::stoi(args[2]) : 0;
        // client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], args[1]), false);
        if (args.size() >= 2) {
            // auto test = JSONtemp::stringify("setfixture", "id", id, mapp[cmd], cmd == "setname" ? args[1] : std::stoi(args[1]));
            if (cmd == "setname") {
                client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], args[1]), false);

            }
            else {
                client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], std::stoi(args[1])), false);
            }
            // client->run(JSONtemp::stringify("setfixture", mapp[cmd], args[1]), false);
        }
        else {
            // client->run(JSONtemp::stringify(mapp[cmd]), false);
        }
        return true;
    }

    return false;
}


bool CommandExecutor::setwifi(const std::vector<std::string>& args) {
    // for (auto&& arg : args) {
    //     std::cout << arg << std::endl;
    // }
    nlohmann::json r;
    r["request"] = "wifi";
    for (int i = 1; i < args.size(); i++) {
        if ((args.size() - i % 2) == 0) {
            logger.error("Invalid argument number for setwifi");
            return false;
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
    return true;
    // client->run(JSONtemp::stringify("wifi", "ssid", sp[1], "password", sp[2]));
}
