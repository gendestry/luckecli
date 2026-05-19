//
// Created by bobi on 17. 05. 26.
//

#include "CommandList.h"

void CommandList::registerCommand(Command&& command, Command::Usable when) {
    const auto ptr = std::make_shared<Command>(std::move(command));
    ptr->usable = when;
    commands.push_back(ptr);
    for (auto& alias : ptr->aliases) {
        commandsMap[when][alias] = ptr;
    }

    currentGroup.commands.push_back(ptr);
}

void CommandList::registerCommandNoname(Command&& command, Command::Usable when) {
    const auto ptr = std::make_shared<Command>(std::move(command));
    ptr->usable = when;
    commandsNameless[when].push_back(ptr);
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

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::anytime(const std::vector<std::string>& args) {
    return getfunc(args, Command::Usable::ANYTIME);
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::selected(const std::vector<std::string>& args) {
    return getfunc(args, Command::Usable::SELECTED);
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::unselected(const std::vector<std::string>& args) {
    return getfunc(args, Command::Usable::UNSELECTED);
}

std::optional<std::function<bool(const std::vector<std::string>&)>> CommandList::getfunc(const std::vector<std::string>& args, Command::Usable when) {
    if (!args.empty()) {
        const auto& cmd = args[0];
        if (commandsMap[when].contains(cmd))
        {
            return commandsMap[when][cmd]->func;
        }
    }

    auto& cmds = commandsNameless[when];
    for (auto& it : cmds) {
        if (it->condition(args)) {
            return it->func;
        }
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