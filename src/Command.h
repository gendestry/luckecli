//
// Created by bobi on 15. 05. 26.
//

#pragma once
#include <functional>
#include <vector>
#include <string>

#include <memory>

struct Command {
    using Callback = std::function<bool(const std::vector<std::string>&)>;

    enum class Usable : uint8_t{
        SELECTED = 1U,
        UNSELECTED = 2U,
        ANYTIME = 4U
    };

    std::string name;
    std::string desc;
    std::string usage;
    Usable usable;
    Callback func;

    bool isUsable(Usable compare) {
        return static_cast<uint8_t>(compare) & static_cast<uint8_t>(usable);
    }

    bool isAnytime() {
        return usable == Usable::ANYTIME;
    }

    bool isSelected() {
        return usable == Usable::SELECTED;
    }

    bool isUnselected() {
        return usable == Usable::UNSELECTED;
    }

    Command() = default;
    Command(std::string name, std::string desc, std::string usage, Callback func)
         : name(std::move(name)), desc(std::move(desc)), usage(std::move(usage)), func(std::move(func)) {}
    Command(Command&&) noexcept = default;
    Command& operator=(Command&&) noexcept = default;

    Command(const Command&) = default;
    Command& operator=(const Command&) = default;
};


