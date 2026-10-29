#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/commands/Command.h"

namespace core::commands
{

    // For `help`: commands grouped under a heading, in registration order.
    struct Group
    {
        std::string name;
        std::vector<std::shared_ptr<Command>> commands;
    };

    // Stores every bound command and resolves a tokenized line to the command
    // that should run in the current mode. Replaces the old CommandList's three
    // Usable-keyed maps with one list + a bitmask test.
    class Registry
    {
        std::vector<std::shared_ptr<Command>> named;     // matched by aliases[i] == args[0]
        std::vector<std::shared_ptr<Command>> predicate; // matched by condition(args)
        std::vector<Group> groups;
        Group current;

    public:
        // Register a named command (one or more aliases).
        void add(std::vector<std::string> aliases, std::string desc, std::string usage,
                 Command::Usable usable, Command::Callback func);

        void add(std::string name, std::string desc, std::string usage,
                 Command::Usable usable, Command::Callback func)
        {
            add(std::vector<std::string>{std::move(name)}, std::move(desc), std::move(usage), usable, std::move(func));
        }

        // Register a nameless command matched by a predicate (e.g. <id>, <name>).
        void addPredicate(std::string label, std::string desc, std::string usage,
                          Command::Usable usable, Command::Condition cond, Command::Callback func);

        // Start a new help group; flush the previous one. Call finish() at the end.
        void group(std::string name);
        void finish();

        const std::vector<Group> &getGroups() const { return groups; }

        // Find the command for `args` valid in `mode`. Named matches (by args[0]
        // / aliases) win over predicate matches. Returns nullptr if none apply.
        // A command usable ANYTIME matches regardless of `mode`.
        std::shared_ptr<Command> resolve(const Args &args, Command::Usable mode) const;

        // Look up a command by name/alias regardless of mode (for `help <cmd>`).
        std::shared_ptr<Command> get(const std::string &name) const;
    };

}
