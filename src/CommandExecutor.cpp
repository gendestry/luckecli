//
// Created by bobi on 10. 05. 26.
//

#include "CommandExecutor.h"
#include "Utils/Text/String.h"
#include "JSONtemp.h"
#include <iostream>
#include <nlohmann/json.hpp>

CommandExecutor::CommandExecutor()
    : logger("Command") {
    logger.toggleScope();

    bindcommands();
}

bool CommandExecutor::resolveCommand(const std::string& line) {
    auto args = Utils::String::split(Utils::String::normalize_spaces(line), " ");
    const std::string& cmd = args.empty() ? "" : args[0];

    if (!selected)
    {
        if (commands.contains(cmd) && commands[cmd].func(args)) {

        }
        else if (cmd.empty()) {
            list(args);
        }
        else if (Utils::String::isInt(cmd)) {
            select(args);
        }
    }
    else {
        if (commandsSelected.contains(cmd) && commandsSelected[cmd].func(args)) {

        }
        else {
            logger.error("Invalid command: {}", line);
        }
    }

    if (selected) {
        logger.print("{}{}@{}>{} ", Utils::Font::colorYellow, heartbeat[client->getIP()].descriptions[0].name, client->getIP(), Utils::Font::colorReset);
    }
    else {
        logger.print("> ");
    }

    return false;
}

void CommandExecutor::bindcommands() {
    commands["exit"] = CmdInfo{
        .name = "exit",
        .desc ="Exits the program",
        .usage = "exit",
        .func = [this](const std::vector<std::string>& args) {
            return exit(args);
        }
    };
    commands["help"] = CmdInfo{
        .name = "help",
        .desc ="Prints help",
        .usage = "help",
        .func = [this](const std::vector<std::string>& args) {
            return help(args);
        }
    };

    commands["list"] = CmdInfo{
        .name = "list",
        .desc = "Lists available items",
        .usage = "list or <ENTER>",
        .func = [this](const std::vector<std::string>& args) {
            return list(args);
        }
    };
    commands["select"] = CmdInfo{
        .name = "select",
        .desc = "Select an item",
        .usage = "select <number> or <number>",
        .func = [this](const std::vector<std::string>& args) {
            return select(args);
        }
    };

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

    commandsSelected["exit"] = commands["exit"];
    commandsSelected["help"] = commands["help"];

    commandsSelected["wifianimation"] = CmdInfo{
        .name = "wifianimation",
        .desc = "Configure WiFi animation task",
        .usage = "wifianimation <true/false>?",
        .func = task_config_cmd
    };

    commandsSelected["serialprint"] = CmdInfo{
        .name = "serialprint",
        .desc = "Print or configure serial print",
        .usage = "serialprint <true/false>?",
        .func = task_config_cmd
    };

    commandsSelected["wirelessprint"] = CmdInfo{
        .name = "wirelessprint",
        .desc = "Print or configure wireless print",
        .usage = "wirelessprint <true/false>?",
        .func = task_config_cmd
    };

    commandsSelected["setwifi"] = CmdInfo{
        .name = "setwifi",
        .desc = "Configure WiFi settings",
        .usage = "setwifi ssid <value> pass <value>",
        .func = [this](const std::vector<std::string>& args) {
            return setwifi(args);
        }
    };

    commandsSelected["back"] = CmdInfo{
        .name = "back",
        .desc = "Return to previous menu",
        .usage = "back",
        .func = [this](const std::vector<std::string>& args) {
            return deselect(args);
        }
    };

    commandsSelected["reboot"] = CmdInfo{
        .name = "reboot",
        .desc = "Reboot device",
        .usage = "reboot",
        .func = [this](const std::vector<std::string>& args) {
            return reboot(args);
        }
    };

    // ---------- grouped info commands ----------
    commandsSelected["describe"] = CmdInfo{ .name = "describe", .desc = "Show info",          .usage="describe", .func = get_info_cmd };
    commandsSelected["fixtures"] = CmdInfo{ .name = "fixtures", .desc = "Show fixtures info", .usage="fixtures", .func = get_info_cmd };
    commandsSelected["inputs"]   = CmdInfo{ .name = "inputs",   .desc = "Show inputs info",   .usage="inputs", .func = get_info_cmd };
    commandsSelected["presets"]  = CmdInfo{ .name = "presets",  .desc = "Show presets info",  .usage="presets", .func = [this](const std::vector<std::string>& args) {
        return presets(args);
    } };
    commandsSelected["ip"]       = CmdInfo{ .name = "ip",       .desc = "Show IP info",       .usage="", .func = [this](const std::vector<std::string>& args) {
        client->run(JSONtemp::stringify(args[0]), false);
        return true;
    } };

    // ---------- fixture getters ----------
    commandsSelected["name"]      = CmdInfo{ .name = "name",      .desc = "Get fixture name",      .usage="name <number=0>?",        .func = get_fixture_info_cmd };
    commandsSelected["universe"]  = CmdInfo{ .name = "universe",  .desc = "Get fixture universe",  .usage="universe <number=0>?",    .func = get_fixture_info_cmd };
    commandsSelected["address"]   = CmdInfo{ .name = "address",   .desc = "Get fixture address",   .usage="address <number=0>?",     .func = get_fixture_info_cmd };
    commandsSelected["preset"]    = CmdInfo{ .name = "preset",    .desc = "Get fixture preset",    .usage="preset <number=0>?",      .func = get_fixture_info_cmd };
    commandsSelected["highlight"] = CmdInfo{ .name = "highlight", .desc = "Highlight fixture",     .usage="highlight <number=0>?",   .func = get_fixture_info_cmd };
    commandsSelected["config"] = CmdInfo{ .name = "config", .desc = "Get full fixture config",     .usage="config <number=0>?", .func = [this](const std::vector<std::string>& args) {
        return get_fixture_config(args);
    } };

    // ---------- fixture setters ----------
    commandsSelected["setname"]      = CmdInfo{ .name = "setname",      .desc = "Set fixture name",      .usage="setname <string> <number=0>?",     .func = set_fixture_info_cmd };
    commandsSelected["setuniverse"]  = CmdInfo{ .name = "setuniverse",  .desc = "Set fixture universe",  .usage="setuniverse <number> <number=0>?", .func = set_fixture_info_cmd };
    commandsSelected["setaddress"]   = CmdInfo{ .name = "setaddress",   .desc = "Set fixture address",   .usage="setaddress <number> <number=0>?",  .func = set_fixture_info_cmd };
    commandsSelected["setpreset"]    = CmdInfo{ .name = "setpreset",    .desc = "Set fixture preset",    .usage="setpreset <number> <number=0>?",   .func = set_fixture_info_cmd };
}

bool CommandExecutor::exit(const std::vector<std::string>&) {
    logger.debug("Exiting");
    if (client) client.reset();
    ESPClient::stop();
    return true;
}

bool CommandExecutor::help(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        std::unique_ptr<CmdInfo> info;
        if (commands.contains(args[1])) {
            info = std::make_unique<CmdInfo>(commands[args[1]]);
        }
        else if (commandsSelected.contains(args[1])) {
            info = std::make_unique<CmdInfo>(commandsSelected[args[1]]);
        }

        if (info) {
            logger.println(" Help for command {}", info->name);
            logger.println(" - description: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info->desc, Utils::Font::colorReset);
            logger.println(" - usage: {}{}{}{}",Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, info->usage, Utils::Font::colorReset);
            return true;
        }
        logger.println("Command {} does not exist", args[1]);
        return false;
    }

    logger.println("Help:");
    for (auto& [_, cmdinfo] : selected ? commandsSelected : commands) {
        logger.println(" - {}: {}{}{}{}", cmdinfo.name, Utils::Font::colorByRGB(100, 100, 100), Utils::Font::colorItalic, cmdinfo.desc, Utils::Font::colorReset);
    }
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
    for (ClientInfo* clientinfo : heartbeat.getClients()) {
        Utils::Text::Stream stream;
        stream << clientinfo->descriptions[0].name;
        for (int i = 1; i < clientinfo->descriptions.size(); i++) {
            stream << ", " << clientinfo->descriptions[i].name;
        }
        logger.println(" - {}: {} {}({}){}", clientinfo->index, stream.end(), Utils::Font::colorItalic, clientinfo->ip, Utils::Font::colorReset);
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

bool CommandExecutor::get_fixture_config(const std::vector<std::string>& args) {
    std::string cmd = args[0];

    uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
    client->run(JSONtemp::stringify("getfixture", "id", id, "value", cmd), true);
    return true;
    return false;
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
