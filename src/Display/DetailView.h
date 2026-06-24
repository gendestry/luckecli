#pragma once
#include "View.h"
#include <functional>
#include <mutex>
#include <string>

class CommandExecutor;

// Per-fixture dashboard: editable fields, engine toggles, action buttons.
// Config + engine state are fetched on a background worker so opening the view
// is instant; results are marshalled back onto the UI thread.
class DetailView : public View {
public:
    explicit DetailView(CommandExecutor*& exec);

    void run(std::function<bool(const std::string&)>& onCommand) override;
    void onLog(const Loggable& entry) override;   // surface command feedback

private:
    CommandExecutor*& m_exec;
    std::mutex m_mutex;            // guards m_status / m_statusError
    std::string m_status;          // latest command feedback shown in the view
    bool m_statusError = false;
};
