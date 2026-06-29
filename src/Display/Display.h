#pragma once
#include <functional>
#include <memory>
#include <string>

// Kept as forward declarations so the abstract interface stays light; concrete
// displays include the full types in their own translation units.
struct Loggable;

namespace Test::Commands
{
    class CommandExecutor;
}

namespace Test
{

    class SharedState;

    // Selectable frontend backends. Concrete implementations (CLIDisplay,
    // FTXUIDisplay) are added later; this is what callers pass to choose which
    // one to construct.
    enum class DisplayType
    {
        CLI,
        FTXUI,
    };

    // Abstract frontend.
    //
    // A Display owns the user-facing input loop and renders log output. The REPL
    // logic itself stays in CommandExecutor and is driven through the onCommand
    // callback handed to run(), so the same command set works behind either a
    // plain CLI or a full TUI.
    class Display
    {
    public:
        virtual ~Display() = default;

        // Run the main input loop. `onCommand` is invoked for each input line;
        // returning true ends the loop.
        virtual void run(std::function<bool(const std::string &)> onCommand) = 0;

        // Update the prompt shown before input.
        virtual void setPrompt(const std::string &prompt) = 0;

        // Render a single log entry (wired as Log's global callback).
        virtual void onLog(const Loggable &entry) = 0;

        // Ask the display to stop its run loop.
        virtual void quit() = 0;

        // Notify that shared state changed (devices discovered/lost).
        virtual void onStateChanged() {}

        // Give the display access to the command executor (command menus, etc.).
        virtual void bindCommandSource(Commands::CommandExecutor *) {}
    };

    // Construct the chosen frontend. `state` is only needed by the TUI backend,
    // but is taken uniformly so callers don't special-case it.
    std::unique_ptr<Display> makeDisplay(DisplayType type, SharedState &state);

}
