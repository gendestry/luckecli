#pragma once
#include <string>
#include <vector>

#include "support/log/Log.h"
#include "core/commands/Registry.h"
#include "core/commands/Args.h"
#include "core/domain/SharedState.h"
#include "core/domain/Selection.h"

namespace ui
{
    class Display; // the frontend driving the input loop; full def needed in .cpp
}

namespace core::commands
{

    // The REPL: reads lines, tokenizes them, resolves against the Registry for
    // the current mode, and runs the matched handler. Handlers take a const Args&
    // and act on the current selection (which may hold one or many fixtures).
    class CommandExecutor
    {
    public:
        // A selected fixture = a device IP plus a fixture id within it.
        // Defined in core/domain/Selection.h; aliased here so existing
        // CommandExecutor::Selection references keep resolving.
        using Selection = core::Selection;

    private:
        Log log;
        SharedState &m_sharedState;
        ui::Display &m_display;
        Registry m_registry;

        // The selection: a set of fixtures commands can target at once;
        // empty selection == "unselected" mode.
        core::SelectionSet m_selection;

        bool m_exit = false;

        // Every command that flows through resolveCommand() (typed in the CLI or
        // fired by a TUI button) is appended here and persisted to
        // ~/.luckecli_history, so both frontends share one history/activity log.
        std::vector<std::string> m_history;
        void recordHistory(const std::string &line);

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
        bool setaddress(const Args &);
        bool setpreset(const Args &);
        bool setname(const Args &);
        bool highlight(const Args &);
        bool blackout(const Args &);
        bool setcolor(const Args &);
        bool reboot(const Args &);
        bool factoryreset(const Args &);
        bool setwifi(const Args &);
        bool describe(const Args &);
        bool presets(const Args &);
        bool outputs(const Args &);
        bool exportgdtf(const Args &);
        bool exportmvr(const Args &);

        // GET/SET a boolean engine task (wifianimation/serialprint/wirelessprint).
        // `field` is the firmware request name; `cache` is the matching cached
        // Engine flag to patch (nullptr if the flag isn't cached).
        bool taskConfig(const Args &args, const std::string &cmdName,
                        const std::string &field, bool Client::Engine::*cache);

        // Re-fetch a device's describe (and preset names) and commit it to the
        // cache. Used after operations that change cached fixture data the device
        // computes itself — e.g. setpreset alters the channel footprint. Returns
        // false (and logs) if the device doesn't respond or the parse fails.
        bool refreshDevice(const std::string &ip);

        // Unique device IPs across the current selection (a device may host
        // several selected fixtures). For device-level commands (reboot, setwifi).
        std::vector<std::string> selectedIps() const;

        // The REPL prompt: "> " when unselected, otherwise the themed, comma-
        // separated names of the selected fixtures.
        std::string selectionPrompt();

        // ── themed output helpers ───────────────────────────────
        // "✓ <label> → <value>  <ip> fixture <id>"
        void okFixture(const Selection &sel, const std::string &label, const std::string &value);
        // "✓ <ip>  <msg>"
        void okDevice(const std::string &ip, const std::string &msg);

        // Send an int-valued `setfixture` field to the *single* selected fixture
        // and patch the cache on success. Rejects empty/multi selections, since
        // address/preset are inherently per-fixture. `jsonKey` is the firmware
        // field name; `member` is the matching cached Client::Fixture field.
        bool setFixtureField(const Args &args, const std::string &cmdName,
                             const std::string &jsonKey, int Client::Fixture::*member);

    public:
        CommandExecutor(SharedState &state, ui::Display &display);

        bool shouldQuit() const { return m_exit; }

        // Selection accessors for frontends (e.g. the FTXUI grid marking which
        // cards are selected). Only touched on the command/UI thread.
        bool isSelected() const { return !m_selection.empty(); }
        const core::SelectionSet &selection() const { return m_selection; }

        // Shared command history, oldest first. Read by the CLI for up/down
        // recall and by the TUI to display an activity log.
        const std::vector<std::string> &history() const { return m_history; }

        // Select the fixture at a flat index (as listed/shown in the grid).
        // additive=false replaces the selection; additive=true toggles the
        // fixture in or out of it (shift-click). Out-of-range indices are ignored.
        void selectIndex(int flatIndex, bool additive);

        // Bulk selection helpers for the TUI menubar. selectAll() selects every
        // currently-online fixture; clearSelection() drops the whole selection.
        // Touched only on the command/UI thread, like selectIndex().
        void selectAll();
        void clearSelection();

        // Tokenize, resolve and dispatch a single line. Returns the handler's
        // result (false on unknown/invalid command).
        bool resolveCommand(const std::string &line);

        // Drive the bound Display's input loop until exit.
        void run();
    };

}
