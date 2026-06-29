#include "CommandExecutor.h"
#include "Tokenizer.h"
#include "Test/ESPClient.h"
#include "JSONtemp.h"

#include <iostream>
#include <optional>
#include <nlohmann/json.hpp>

namespace Test::Commands
{

    CommandExecutor::CommandExecutor(SharedState &state)
        : log("Command"), m_sharedState(state)
    {
        bindCommands();
    }

    // Flatten the device registry into an ordered (ip, fixtureId) list so a
    // single index can address any fixture across all devices.
    static std::vector<CommandExecutor::Selection> flatten(SharedState &state);

    void CommandExecutor::run()
    {
        std::string line;
        while (!m_exit)
        {
            std::cout << (m_selection.empty() ? "> " : "selected> ") << std::flush;
            if (!std::getline(std::cin, line))
                break;
            resolveCommand(line);
        }
    }

    bool CommandExecutor::resolveCommand(const std::string &line)
    {
        Args args = tokenize(line);

        auto cmd = m_registry.resolve(args, mode());
        if (!cmd)
        {
            // Empty line defaults to `list` when unselected — only complain on
            // a non-empty, unrecognised command.
            if (!args.empty())
                log.error("Invalid command: {}", line);
            else if (m_selection.empty())
                return list(args);
            return false;
        }

        return cmd->func(args);
    }

    void CommandExecutor::bindCommands()
    {
        m_registry.group("Generic");

        m_registry.add("exit", "Exit the program", "exit",
                       Command::Usable::ANYTIME,
                       [this](const Args &a) { return exit(a); });

        m_registry.add("help", "Print help", "help <command>?",
                       Command::Usable::ANYTIME,
                       [this](const Args &a) { return help(a); });

        m_registry.add("list", "List devices and fixtures", "list",
                       Command::Usable::UNSELECTED,
                       [this](const Args &a) { return list(a); });

        m_registry.add("select", "Select fixture(s) by index", "select <id> <id>...",
                       Command::Usable::UNSELECTED,
                       [this](const Args &a) { return select(a); });

        // Bare "<id> ..." (no `select` keyword) also selects, as long as the
        // first token is an integer.
        m_registry.addPredicate("<id>", "Select fixture(s) by index", "<id> <id>...",
                                Command::Usable::UNSELECTED,
                                [](const Args &a) { return a.isInt(0); },
                                [this](const Args &a) { return select(a); });

        m_registry.group("Selection");

        m_registry.add(std::vector<std::string>{"back", "b"}, "Clear the selection", "back",
                       Command::Usable::SELECTED,
                       [this](const Args &a) { return deselect(a); });

        m_registry.add("setuniverse", "Set universe on selected fixture(s)", "setuniverse <universe>",
                       Command::Usable::SELECTED,
                       [this](const Args &a) { return setuniverse(a); });

        m_registry.add("describe", "Pretty-print describe JSON for selected fixture(s)", "describe",
                       Command::Usable::SELECTED,
                       [this](const Args &a) { return describe(a); });

        m_registry.finish();
    }

    // ── handlers ────────────────────────────────────────────────

    bool CommandExecutor::exit(const Args &)
    {
        m_exit = true;
        return true;
    }

    bool CommandExecutor::help(const Args &args)
    {
        if (args.has(1))
        {
            auto cmd = m_registry.get(args[1]);
            if (!cmd)
            {
                log.error("Unknown command: {}", args[1]);
                return false;
            }
            log.println("{} - {}", cmd->name(), cmd->desc);
            log.println("  usage: {}", cmd->usage);
            return true;
        }

        for (const auto &group : m_registry.getGroups())
        {
            bool printedHeader = false;
            for (const auto &cmd : group.commands)
            {
                if (!cmd->isUsable(mode()) && !cmd->isUsable(Command::Usable::ANYTIME))
                    continue;
                if (!printedHeader)
                {
                    log.println("{}", group.name);
                    printedHeader = true;
                }
                log.println("  {} - {}", cmd->name(), cmd->desc);
            }
        }
        return true;
    }

    bool CommandExecutor::list(const Args &)
    {
        auto devices = m_sharedState.snapshot();
        log.println("Devices online ({})", devices.size());

        int index = 0;
        for (const auto &[ip, client] : devices)
        {
            log.println("  {}  v{}", ip, client.engine.version);
            for (const auto &fx : client.fixtures)
            {
                log.println("    [{}] {}  {}", index++, fx.name, fx.type);
            }
        }
        return true;
    }

    bool CommandExecutor::select(const Args &args)
    {
        auto all = flatten(m_sharedState);

        // Collect every integer token as a flat fixture index. The leading
        // "select" keyword (if present) is not an int, so it's skipped naturally.
        std::vector<Selection> picked;
        for (size_t i = 0; i < args.size(); ++i)
        {
            auto idx = args.getInt(i);
            if (!idx)
                continue;
            if (*idx < 0 || static_cast<size_t>(*idx) >= all.size())
            {
                log.error("Index out of range: {} (have {})", *idx, all.size());
                continue;
            }
            picked.push_back(all[*idx]);
        }

        if (picked.empty())
        {
            log.error("Nothing selected");
            return false;
        }

        m_selection = std::move(picked);
        log.println("Selected {} fixture(s)", m_selection.size());
        return true;
    }

    bool CommandExecutor::deselect(const Args &)
    {
        m_selection.clear();
        return true;
    }

    bool CommandExecutor::setuniverse(const Args &args)
    {
        auto universe = args.getInt(1);
        if (!universe)
        {
            log.error("Usage: setuniverse <universe>");
            return false;
        }

        // Scalar shape: apply the same universe to every selected fixture.
        // Range/ascending and per-fixture vector inputs are planned but not yet
        // implemented (see multi-fixture-selection-plan).
        for (const auto &sel : m_selection)
        {
            auto client = m_sharedState.getESPClient(sel.ip);
            auto resp = client->sendRequestOpt(
                JSONtemp::stringify("setfixture", "id", sel.fixtureId, "universe", *universe), 4000ms);
            if (!resp)
                log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
        }
        return true;
    }

    bool CommandExecutor::describe(const Args &)
    {
        using json = nlohmann::json;
        for (const auto &sel : m_selection)
        {
            auto client = m_sharedState.getESPClient(sel.ip);
            auto resp = client->sendRequestOpt(JSONtemp::stringify("describe"), 4000ms);
            if (!resp)
            {
                log.error("No response from {} fixture {}", sel.ip, sel.fixtureId);
                continue;
            }

            try
            {
                json data = json::parse(*resp);
                log.println("{} fixture {}:\n{}", sel.ip, sel.fixtureId, data.dump(2));
            }
            catch (const json::parse_error &e)
            {
                log.error("Parse error from {}: {}", sel.ip, e.what());
            }
        }
        return true;
    }

    static std::vector<CommandExecutor::Selection> flatten(SharedState &state)
    {
        std::vector<CommandExecutor::Selection> out;
        for (const auto &[ip, client] : state.snapshot())
            for (int id = 0; id < static_cast<int>(client.fixtures.size()); ++id)
                out.push_back({ip, id});
        return out;
    }

}
