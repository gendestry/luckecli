//
// Created by bobi on 17. 05. 26.
//

#include "CommandList.h"

void CommandList::registerCommand(Command&& command, Command::Usable when) {
    const auto ptr = std::make_shared<Command>(std::move(command));
    ptr->usable = when;
    commands.push_back(ptr);
    commandsMap[when][ptr->name] = ptr;
    currentGroup.commands.push_back(ptr);
}

void CommandList::registerGroup(std::string name) {
    if (currentGroup.name.empty()) {
        currentGroup.name = std::move(name);
    }
    else {
        groups.push_back(std::move(currentGroup));
        currentGroup = Group();
        currentGroup.name = name;
    }
}

void CommandList::pushGroup() {
    groups.push_back(std::move(currentGroup));
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::anytime(const std::string& cmd) {
    if (commandsMap[Command::Usable::ANYTIME].contains(cmd))
    {
        return commandsMap[Command::Usable::ANYTIME][cmd]->func;
    }
    return {};
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::selected(const std::string& cmd) {
    if (commandsMap[Command::Usable::SELECTED].contains(cmd))
    {
        return commandsMap[Command::Usable::SELECTED][cmd]->func;
    }
    return {};
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::unselected(const std::string& cmd) {
    if (commandsMap[Command::Usable::UNSELECTED].contains(cmd))
    {
        return commandsMap[Command::Usable::UNSELECTED][cmd]->func;
    }
    return {};
}

std::optional<std::shared_ptr<Command>> CommandList::get(const std::string& cmd) {
    for (auto& [_, value] : commandsMap) {
        if (value.contains(cmd)) {
            return value.at(cmd);
        }
    }
    return {};
}