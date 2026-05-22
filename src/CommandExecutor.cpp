//
// Created by bobi on 10. 05. 26.
//

#include "CommandExecutor.h"
#include "Utils/Text/String.h"
#include "JSONtemp.h"
#include <nlohmann/json.hpp>
#include <iostream>

CommandExecutor::CommandExecutor(SharedState& state)
    : log("Command"), m_sharedState(state) {
    // log.toggleScope();

    bindCommands();
}

void CommandExecutor::run() {
    log.print("> ");
    std::string line;
    while (std::getline(std::cin, line)) {
        auto resolved = resolveCommand(line);
        if (shouldQuit()) {
            break;
        }
    }
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

    if (selected) {
        auto sclient = m_client->getClientInfo();
        auto cname = sclient->descriptions[sclient->selected].name;
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
    [this](const std::vector<std::string>& args) {
        return describe(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("fixtures", "Show fixtures info", "fixtures",
    [this](const std::vector<std::string>& args) {
        return fixtures(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("inputs", "Show inputs info", "inputs",
    [this](const std::vector<std::string>& args) {
        return inputs(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("presets", "Show presets info", "presets",
    [this](const std::vector<std::string>& args) {
        return presets(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("ip", "Show IP info", "",
    [this](const std::vector<std::string>& args) {
        m_client->run(JSONtemp::stringify(args[0]), false);
        return true;
    }), Command::Usable::SELECTED);


    m_cmdList.registerGroup("Fixture info & config");

    // ---------- fixture getters ----------
    m_cmdList.registerCommand(Command("name", "Get or set fixture name", "name <string>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("universe", "Get or set fixture universe", "universe <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("address", "Get or set fixture address", "address <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("preset", "Get or set fixture preset", "preset <number>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("highlight", "Get or set highlight", "highlight <boolean>?",
    get_fixture_info_cmd), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("config", "Get full fixture config", "config",
    [this](const std::vector<std::string>& args) {
        return get_fixture_config(args);
    }), Command::Usable::SELECTED);

    m_cmdList.registerCommand(Command("inbytes", "Get full dmx array", "inbytes",
    [this](const std::vector<std::string>& args) {
        return inbytes(args);
    }), Command::Usable::SELECTED);

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

    auto basicAppend = [this](Utils::Text::Stream& ss, const ClientInfo& info, int i){
        ss << Utils::Font::colorBlue << info.descriptions[i].name << Utils::Font::colorReset;
        if(info.descriptions[i].num_leds != 0)
        {
            ss << Utils::Font::colorByRGB(50,50,50) << " (" << std::to_string(info.descriptions[i].num_leds) << " leds)" << Utils::Font::colorReset;
        }
    };

    for (auto clientinfo : m_sharedState.getClients()) {
        Utils::Text::Stream stream;
        basicAppend(stream, *clientinfo, 0);
        for (int i = 1; i < clientinfo->descriptions.size(); i++) {
            stream << ", ";
            basicAppend(stream, *clientinfo, i);
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
        const auto cl = m_sharedState.getClients()[i];
        log.debug("Selected: {} with ip {}", i, cl->ip);
        selected = true;
        m_client = std::make_unique<ESPClient>(cl, m_sharedState);
    }
    return true;
        // log.error("Required inputs: 2, given: {}", args.size());
}

bool CommandExecutor::selectName(const std::vector<std::string>& args) {
    auto c = m_sharedState.getClientByName(args[0]);
    selected = true;
    m_client = std::make_unique<ESPClient>(c, m_sharedState);

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

    void applyOffset(int offset)
    {
        address += address == -1 ? 0 : offset; 
        presetIndex += presetIndex == -1 ? 0 : offset; 
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    FixtureConfig, id, name, type, universe, address, presetIndex, numPresets
)


bool CommandExecutor::describe(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify(args[0]), true);
    using nlohmann::json;
    
    json respj = json::parse(resp);
    std::string responseJsonStr = respj["response"];

    json engineJson = json::parse(responseJsonStr);

    {
        json settings = engineJson["engine"]["settings"];
        json wifi = engineJson["engine"]["wifi"];
        std::string version = settings.value("version", "unknown");
        std::string ssid = settings.value("ssid", "unknown");
        std::string password = settings.value("password", "unknown");

        int rssi = wifi.value("rssi", 0);
        std::string ip = wifi.value("local_ip", "HMMM");
        bool connected = wifi.value("connected", false);

        bool serial_report_task = settings.value("serial_report_task", true);
        bool wireless_report_task = settings.value("wireless_report_task", true);
        bool wifi_animation = settings.value("wifi_animation", true);

        log.println("Engine settings:");
        log.println(" - Version: {}", version);
        log.println(" - Serial print enabled: {}", serial_report_task);
        log.println(" - Wireless print enabled: {}", wireless_report_task);
        log.println(" - Wifi animation enabled: {}", wifi_animation);

        log.println("Wifi:");
        log.println(" - SSID: '{}'", ssid);
        log.println(" - Password: '{}'", password);
        log.println(" - Connected: {}", connected);
        log.println(" - IP: {}", ip);
        log.println(" - RSSI: {}", rssi);
    }

    json fixtures = engineJson["engine"]["fixture_handler"];
    json inputs = engineJson["engine"]["input_handler"];

    log.println("Inputs:");
    for (const auto& input : inputs["inputs"])
    {
        int id = input.value("id", 0);
        log.println(" - ID: {}", id);

        int universe = input.value("universe", 0);
        std::string type = input.value("type", "unknown");

        log.println("   - universe: {}", universe);
        log.println("   - type: {}", type);
    }

    log.println("Fixtures (num = {}):", fixtures.value("num_fixtures", 1));
    for (const auto& fixture : fixtures["fixtures"])
    {
        FixtureConfig cfg(fixture);
        log.println(" - ID: {}", cfg.id);
        log.println("   - name: '{}'", cfg.name);
        log.println("   - type: '{}'", cfg.type);
        log.println("   - universe: {}", cfg.universe);
        log.println("   - address: {}", cfg.address);
        log.println("   - selected preset: {}", cfg.presetIndex);
        log.println("   - number of presets: {}", cfg.numPresets);
    }

    return true;
}

bool CommandExecutor::fixtures(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify(args[0]), true);
    using nlohmann::json;
    
    json respj = json::parse(resp);
    // "response" contains a JSON string
    std::string responseJsonStr = respj["response"];

    // Parse inner JSON
    json engineJson = json::parse(responseJsonStr);

    json fixtures = engineJson["fixture_handler"];

    log.println("Fixtures (num = {}):", fixtures.value("num_fixtures", 1));
    for (const auto& fixture : fixtures["fixtures"])
    {
        FixtureConfig cfg;
        if(fixture.contains("config") && !fixture["config"].is_null())
        {
            cfg = FixtureConfig(fixture["config"]);
        }
        
        log.println(" - ID: {}", cfg.id);
        log.println(" - name: '{}'", cfg.name);
        log.println(" - type: '{}'", cfg.type);
        log.println(" - universe: {}", cfg.universe);
        log.println(" - address: {}", cfg.address);
        log.println(" - selected preset({}): '{}'", cfg.presetIndex, fixture.value("preset", "unknown"));
        log.println(" - number of presets: {}", cfg.numPresets);
        log.println(" - footprint: {}", fixture.value("footprint", -1));
        if(fixture.contains("input_type"))
        {
            log.println(" - input type: {}", fixture.value("input_type", "unknown"));
        }

        if(!fixture.contains("outputs"))
        {
            return true;
        }

        log.println(" - outputs:");
        int i = 0;
        for (const auto& output : fixture["outputs"])
        {
            std::string outputType = output["type"];
            int pin = output["hardware_pin"];

            log.println("   - id: {}", i++);
            log.println("     - type: {}", outputType);
            log.println("     - hardware pin: {}", pin);
        }
    }

    return true;
}

bool CommandExecutor::inputs(const std::vector<std::string>& args)
{
    auto resp = m_client->runStr(JSONtemp::stringify(args[0]), true);
    using nlohmann::json;
    
    json respj = json::parse(resp);
    std::string responseJsonStr = respj["response"];

    json engineJson = json::parse(responseJsonStr);
    json inputs = engineJson["input_handler"];

    log.println("Inputs (num = {}):", inputs.value("num_inputs", 1));
    for (const auto& input : inputs["inputs"])
    {
        log.println(" - ID: {}", input.value("id", -1));
        log.println(" - type: '{}'", input.value("type", "Unknown"));
        log.println(" - universe: {}", input.value("universe", -1));
        log.print(" - bytes: ");

        const auto& bytes = input["9_bytes"];

        for (const auto& b : bytes)
        {
            int value = b;
            log.print("{} ", value);
        }

        log.println("");
    }

    return true;
}

bool CommandExecutor::inbytes(const std::vector<std::string>& args)
{
    std::string cmd = args[0];
    // uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
    auto resp = m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", cmd), false);

    using nlohmann::json;    
    try
    {
        json respj = json::parse(resp);
        std::string responseJsonStr = respj["response"];
        json engineJson = json::parse(responseJsonStr);

        std::vector<int> values = engineJson["value"].get<std::vector<int>>();

        for (int i = 0; i < 16; i++)
        {
            static const std::string hex = "0123456789ABCDEF";
            log.print("  0x{}", hex[i]);
        }

        log.println("");
        for (int i = 0; i < 16 * 5; i++)
        {
            log.print("-");
        }
        log.println("");

        int i = 0;
        for(auto& b : values)
        {
            if(i != 0 && i % 16 == 0)
            {
                log.println("");
            }

            log.print("  {:3}", b);
            i++;
        }
        log.println("");
    }
    catch (const json::parse_error& e)
    {
        log.println("Inbytes not supported");
        return false;
    }
    
    return true;
}



bool CommandExecutor::get_fixture_config(const std::vector<std::string>& args) {
    // uint32_t id = args.size() == 2 ? std::stoi(args[1]) : 0;
    auto ret = m_client->runStr(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", "config"), false);
    using json = nlohmann::json;

    json data = json::parse(ret);

    json respjson = json::parse(data["response"].get<std::string>());
    auto v = respjson["value"];
    FixtureConfig cfg(v);

    log.println("Fixture '{}' [{}]:", cfg.name, cfg.type);
    log.println(" - patch: uni {}, addr {}, preset {}", cfg.universe, cfg.address, cfg.presetIndex);
    log.println(" - number of presets: {}", cfg.numPresets);
    auto nleds = m_client->getClientInfo()->descriptions[m_client->clientID()].num_leds;
    if(nleds != 0)
    {
        log.println(" - num leds: {}", nleds);
    }
    return true;
}

bool CommandExecutor::get_fixture_info(const std::vector<std::string> & args) {
    std::string cmd = args[0];
    if(cmd == "preset")
    {
        cmd = "presetIndex";
    }

    log.debug("{} exists in array", cmd);
    switch(args.size())
    {
        case 1:
            m_client->run(JSONtemp::stringify("getfixture", "id", m_client->clientID(), "value", cmd), false);
            break;
        case 2:
            if (cmd == "name") {
                m_client->run(JSONtemp::stringify("setfixture", "id", m_client->clientID(), cmd, args[1]), false);
            }
            else if (cmd == "highlight") {
                m_client->run(JSONtemp::stringify("setfixture", "id", m_client->clientID(), cmd, args[1] == "true" ? true : false), false);
            }
            else {
                m_client->run(JSONtemp::stringify("setfixture", "id", m_client->clientID(), cmd, std::stoi(args[1])), false);
            }
            break;
        // default:
        //     return false;
    }

    return true;
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
