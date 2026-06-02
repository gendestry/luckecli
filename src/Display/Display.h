#pragma once
#include <string>
#include <functional>

struct Loggable;
class CommandExecutor;

class Display {
public:
    virtual ~Display() = default;

    // Run the main input loop.
    // onCommand is called for each input line; return true to exit.
    virtual void run(std::function<bool(const std::string&)> onCommand) = 0;

    // Update the prompt shown before input
    virtual void setPrompt(const std::string& prompt) = 0;

    // Handle a log entry (wired as Log's global callback)
    virtual void onLog(const Loggable& entry) = 0;

    // Signal the display to stop its run loop
    virtual void quit() = 0;

    // Notify that shared state changed (devices discovered/lost)
    virtual void onStateChanged() {}

    // Give the display access to the command executor (for command menus etc.)
    virtual void bindCommandSource(CommandExecutor*) {}
};
