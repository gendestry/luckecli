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
        if (exec.resolveCommand(line)) {
            break;
        }
    }

    return 0;
}


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