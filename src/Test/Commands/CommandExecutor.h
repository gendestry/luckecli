#pragma once
#include <string>
#include <vector>

#include "Diary/Log.h"
#include "Registry.h"
#include "Args.h"
#include "Test/SharedState.h"

namespace Test::Commands
{

    // The REPL: reads lines, tokenizes them, resolves against the Registry for
    // the current mode, and runs the matched handler. Handlers take a const Args&
    // and act on the current selection (which may hold one or many fixtures).
    class CommandExecutor
    {
    public:
        // A selected fixture = a device IP plus a fixture id within it.
        struct Selection
        {
            std::string ip;
            int fixtureId;
        };

    private:
        Log log;
        SharedState &m_sharedState;
        Registry m_registry;

        // The selection is a list so commands can target many fixtures at once;
        // empty selection == "unselected" mode.
        std::vector<Selection> m_selection;

        bool m_exit = false;

        void bindCommands();
        Command::Usable mode() const
        {
            return m_selection.empty() ? Command::Usable::UNSELECTED : Command::Usable::SELECTED;
        }

        // ── handlers ────────────────────────────────────────────
        bool exit(const Args &);
        bool help(const Args &);
        bool list(const Args &);
        bool select(const Args &);
        bool deselect(const Args &);
        bool setuniverse(const Args &);
        bool describe(const Args &);

    public:
        explicit CommandExecutor(SharedState &state);

        bool shouldQuit() const { return m_exit; }

        // Tokenize, resolve and dispatch a single line. Returns the handler's
        // result (false on unknown/invalid command).
        bool resolveCommand(const std::string &line);

        // Blocking REPL over stdin until exit.
        void run();
    };

}
