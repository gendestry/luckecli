#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Args.h"

namespace Test::Commands
{

    // A single bound command: how to match it, how to describe it, and what to
    // run. `usable` is a bitmask so one command can be valid in several modes
    // (e.g. SELECTED | UNSELECTED) — see isUsable().
    //
    // Note: a command is just a name -> callback route. It is intentionally
    // agnostic about how many fixtures are selected; the selection (one or many)
    // lives in CommandExecutor, and the handler applies the parsed Args to all
    // of them. So batch/multi-fixture commands need no change here.
    struct Command
    {
        using Callback = std::function<bool(const Args &)>;
        // Predicate for nameless commands (<id>, <name>): decides whether this
        // command should handle the given line. Default: never.
        using Condition = std::function<bool(const Args &)>;

        enum class Usable : uint8_t
        {
            SELECTED = 1U,
            UNSELECTED = 2U,
            ANYTIME = 4U,
        };

        std::vector<std::string> aliases; // aliases[0] is the canonical name
        std::string desc;
        std::string usage;
        Usable usable = Usable::ANYTIME;
        Callback func;
        Condition condition = [](const Args &) { return false; };

        const std::string &name() const { return aliases.front(); }

        // True if this command is allowed in `mode` (bitmask test).
        bool isUsable(Usable mode) const
        {
            return static_cast<uint8_t>(usable) & static_cast<uint8_t>(mode);
        }
    };

    // Bitwise OR so registrations can say `SELECTED | UNSELECTED`.
    inline Command::Usable operator|(Command::Usable a, Command::Usable b)
    {
        return static_cast<Command::Usable>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

}
