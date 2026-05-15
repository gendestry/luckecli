//
// Created by bobi on 10. 05. 26.
//
#pragma once
#include <memory>
#include "ESPClient.h"
#include "Heartbeat.h"
#include <unordered_map>
#include <functional>
#include <list>

#include "Utils/Logging/Logger.h"

struct CmdInfo {
     enum class Usable {
          SELECTED,
          UNSELECTED,
          ANYTIME
     } usable;
     std::string name;
     std::string desc;
     std::string usage;
     using cmdFunc = std::function<bool(const std::vector<std::string>&)>;
     cmdFunc func;
};

struct CmdCache {
     std::list<CmdInfo> cmds;
     std::unordered_map<std::string, CmdInfo> commands;
     std::unordered_map<std::string, CmdInfo> commandsSelected;

     CmdInfo findCommand(const std::string& name) {}

};


class CommandExecutor {

     std::unique_ptr<ESPClient> client;
     std::unordered_map<std::string, CmdInfo> commands;
     std::unordered_map<std::string, CmdInfo> commandsSelected;
     Heartbeat heartbeat;

     bool selected = false;

     Utils::Logger logger;

     void bindcommands();
public:
     CommandExecutor();
//
//     void setClient(std::shared_ptr<ESPClient> newclient) {
//         this->client = newclient;
//     }
//
     bool isSelected() { return selected; }
     bool resolveCommand(const std::string& line);
//
     bool exit(const std::vector<std::string>& = {});
     bool help(const std::vector<std::string>& = {});
     bool reboot(const std::vector<std::string>& = {});

     bool all_config(const std::vector<std::string>& = {});
     bool list(const std::vector<std::string>& = {});
     bool select(const std::vector<std::string>& = {});
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