//
// Created by bobi on 17. 05. 26.
//

#pragma once
#include <list>
#include <unordered_map>
#include <optional>

#include "Command.h"
#include "Command.h"

struct Group {
    std::string name;
    std::vector<std::shared_ptr<Command>> commands;
};

class CommandList
{
    std::optional<std::function<bool(const std::vector<std::string>&)>> getfunc(const std::vector<std::string>& args, Command::Usable when);

    std::vector<Group> groups;
    Group currentGroup;

    std::vector<std::shared_ptr<Command>> commands;
    std::unordered_map<Command::Usable, std::list<std::shared_ptr<Command>>> commandsNameless;
    std::unordered_map<Command::Usable, std::unordered_map<std::string, std::shared_ptr<Command>>> commandsMap;
public:


    void registerCommand(Command&& command, Command::Usable when);
    void registerCommandNoname(Command&& command, Command::Usable when);
    void registerGroup(std::string name);
    void pushGroup();

    const std::vector<Group>& getGroups() const { return groups; }

    std::optional<std::function<bool(const std::vector<std::string>&)>> anytime(const std::vector<std::string>& args);

    std::optional<std::function<bool(const std::vector<std::string>&)>> selected(const std::vector<std::string>& args);

    std::optional<std::function<bool(const std::vector<std::string>&)>> unselected(const std::vector<std::string>& args);

    std::optional<std::shared_ptr<Command>> get(const std::string& cmd);
};