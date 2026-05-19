//
// Created by bobi on 10. 05. 26.
//
#pragma once
#include <memory>
#include "ESPClient.h"
#include "Heartbeat.h"
#include <unordered_map>
#include <functional>

#include "Utils/Logging/Logger.h"
#include "CommandList.h"
#include "SharedState.h"


class CommandExecutor {

     std::unique_ptr<ESPClient> client;
     // Heartbeat heartbeat;
     CommandList cmdlist;

     bool selected = false;
     bool exitRequest = false;

     SharedState& m_sharedState;

     Utils::Logger logger;

     void bindcommands();
public:
     static Utils::Text::Stream stream;
     CommandExecutor(SharedState& state);
//
//     void setClient(std::shared_ptr<ESPClient> newclient) {
//         this->client = newclient;
//     }
//
     bool isSelected() { return selected; }
     bool shouldQuit() { return exitRequest;}
     bool resolveCommand(const std::string& line);
//
     bool exit(const std::vector<std::string>& = {});
     bool help(const std::vector<std::string>& = {});
     bool reboot(const std::vector<std::string>& = {});

     bool all_config(const std::vector<std::string>& = {});
     bool list(const std::vector<std::string>& = {});
     bool select(const std::vector<std::string>& = {});
     bool selectName(const std::vector<std::string>& = {});
     bool selectIP(const std::vector<std::string>& = {});

     bool deselect(const std::vector<std::string>& = {});

     bool task_config(const std::vector<std::string>& = {});
     bool get_info(const std::vector<std::string>& = {});
     bool presets(const std::vector<std::string>& args = {});

     bool get_fixture_info(const std::vector<std::string>& = {});
     bool get_fixture_config(const std::vector<std::string>& = {});
     bool set_fixture_info(const std::vector<std::string>& = {});

     bool setwifi(const std::vector<std::string>&);
};
