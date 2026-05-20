//
// Created by bobi on 10. 05. 26.
//

#include "CommandExecutor.h"
#include "Utils/Text/String.h"
#include "JSONtemp.h"
#include <nlohmann/json.hpp>

CommandExecutor::CommandExecutor(SharedState& state)
    : log("Command"), m_sharedState(state) {
    // log.toggleScope();

    bindCommands();
}

bool CommandExecutor::resolveCommand(const std::string& line) {
    auto args = Utils::String::split(Utils::String::normalize_spaces(line), " ");
    const std::string& cmd = args.empty() ? "" : args[0];

    bool result = false;

    auto anytime = m_cmdList.anytime(args);
    if (anytime) {
        result = anytime.value()(args);
    }
    else {
        if (!selected)
        {
            if (cmd.empty()) {
                result = list(args);
            }
            else if (auto fun = m_cmdList.unselected(args)) {
                result = fun.value()(args);
            }
            else {
                log.error("Invalid command: {}", line);
            }
        }
        else {
            if (cmd.empty()) {
                result = get_fixture_config();
            }
            else if (auto fun = m_cmdList.selected(args)) {
                result = fun.value()(args);
            }
            else {
                log.error("Invalid command: {}", line);
            }
        }
    }

    if (selected) {
        auto sclient = m_sharedState.getClientByIp(m_client->getIP());
        auto cname = sclient->descriptions[0].name;
        log.print("{}{}@{}>{} ", Utils::Font::colorYellow, cname, sclient->ip, Utils::Font::colorReset);
    }
    else {
        log.print("> ");
    }

    return result;
}

void CommandExecutor::bindCommands() {
    m_cmdList.registerGroup("Generic");

    m_cmdList.registerCommand(Command("exit", "Exits the program", "exit",
[this](const std::vector<std::string>& args) {
        return exit(args);
    }), Command::Usable::ANYTIME);

    m_cmdList.registerCommand(Command("help", "Prints help", "help",
    [this](const std::vector<std::string>& args) {
        return help(args);
    }), Command::Usable::ANYTIME);

    m_cmdList.registerCommand(Command("list", "Lists available items", "list or <ENTER>",
    [this](const std::vector<std::string>& args) {
        return this->list();
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommand(Command("select", "Select an item", "select <number>",
    [this](const std::vector<std::string>& args) {
        return select(args);
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommandNoname(Command("<id>", "Select fixture by id", "<id>",
    [](const std::vector<std::string>& args) {
        return Utils::String::isInt(args[0]);
    },
    [this](const std::vector<std::string>& args) {
        return select(args);
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommandNoname(Command("<name>", "Select fixture by name", "<name>",
    [this](const std::vector<std::string>& args) {
        return !(args.empty() || !m_sharedState.getClientByName(args[0]));
    },
    [this](const std::vector<std::string>& args) {
        return selectName(args);
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommand(Command("config", "Prints all config", "config",
    [this](const std::vector<std::string>& args) {
        return all_config();
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommand(Command(std::vector<std::string>{"back", "b"}, "Return to previous menu", "back",
    [this](const std::vector<std::string>& args) {
        return deselect();
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("reboot", "Reboot device", "reboot",
    [this](const std::vector<std::string>& args) {
        return reboot(args);
    }), Command::Usable::SELECTED);

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

    m_cmdList.registerGroup("Engine config");

    // ---------- selected commands ----------
    m_cmdList.registerCommand(Command("wifianimation", "Configure WiFi animation task", "wifianimation <true/false>?",
    task_config_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("serialprint", "Print or configure serial print", "serialprint <true/false>?",
    task_config_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("wirelessprint", "Print or configure wireless print", "wirelessprint <true/false>?",
    task_config_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("setwifi", "Configure WiFi settings", "setwifi ssid <value> pass <value>",
    [this](const std::vector<std::string>& args) {
        return setwifi(args);
    }), Command::Usable::SELECTED);


    m_cmdList.registerGroup("Engine info");

    // ---------- grouped info commands ----------
    m_cmdList.registerCommand(Command("describe", "Show info", "describe",
    get_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("fixtures", "Show fixtures info", "fixtures",
    get_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("inputs", "Show inputs info", "inputs",
    get_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("presets", "Show presets info", "presets",
    [this](const std::vector<std::string>& args) {
        return presets(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("ip", "Show IP info", "",
    [this](const std::vector<std::string>& args) {
        m_client->run(JSONtemp::stringify(args[0]), false);
        return true;
    }), Command::Usable::SELECTED);


    m_cmdList.registerGroup("Fixture info");

    // ---------- fixture getters ----------
    m_cmdList.registerCommand(Command("name", "Get fixture name", "name <number=0>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("universe", "Get fixture universe", "universe <number=0>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("address", "Get fixture address", "address <number=0>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("preset", "Get fixture preset", "preset <number=0>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("highlight", "Highlight fixture", "highlight <number=0>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("config", "Get full fixture config", "config <number=0>?",
    [this](const std::vector<std::string>& args) {
        return get_fixture_config(args);
    }), Command::Usable::SELECTED);


    m_cmdList.registerGroup("Fixture config");

    // ---------- fixture setters ----------
    m_cmdList.registerCommand(Command("setname", "Set fixture name", "setname <string> <number=0>?",
    set_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("setuniverse", "Set fixture universe", "setuniverse <number> <number=0>?",
    set_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("setaddress", "Set fixture address", "setaddress <number> <number=0>?",
    set_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("setpreset", "Set fixture preset", "setpreset <number> <number=0>?",
    set_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.pushGroup();
}

bool CommandExecutor::exit(const std::vector<std::string>&) {
    log.debug("Exiting");
    if (m_client) m_client.reset();
    ESPClient::stop();
    exitRequest = true;
    return true;
}

bool CommandExecutor::help(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        auto info = m_cmdList.get(args[1]);
        if (info) {
            log.println(" Help for command {}", info.value()->name);
            log.println(" - description: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info.value()->desc, Utils::Font::colorReset);
            log.println(" - usage: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info.value()->usage, Utils::Font::colorReset);
            return true;
        }
        log.println("Command {} does not exist", args[1]);
        return false;
    }

    for (auto& group : m_cmdList.getGroups()) {
        bool found = false;
        for (auto& cmds : group.commands) {

            if (cmds->isUsable(Command::Usable::ANYTIME) || cmds ->isUsable(selected? Command::Usable::SELECTED : Command::Usable::UNSELECTED)) {
                if (!found) {
                    log.println("{}", group.name);
                    found = true;
                }
                log.println(
                    " - {}: {}{}{}{}",
                    cmds->name,
                    Utils::Font::colorByRGB(100, 100, 100),
                    Utils::Font::colorItalic,
                    cmds->desc,
                    Utils::Font::colorReset
                );
            }
        }
    }
    return true;
}

bool CommandExecutor::reboot(const std::vector<std::string>&) {
    m_client->run(JSONtemp::stringify("reboot"));
    m_client.reset();
    selected = false;
    return true;
}

bool CommandExecutor::list(const std::vector<std::string>&) {
    log.println("Fixtures online:");
    int j = 0;
    for (auto clientinfo : m_sharedState.getClients()) {
        Utils::Text::Stream stream;
        stream << clientinfo->descriptions[0].name;
        for (int i = 1; i < clientinfo->descriptions.size(); i++) {
            stream << ", " << clientinfo->descriptions[i].name;
        }
        log.print(" - {}: {} {}({}){}", j++, stream.end(), Utils::Font::colorItalic, clientinfo->ip, Utils::Font::colorReset);
        log.println("{}{} v{}{}", Utils::Font::colorGreen, Utils::Font::colorItalic, clientinfo->version, Utils::Font::colorReset);
    }
    return true;
}

bool CommandExecutor::all_config(const std::vector<std::string>&) {
    const auto& allclients = m_sharedState.getClients();
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
    const auto size = m_sharedState.getClients().size();
    if (i >= size) {
        log.error("Index out of bounds, size: {}, index: {}", size, args[0]);
    }
    else {
        // selectIndex(i);
        const auto ip = m_sharedState.getClients()[i]->ip;
        log.debug("Selected: {} with ip {}", i, ip);
        selected = true;
        m_client = std::make_unique<ESPClient>(ip, m_sharedState);
    }
    return true;
        // log.error("Required inputs: 2, given: {}", args.size());
}

bool CommandExecutor::selectName(const std::vector<std::string>& args) {
    auto c = m_sharedState.getClientByName(args[0]);
    selected = true;
    m_client = std::make_unique<ESPClient>(c->ip, m_sharedState);

    return true;
}

bool CommandExecutor::selectIP(const std::vector<std::string>& args) {
    return true;
}


bool CommandExecutor::deselect(const std::vector<std::string>& args) {
    m_client.reset();
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
        log.debug("{} exists in array", cmd);
        if (args.size() == 2) {
            bool val = args[1] == "true" ? true : false;
            m_client->run(JSONtemp::stringify(mapp[cmd], "value", val), false);
        }
        else {
            m_client->run(JSONtemp::stringify(mapp[cmd]), false);
        }
        return true;
    }

    return false;
}

bool CommandExecutor::get_info(const std::vector<std::string>& args) {
    std::string cmd = args[0];
    log.debug("{} exists in array", cmd);
    m_client->run(JSONtemp::stringify(args[0]), true);
    return true;
}

bool CommandExecutor::presets(const std::vector<std::string>& args) {
    auto response = m_client->runStr(JSONtemp::stringify(args[0]));
    if (response.empty()) {
        return false;
    }

    log.println("Listing presets: ");
    Utils::Text::Stream streamSelected;
    Utils::Text::Stream streamAll;

    using nlohmann::json;
    json respj = json::parse(response);
    std::string respdata = respj["response"];
    json j = json::parse(respdata);

    for (const auto& preset : j["presets"]) {
        for (auto& [key, value] : preset.items()) {
            log.println("Fixture name: {}", key);

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

    log.println("Selected preset:\n{}", streamSelected.end());
    log.println("All presets:\n{}", streamAll.end());

    return true;
}

bool CommandExecutor::get_fixture_info(const std::vector<std::string>& args) {
    std::string cmd = args[0];
    std::vector<std::string> arr = {"name", "universe", "address", "highlight", "config"};
    if (std::ranges::find(arr, cmd) != arr.end() || cmd == "preset") {
        if (cmd == "preset") cmd = "presetIndex";
        log.debug("{} exists in array", cmd);
        uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
        m_client->run(JSONtemp::stringify("getfixture", "id", id, "value", cmd), false);
        return true;
    }
    return false;
}

struct FixtureConfig {
    int id = -1;
    std::string name = "";
    std::string type = "";
    int universe = -1;
    int address = -1;
    int presetIndex = -1;
    int numPresets = -1;

    FixtureConfig() = default;
    FixtureConfig(const nlohmann::json& v) {
        // auto& v = json["value"];

        id = v.value("id", -1);
        name = v.value("name", "");
        type = v.value("type", "");
        universe = v.value("universe", 0);
        address = v.value("address", 0);
        presetIndex = v.value("presetIndex", -1);
        numPresets = v.value("numPresets", -1);
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    FixtureConfig, id, name, type, universe, address, presetIndex, numPresets
)

bool CommandExecutor::get_fixture_config(const std::vector<std::string>& args) {
    uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
    auto ret = m_client->runStr(JSONtemp::stringify("getfixture", "id", id, "value", "config"), false);
    using json = nlohmann::json;

    json data = json::parse(ret);

    json respjson = json::parse(data["response"].get<std::string>());
    auto v = respjson["value"];
    FixtureConfig cfg(v);

    log.println("Fixture '{}' info:", cfg.name);
    log.println(" - universe: {}", cfg.universe);
    log.println(" - address: {}", cfg.address);
    log.println(" - selected preset: {}", cfg.presetIndex);
    log.println(" - number of presets: {}", cfg.numPresets);
    log.println(" - type: '{}'", cfg.type);
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
        log.debug("{} exists in array", cmd);
        uint32_t id = args.size() == 3 ? std::stoi(args[2]) : 0;
        // client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], args[1]), false);
        if (args.size() >= 2) {
            // auto test = JSONtemp::stringify("setfixture", "id", id, mapp[cmd], cmd == "setname" ? args[1] : std::stoi(args[1]));
            if (cmd == "setname") {
                m_client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], args[1]), false);

            }
            else {
                m_client->run(JSONtemp::stringify("setfixture", "id", id, mapp[cmd], std::stoi(args[1])), false);
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
            log.error("Invalid argument number for setwifi");
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
    // log.println(r.dump());
    m_client->run(r.dump()); // TODO: untested
    return true;
    // client->run(JSONtemp::stringify("wifi", "ssid", sp[1], "password", sp[2]));
}
