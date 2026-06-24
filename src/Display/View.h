#pragma once
#include <atomic>
#include <string>
#include <functional>

struct Loggable;
namespace ftxui { class App; }

// A full-screen FTXUI view (e.g. the fixture grid or the detail dashboard).
// Each view owns the event loop for its own screen. The coordinator
// (FTXUIDisplay) routes the thread-facing Display callbacks to whichever view
// is currently active.
class View {
public:
    virtual ~View() = default;

    // Runs this view's screen loop until the user navigates away or quits.
    virtual void run(std::function<bool(const std::string&)>& onCommand) = 0;

    // Invoked from other threads (heartbeat discovery / log). Default: redraw.
    virtual void onStateChanged() { redraw(); }
    virtual void onLog(const Loggable&) {}

    // Stop this view's loop (used when the whole app is quitting).
    void requestExit();

protected:
    // Force a repaint of the current frame. Safe to call from any thread.
    void redraw();

    // The active screen while run() is looping; null otherwise.
    std::atomic<ftxui::App*> m_app{nullptr};
};

// Strip ANSI escape sequences from a string (for log/prompt text).
std::string stripAnsi(const std::string& s);
