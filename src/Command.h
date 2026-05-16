//
// Created by bobi on 15. 05. 26.
//

#pragma once
#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <optional>
#include <memory>

struct Command {
    using Callback = std::function<bool(const std::vector<std::string>&)>;


    std::string name;
    std::string desc;
    std::string usage;
    // Usable usable;
    Callback func;

    Command() = default;
    Command(std::string name, std::string desc, std::string usage, Callback func)
         : name(std::move(name)), desc(std::move(desc)), usage(std::move(usage)), func(std::move(func)) {}
    Command(Command&&) noexcept = default;
    Command& operator=(Command&&) noexcept = default;

    Command(const Command&) = default;
    Command& operator=(const Command&) = default;
};

struct CommandList
{
    enum class Usable {
        SELECTED,
        UNSELECTED,
        ANYTIME
   };

    std::unordered_map<Usable, std::unordered_map<std::string, Command>> commandsMap;

    void registerCommand(Command&& command, Usable when) {
        commandsMap[when][command.name] = std::move(command);
    }

    std::optional<std::function<bool(const std::vector<std::string>&)>> anytime(const std::string& cmd) {
        if (commandsMap[Usable::ANYTIME].contains(cmd))
        {
            return commandsMap[Usable::ANYTIME][cmd].func;
        }
        return {};
    }

    std::optional<std::function<bool(const std::vector<std::string>&)>> selected(const std::string& cmd) {
        if (commandsMap[Usable::SELECTED].contains(cmd))
        {
            return commandsMap[Usable::SELECTED][cmd].func;
        }
        return {};
    }

    std::optional<std::function<bool(const std::vector<std::string>&)>> unselected(const std::string& cmd) {
        if (commandsMap[Usable::UNSELECTED].contains(cmd))
        {
            return commandsMap[Usable::UNSELECTED][cmd].func;
        }
        return {};
    }

    std::optional<std::reference_wrapper<Command>> get(const std::string& cmd) {
        for (auto& [_, value] : commandsMap) {
            if (value.contains(cmd)) {
                return value.at(cmd);
            }
        }
        return {};
    }
};
