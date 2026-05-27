//
// Created by bobi on 10. 05. 26.
//
#pragma once
#include <memory>
#include "ESPClient.h"

#include "Diary/Log.h"
#include "CommandList.h"
#include "SharedState.h"

class CommandExecutor {
     CommandList m_cmdList;
     SharedState& m_sharedState;
     Log log;

     std::unique_ptr<ESPClient> m_client;
     bool selected = false;
     bool exitRequest = false;

     void bindCommands();
public:
     CommandExecutor(SharedState& state);

     bool isSelected() const { return selected; }
     bool shouldQuit() const { return exitRequest;}

     void run();
     bool resolveCommand(const std::string& line);

     bool exit(const std::vector<std::string>& = {});
     bool help(const std::vector<std::string>& = {});
     bool reboot(const std::vector<std::string>& = {});

     bool all_config(const std::vector<std::string>& = {});
     bool list(const std::vector<std::string>& = {});
     bool select(const std::vector<std::string>& = {});
     bool selectName(const std::vector<std::string>& = {});
     bool selectIP(const std::vector<std::string>& = {});
     bool setuniverse(const std::vector<std::string>& = {});
     bool setpreset(const std::vector<std::string>& = {});

     bool deselect(const std::vector<std::string>& = {});

     bool task_config(const std::vector<std::string>& = {});
     bool get_info(const std::vector<std::string>& = {});
     bool presets(const std::vector<std::string>& args = {});

     bool describe(const std::vector<std::string>& = {});
     bool fixtures(const std::vector<std::string>& = {});
     bool inputs(const std::vector<std::string>& = {});
     bool inbytes(const std::vector<std::string>& = {});
     bool get_fixture_info(const std::vector<std::string>& = {});
     bool get_fixture_config(const std::vector<std::string>& = {});

     bool setwifi(const std::vector<std::string>&);
     bool refresh(const std::vector<std::string>&);
};
