//
// Created by bobi on 10. 05. 26.
//

#include "CommandExecutor.h"
#include "Utils/Text/String.h"
#include "JSONtemp.h"
#include "Theme.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

CommandExecutor::CommandExecutor(SharedState& state, Display& display)
    : log("Command"), m_sharedState(state), m_display(display) {
    bindCommands();
}

void CommandExecutor::run() {
    m_display.run([this](const std::string& line) {
        resolveCommand(line);
        return shouldQuit();
    });
}

bool CommandExecutor::resolveCommand(const std::string& line) {
    auto args = Utils::String::split(Utils::String::normalize_spaces(line), " ");
    const std::string& cmd = args.empty() ? "" : args[0];

    bool result = false;

    auto anytime = m_cmdList.anytime(args);
    if (anytime) {
        result = anytime.value()(args);
        if(shouldQuit())
        {
            return true;
        }
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

    updatePrompt();
    return result;
}

void CommandExecutor::bindCommands() {
    m_cmdList.registerGroup("Generic");

    m_cmdList.registerCommand(Command("exit", "Exits the program", "exit",
[this](const std::vector<std::string>& args) {
        return exit(args);
    }), Command::Usable::ANYTIME);

    m_cmdList.registerCommand(Command("refresh", "Clear all clients and wait for heartbeat", "refresh",
    [this](const std::vector<std::string>& args) {
        return refresh(args);
    }), Command::Usable::ANYTIME);

    m_cmdList.registerCommand(Command("help", "Prints help", "help",
    [this](const std::vector<std::string>& args) {
        return help(args);
    }), Command::Usable::ANYTIME);

    m_cmdList.registerCommand(Command("list", "Lists available items", "list or <ENTER>",
    [this](const std::vector<std::string>& args) {
        return this->list();
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommand(Command("setuniverse", "Set fixture universe(s)", "setuniverse <id,id,...> <universe,universe,...>",
    [this](const std::vector<std::string>& args) {
        return setuniverse(args);
    }), Command::Usable::UNSELECTED);

    m_cmdList.registerCommand(Command("setpreset", "Set fixture preset(s)", "setpreset <id,id,...> <preset,preset,...>",
    [this](const std::vector<std::string>& args) {
        return setpreset(args);
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
        return !(args.empty());
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

    auto get_fixture_info_cmd = [this](const std::vector<std::string>& args) {
        return get_fixture_info(args);
    };

    m_cmdList.registerGroup("Engine config");

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

    m_cmdList.registerCommand(Command("describe", "Show engine & wifi state", "describe",
    [this](const std::vector<std::string>& args) {
        return describe(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("fixtures", "Show all fixtures", "fixtures",
    [this](const std::vector<std::string>& args) {
        return fixtures(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("inputs", "Show inputs info", "inputs",
    [this](const std::vector<std::string>& args) {
        return inputs(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("presets", "Show fixture presets", "presets",
    [this](const std::vector<std::string>& args) {
        return presets(args);
    }), Command::Usable::SELECTED);


    m_cmdList.registerGroup("Fixture info & config");

    m_cmdList.registerCommand(Command("name", "Get or set fixture name", "name <string>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("universe", "Get or set fixture universe", "universe <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("address", "Get or set fixture address", "address <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("preset", "Get or set fixture preset", "preset <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("highlight", "Toggle highlight", "highlight",
    [this](const std::vector<std::string>& args) {
        m_client->run(JSONtemp::stringify("setfixture", "id", m_client->clientID(), "highlight", true));
        return true;
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("config", "Get full fixture config", "config",
    [this](const std::vector<std::string>& args) {
        return get_fixture_config(args);
    }), Command::Usable::SELECTED);

    m_cmdList.pushGroup();
}

void CommandExecutor::updatePrompt() {
    if (selected) {
        auto sclient = m_client->getClientInfo();
        auto prompt = std::format("{}{}{}:{}{}{}{}{} {}",
            Theme::name(), sclient.description.name, Theme::r(),
            Theme::dim(), sclient.description.type, Theme::r(),
            Theme::name(), ">", Theme::r());
        m_display.setPrompt(prompt);
    } else {
        m_display.setPrompt("> ");
    }
}

std::string CommandExecutor::getSelectedName() const {
    return m_client ? m_client->getClientInfo().description.name : "";
}

std::string CommandExecutor::getSelectedType() const {
    return m_client ? m_client->getClientInfo().description.type : "";
}

int CommandExecutor::getSelectedNumLeds() const {
    return m_client ? m_client->getClientInfo().description.num_leds : 0;
}

std::string CommandExecutor::fetchFixtureConfigJson() {
    if (!m_client) return "";
    return m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", "config"));
}

std::string CommandExecutor::fetchDescribeJson() {
    if (!m_client) return "";
    return m_client->runStr(JSONtemp::stringify("describe"));
}

bool CommandExecutor::exit(const std::vector<std::string>&) {
    log.debug("Exiting");
    if (m_client) m_client.reset();
    ESPClient::stop();
    exitRequest = true;
    m_display.quit();
    return true;
}

bool CommandExecutor::help(const std::vector<std::string>& args) {
    if (args.size() == 2) {
        auto info = m_cmdList.get(args[1]);
        if (info) {
            log.println(" Help for command {}", info.value()->name);
            log.println(" - description: {}{}{}", Theme::dim(), info.value()->desc, Theme::r());
            log.println(" - usage: {}{}{}", Theme::dim(), info.value()->usage, Theme::r());
            return true;
        }
        log.println("Command {} does not exist", args[1]);
        return false;
    }

    for (auto& group : m_cmdList.getGroups()) {
        bool found = false;
        for (auto& cmds : group.commands) {
            if (cmds->isUsable(Command::Usable::ANYTIME) || cmds->isUsable(selected ? Command::Usable::SELECTED : Command::Usable::UNSELECTED)) {
                if (!found) {
                    log.println("{}{}{}", Theme::heading(), group.name, Theme::r());
                    found = true;
                }
                log.println(" - {}: {}{}{}",
                    cmds->name,
                    Theme::dim(),
                    cmds->desc,
                    Theme::r()
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
    struct Entry { int id; const ClientInfo* info; };
    std::vector<Entry> entries;
    int idx = 0;
    for (const auto& info : m_sharedState.getClients()) {
        entries.push_back({idx++, &info});
    }

    std::vector<std::pair<std::string, std::vector<Entry>>> groups;
    std::unordered_map<std::string, size_t> ipIndex;
    for (auto& e : entries) {
        auto it = ipIndex.find(e.info->ip);
        if (it == ipIndex.end()) {
            ipIndex[e.info->ip] = groups.size();
            groups.push_back({e.info->ip, {e}});
        } else {
            groups[it->second].second.push_back(e);
        }
    }

    log.println("Devices online ({})", groups.size());

    for (const auto& [ip, fixtures] : groups) {
        auto& first = *fixtures[0].info;
        log.println("  {}{}{}  {}v{}{}", Theme::ip(), ip, Theme::r(), Theme::ver(), first.version, Theme::r());

        for (const auto& [id, info] : fixtures) {
            log.print("    [{}] {}{}{}", id, Theme::name(), info->description.name, Theme::r());
            if (!info->description.type.empty()) {
                log.print("  {}{}{}", Theme::dim(), info->description.type, Theme::r());
            }
            if (info->description.num_leds != 0) {
                log.print("  {}({} leds){}", Theme::dim(), info->description.num_leds, Theme::r());
            }
            log.println("");
        }
    }
    return true;
}

bool CommandExecutor::all_config(const std::vector<std::string>&) {
    const auto& allclients = m_sharedState.getClients();
    for (int i = 0; i < allclients.size(); i++) {
        select({std::to_string(i)});
        get_fixture_config();
        deselect();
    }
    return true;
}

bool CommandExecutor::setuniverse(const std::vector<std::string>& args) {
    auto ids = Utils::String::split(args[1], ",");
    auto unis = Utils::String::split(args[2], ",");
    if(unis.size() == 1 || unis.size() == ids.size())
    {
        for(int i = 0; i < ids.size(); i++)
        {
            auto& id = ids[i];
            auto uni = unis[i % unis.size()];
            select({id});
            get_fixture_info(std::vector<std::string>{"universe", uni});
            deselect();
        }
    }
    return true;
}

bool CommandExecutor::setpreset(const std::vector<std::string>& args) {
    auto ids = Utils::String::split(args[1], ",");
    auto unis = Utils::String::split(args[2], ",");
    if(unis.size() == 1 || unis.size() == ids.size())
    {
        for(int i = 0; i < ids.size(); i++)
        {
            auto& id = ids[i];
            auto uni = unis[i % unis.size()];
            select(std::vector<std::string>{id});
            get_fixture_info(std::vector<std::string>{"preset", uni});
            deselect();
        }
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
        auto it = m_sharedState.getClients().begin();
        std::advance(it, i);
        const auto& cl = *it;
        log.debug("Selected: {} with ip {}", i, (*it).ip);
        selected = true;
        m_client = std::make_unique<ESPClient>(cl, m_sharedState);
    }
    return true;
}

bool CommandExecutor::selectName(const std::vector<std::string>& args) {
    selected = true;
    m_client = std::make_unique<ESPClient>(m_sharedState.getClientByName(args[0]), m_sharedState);
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

    if (!mapp.contains(cmd)) return false;

    std::string resp;
    if (args.size() == 2) {
        bool v = args[1] == "true";
        resp = m_client->runStr(JSONtemp::stringify(mapp[cmd], "value", v));
    } else {
        resp = m_client->runStr(JSONtemp::stringify(mapp[cmd]));
    }
    if (resp.empty()) return false;

    using json = nlohmann::json;
    json data;
    try { data = json::parse(resp); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    bool current = data.value("val", false);
    auto boolCol = current ? Theme::ok() : Theme::err();

    if (args.size() == 2) {
        log.println("{}Set {}{} to {}{}{}", Theme::lbl(), cmd, Theme::r(), boolCol, current, Theme::r());
    } else {
        log.println("{}{}: {}{}{}", Theme::lbl(), cmd, boolCol, current, Theme::r());
    }
    return true;
}

bool CommandExecutor::get_info(const std::vector<std::string>& args) {
    m_client->run(JSONtemp::stringify(args[0]));
    return true;
}

bool CommandExecutor::presets(const std::vector<std::string>& args) {
    auto resp = m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", "presets"));
    if (resp.empty()) return false;

    using nlohmann::json;
    json data;
    try { data = json::parse(resp); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    auto val = data["val"];
    int sel = val.value("selected", -1);
    auto& groups = val["presets"]["groups"];

    log.println("{}Presets{} {}(selected: {}){}", Theme::heading(), Theme::r(), Theme::dim(), sel, Theme::r());
    int i = 0;
    for (const auto& group : groups) {
        std::string gname = group.value("name", "unnamed");
        bool isCurrent = (i == sel);

        if (isCurrent) {
            log.print("  {}[{}] \"{}\"{}", Theme::ok(), i, gname, Theme::r());
        } else {
            log.print("  [{}] {}\"{}\"{}", i, Theme::dim(), gname, Theme::r());
        }

        for (const auto& setting : group["settings"]) {
            log.print("  {}{}{}", Theme::val(), setting.dump(), Theme::r());
        }
        log.println("");
        i++;
    }
    return true;
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
        id = v.value("id", -1);
        name = v.value("name", "");
        type = v.value("type", "");
        universe = v.value("universe", -1);
        address = v.value("address", -1);
        presetIndex = v.value("presetIndex", -1);
        numPresets = v.value("numPresets", -1);
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    FixtureConfig, id, name, type, universe, address, presetIndex, numPresets
)


bool CommandExecutor::describe(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify("describe"));
    if (resp.empty()) return false;

    using nlohmann::json;
    json data;
    try { data = json::parse(resp); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    log.println("{}Engine{}", Theme::heading(), Theme::r());
    log.println("  {}version:{}       {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("version", "?"), Theme::r());
    log.println("  {}serial print:{}  {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("serial_report_task", false), Theme::r());
    log.println("  {}wifi print:{}    {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("wireless_report_task", false), Theme::r());
    log.println("  {}wifi anim:{}     {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("wifi_animation", false), Theme::r());

    if (data.contains("wifi"))
    {
        auto& wifi = data["wifi"];
        bool connected = wifi.value("connected", false);
        auto connCol = connected ? Theme::ok() : Theme::err();

        log.println("{}Wifi{}", Theme::heading(), Theme::r());
        log.println("  {}connected:{}  {}{}{}", Theme::lbl(), Theme::r(), connCol, connected, Theme::r());
        log.println("  {}ip:{}         {}{}{}", Theme::lbl(), Theme::r(), Theme::ip(), wifi.value("local_ip", "?"), Theme::r());
        log.println("  {}rssi:{}       {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), wifi.value("rssi", 0), Theme::r());
        log.println("  {}ssid:{}       {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("ssid", "?"), Theme::r());
        log.println("  {}password:{}   {}{}{}", Theme::lbl(), Theme::r(), Theme::val(), data.value("password", "?"), Theme::r());
    }

    return true;
}

bool CommandExecutor::fixtures(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify("fixtures"));
    if (resp.empty()) return false;

    using nlohmann::json;
    json data;
    try { data = json::parse(resp); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    log.println("{}Fixtures{}", Theme::heading(), Theme::r());
    for (const auto& fixture : data["fixtures"])
    {
        FixtureConfig cfg(fixture);
        log.println("  [{}] {}{}{}  {}{}{}",
            cfg.id,
            Theme::name(), cfg.name, Theme::r(),
            Theme::dim(), cfg.type, Theme::r());
        log.println("      {}uni:{}  {}{}{}  {}addr:{}  {}{}{}  {}preset:{}  {}{}/{}{}",
            Theme::lbl(), Theme::r(), Theme::val(), cfg.universe, Theme::r(),
            Theme::lbl(), Theme::r(), Theme::val(), cfg.address, Theme::r(),
            Theme::lbl(), Theme::r(), Theme::val(), cfg.presetIndex, cfg.numPresets, Theme::r());
    }
    return true;
}

bool CommandExecutor::inputs(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify("inputs"));
    if (resp.empty()) return false;

    using nlohmann::json;
    json data;
    try { data = json::parse(resp); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    log.println("{}Inputs{} ({})", Theme::heading(), Theme::r(), data.value("num_inputs", 0));
    for (const auto& input : data["inputs"])
    {
        log.println("  {}type:{}  {}{}{}  {}universe:{}  {}{}{}",
            Theme::lbl(), Theme::r(), Theme::val(), input.value("type", "?"), Theme::r(),
            Theme::lbl(), Theme::r(), Theme::val(), input.value("universe", 0), Theme::r());
    }
    return true;
}

bool CommandExecutor::inbytes(const std::vector<std::string>& args)
{
    log.error("inbytes command not supported in this firmware version");
    return false;
}


bool CommandExecutor::get_fixture_config(const std::vector<std::string>& args) {
    auto ret = m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", "config"));
    if (ret.empty()) return false;

    using json = nlohmann::json;
    json data;
    try { data = json::parse(ret); }
    catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
    if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

    auto v = data["val"];
    FixtureConfig cfg(v);

    log.println("{}{}{}  {}{}{}",
        Theme::name(), cfg.name, Theme::r(),
        Theme::dim(), cfg.type, Theme::r());

    log.println("  {}universe: {}{}{}  {}address: {}{}{}  {}preset: {}{}/{}{}",
        Theme::lbl(), Theme::val(), cfg.universe, Theme::r(),
        Theme::lbl(), Theme::val(), cfg.address, Theme::r(),
        Theme::lbl(), Theme::val(), cfg.presetIndex, cfg.numPresets, Theme::r());

    if (v.contains("footprint"))
    {
        log.print("  {}footprint: {}{}{}", Theme::lbl(), Theme::val(), v["footprint"].get<int>(), Theme::r());
    }

    auto nleds = m_client->getClientInfo().description.num_leds;
    if (nleds != 0)
    {
        log.print("  {}leds: {}{}{}", Theme::lbl(), Theme::val(), nleds, Theme::r());
    }
    log.println("");

    if (v.contains("outputs"))
    {
        log.println("  {}outputs:{}", Theme::lbl(), Theme::r());
        for (const auto& out : v["outputs"])
        {
            std::string otype = out.value("type", "?");
            int pin = out.value("hardware_pin", -1);
            int leds = out.value("num_leds", 0);
            int groups = out.value("num_groups", 0);

            log.println("    {}type: {}{}{}  {}pin: {}{}{}  {}leds: {}{}{}  {}groups: {}{}{}",
                Theme::lbl(), Theme::val(), otype, Theme::r(),
                Theme::lbl(), Theme::val(), pin, Theme::r(),
                Theme::lbl(), Theme::val(), leds, Theme::r(),
                Theme::lbl(), Theme::val(), groups, Theme::r());
        }
    }
    return true;
}

bool CommandExecutor::get_fixture_info(const std::vector<std::string> & args) {
    std::string cmd = args[0];
    std::string label = cmd;
    if(cmd == "preset")
    {
        cmd = "presetIndex";
    }

    if (args.size() == 1)
    {
        auto resp = m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", cmd));
        if (resp.empty()) return false;

        using json = nlohmann::json;
        json data;
        try { data = json::parse(resp); }
        catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
        if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

        auto v = data["val"];
        if (v.is_string())
            log.println("{}{}: {}{}{}", Theme::lbl(), label, Theme::val(), v.get<std::string>(), Theme::r());
        else
            log.println("{}{}: {}{}{}", Theme::lbl(), label, Theme::val(), v.dump(), Theme::r());
    }
    else
    {
        std::string resp;
        if (cmd == "name") {
            resp = m_client->runStr(JSONtemp::stringify("setfixture", "id", m_client->clientID(), cmd, args[1]));
        } else {
            resp = m_client->runStr(JSONtemp::stringify("setfixture", "id", m_client->clientID(), cmd, std::stoi(args[1])));
        }
        if (resp.empty()) return false;

        using json = nlohmann::json;
        json data;
        try { data = json::parse(resp); }
        catch (const json::parse_error& e) { log.error("JSON parse error: {}", e.what()); return false; }
        if (data.contains("err")) { log.error("{}", data["err"].get<std::string>()); return false; }

        log.println("{}Set {}{} to {}{}{}", Theme::lbl(), label, Theme::r(), Theme::val(), args[1], Theme::r());
    }
    return true;
}


bool CommandExecutor::setwifi(const std::vector<std::string>& args) {
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

    std::string oldIp = m_client->getIP();
    std::string clientName = m_client->getClientInfo().description.name;

    log.println("{}Sending wifi config...{}", Theme::lbl(), Theme::r());
    m_client->runStr(r.dump());

    m_client.reset();
    selected = false;
    m_sharedState.removeClientsByIp(oldIp);

    log.println("{}Waiting for '{}{}{}' to reconnect...{}", Theme::lbl(), Theme::name(), clientName, Theme::lbl(), Theme::r());

    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (m_sharedState.hasClientWithName(clientName)) {
            auto& info = m_sharedState.getClientByName(clientName);
            log.println("{}Reconnected at {}{}{}", Theme::ok(), Theme::ip(), info.ip, Theme::r());
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    log.error("Timed out waiting for '{}' to reconnect", clientName);
    return false;
}

bool CommandExecutor::refresh(const std::vector<std::string>&) {
    if (m_client) {
        m_client.reset();
        selected = false;
    }

    m_sharedState.clearAll();

    log.println("{}Cleared all clients, waiting for heartbeat...{}", Theme::lbl(), Theme::r());

    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (!m_sharedState.getClients().empty()) {
            log.println("{}Found {} client(s){}", Theme::ok(), m_sharedState.getClients().size(), Theme::r());
            return list();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    log.error("No clients found");
    return false;
}
