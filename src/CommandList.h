//
// Created by bobi on 17. 05. 26.
//

#pragma once
#include <unordered_map>
#include <optional>
#include "Command.h"

struct Group {
    std::string name;
    std::vector<std::shared_ptr<Command>> commands;
};

struct CommandList
{
    std::vector<Group> groups;
    Group currentGroup;

    std::vector<std::shared_ptr<Command>> commands;
    std::unordered_map<Command::Usable, std::unordered_map<std::string, std::shared_ptr<Command>>> commandsMap;

    void registerCommand(Command&& command, Command::Usable when);
    void registerGroup(std::string name);
    void pushGroup();

    std::optional<std::function<bool(const std::vector<std::string>&)>> anytime(const std::string& cmd);

    std::optional<std::function<bool(const std::vector<std::string>&)>> selected(const std::string& cmd);

    std::optional<std::function<bool(const std::vector<std::string>&)>> unselected(const std::string& cmd);

    std::optional<std::shared_ptr<Command>> get(const std::string& cmd);
};