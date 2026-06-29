#include "Registry.h"

namespace Test::Commands
{

    void Registry::add(std::vector<std::string> aliases, std::string desc, std::string usage,
                       Command::Usable usable, Command::Callback func)
    {
        auto cmd = std::make_shared<Command>();
        cmd->aliases = std::move(aliases);
        cmd->desc = std::move(desc);
        cmd->usage = std::move(usage);
        cmd->usable = usable;
        cmd->func = std::move(func);

        named.push_back(cmd);
        current.commands.push_back(cmd);
    }

    void Registry::addPredicate(std::string label, std::string desc, std::string usage,
                                Command::Usable usable, Command::Condition cond, Command::Callback func)
    {
        auto cmd = std::make_shared<Command>();
        cmd->aliases = {std::move(label)};
        cmd->desc = std::move(desc);
        cmd->usage = std::move(usage);
        cmd->usable = usable;
        cmd->condition = std::move(cond);
        cmd->func = std::move(func);

        predicate.push_back(cmd);
        current.commands.push_back(cmd);
    }

    void Registry::group(std::string name)
    {
        if (current.name.empty())
        {
            current.name = std::move(name);
        }
        else
        {
            groups.push_back(std::move(current));
            current = Group{};
            current.name = std::move(name);
        }
    }

    void Registry::finish()
    {
        if (!current.name.empty() || !current.commands.empty())
            groups.push_back(std::move(current));
        current = Group{};
    }

    std::shared_ptr<Command> Registry::resolve(const Args &args, Command::Usable mode) const
    {
        // A command matches the current mode if it's usable in `mode` OR ANYTIME.
        auto matchesMode = [&](const Command &c)
        {
            return c.isUsable(mode) || c.isUsable(Command::Usable::ANYTIME);
        };

        // Named commands win: match args[0] against each alias.
        if (!args.empty())
        {
            for (const auto &cmd : named)
                if (matchesMode(*cmd))
                    for (const auto &alias : cmd->aliases)
                        if (alias == args.command())
                            return cmd;
        }

        // Fall back to predicate commands (<id>, <name>, empty-line handlers, ...).
        for (const auto &cmd : predicate)
            if (matchesMode(*cmd) && cmd->condition(args))
                return cmd;

        return nullptr;
    }

    std::shared_ptr<Command> Registry::get(const std::string &name) const
    {
        for (const auto &cmd : named)
            for (const auto &alias : cmd->aliases)
                if (alias == name)
                    return cmd;
        return nullptr;
    }

}
