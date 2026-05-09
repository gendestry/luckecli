#include <iostream>
#include <string>
#include <memory>
#include <vector>

#include "Utils/Logging/Logger.h"
#include "Utils/Text/String.h"
#include "ESPClient.h"
#include "Heartbeat.h"
#include "JSONtemp.h"

int main() {
    Utils::Logger logger("Main");
    logger.toggleScope();
    // logger.setLoggerLevel(Utils::Logger::DEBUGGING);

    Heartbeat heartbeat;
    std::unique_ptr<ESPClient> client;

    bool selected = false;
    logger.print("> ");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit") {
            logger.debug("Exiting");
            if (client) client.reset();
            ESPClient::stop();
            break;
        }

        if (!selected) {
            if (line.size() == 0 || line == "list") {
                logger.println("Fixtures online:");
                for (ClientInfo* clientinfo : heartbeat.getClients()) {
                    Utils::Text::Stream stream;
                    stream << clientinfo->descriptions[0].name;
                    for (int i = 1; i < clientinfo->descriptions.size(); i++) {
                        stream << ", " << clientinfo->descriptions[i].name;
                    }
                    logger.println(" - {}: {} {}({}){}", clientinfo->index, stream.end(), Utils::Font::colorItalic, clientinfo->ip, Utils::Font::colorReset);
                }
            }
            else if (line.starts_with("select")) {
                auto sp = Utils::String::split(line, " ");
                if (sp.size() == 2) {
                    const auto i = std::stoi(sp[1]);
                    const auto size = heartbeat.size();
                    if (i >= size) {
                        logger.error("Index out of bounds, size: {}, index: {}", size, sp[1]);
                    }
                    else {
                        const auto ip = heartbeat.getClients()[i]->ip;
                        logger.debug("Selected: {} with ip {}", sp[1], ip);
                        selected = true;
                        client = std::make_unique<ESPClient>(ip);
                    }
                }
                else {
                    logger.error("Required inputs: 2, given: {}", sp.size());
                }
            }
            else if (Utils::String::isInt(line)) {
                const auto i = std::stoi(line);
                const auto size = heartbeat.size();
                if (i >= size) {
                    logger.error("Index out of bounds, size: {}, index: {}", size, i);
                }
                else {
                    const auto ip = heartbeat.getClients()[i]->ip;
                    logger.debug("Selected: {} with ip {}", line, ip);
                    selected = true;
                    client = std::make_unique<ESPClient>(ip);
                }
            }
            else if (line.starts_with("selectip")) {
                auto sp = Utils::String::split(line, " ");
                if (sp.size() == 2) {
                    logger.debug("Selected: {}", sp[1]);
                    selected = true;
                    client = std::make_unique<ESPClient>(sp[1]);
                }
                else {
                    logger.error("Required inputs: 2, given: {}", sp.size());
                }
            }
            else {
                logger.error("Invalid command: {}", line);
            }
        }
        else {
            auto sp = Utils::String::split(line, " ");

            if (line.starts_with("back")) {
                client.reset();
                selected = false;
            }
            else if (line.starts_with("describe")) {
                client->run(JSONtemp::stringify("describe"), true);
            }
            else if (line.starts_with("fixtures")) {
                client->run(JSONtemp::stringify("fixtures"), true);
            }
            else if (line.starts_with("inputs")) {
                client->run(JSONtemp::stringify("inputs"), true);
            }
            else if (line.starts_with("presets")) {
                client->run(JSONtemp::stringify("presets"), true);
            }
            else if (line.starts_with("ip")) {
                client->run(JSONtemp::stringify("ip"));
            }
            else if (line.starts_with("factoryreset")) {
                client->sendRequest(JSONtemp::stringify("factory_reset"));
            }
            else if (line.starts_with("reboot")) {
                client->run(JSONtemp::stringify("reboot"));
                client.reset();
                selected = false;
            }

            else if (line.starts_with("wifianimation")) {
                if (sp.size() == 1) {
                    client->run(JSONtemp::stringify("wifi_animation"));
                }
                else {
                    client->run(JSONtemp::stringify("wifi_animation", "value", sp[1]));
                }
            }
            else if (line.starts_with("serialprint")) {
                if (sp.size() == 1) {
                    client->run(JSONtemp::stringify("serial_report_task"));
                }
                else {
                    client->run(JSONtemp::stringify("serial_report_task", "value", sp[1]));
                }
            }
            else if (line.starts_with("wirelessprint")) {
                if (sp.size() == 1) {
                    client->run(JSONtemp::stringify("wireless_report_task"));
                }
                else {
                    client->run(JSONtemp::stringify("wireless_report_task", "value", sp[1]));
                }
            }

            else if (line.starts_with("setwifi")) {
                client->run(JSONtemp::stringify("wifi", "ssid", sp[1], "password", sp[2]));
            }
            else if (line.starts_with("universe")) {
                uint32_t id = sp.size() == 2 ? std::stoi(sp[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "universe"), true);
            }
            else if (line.starts_with("address")) {
                uint32_t id = sp.size() == 2 ? std::stoi(sp[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "address"), true);
            }
            else if (line.starts_with("preset")) {
                uint32_t id = sp.size() == 2 ? std::stoi(sp[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "preset"), true);
            }
            else if (line.starts_with("name")) {
                uint32_t id = sp.size() == 2 ? std::stoi(sp[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "name"), true);
            }
            else if (line.starts_with("type")) {
                uint32_t id = sp.size() == 2 ? std::stoi(sp[1]) : 0;
                client->run(JSONtemp::stringify("getfixture", "id", id, "value", "type"), true);
            }
            else if (line.starts_with("setuniverse")) {
                uint32_t id = sp.size() == 3 ? std::stoi(sp[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "universe", std::stoi(sp[1])));
            }
            else if (line.starts_with("setaddress")) {
                uint32_t id = sp.size() == 3 ? std::stoi(sp[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "address", std::stoi(sp[1])));
            }
            else if (line.starts_with("setpreset")) {
                uint32_t id = sp.size() == 3 ? std::stoi(sp[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "presetIndex", std::stoi(sp[1])));
            }
            else if (line.starts_with("highlight")) {
                uint32_t id = sp.size() == 3 ? std::stoi(sp[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "highlight", sp[1]));
            }
            else if (line.starts_with("setname")) {
                uint32_t id = sp.size() == 3 ? std::stoi(sp[2]) : 0;
                client->run(JSONtemp::stringify("setfixture", "id", id, "name", sp[1]));
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
