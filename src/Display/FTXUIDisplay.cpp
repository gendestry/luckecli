#include "FTXUIDisplay.h"
#include "CommandExecutor.h"

// m_grid / m_detail bind a reference to m_exec, so they always see the pointer
// set later by bindCommandSource(). (m_exec is declared before them.)
FTXUIDisplay::FTXUIDisplay(SharedState& state)
    : m_state(state), m_grid(m_state, m_exec), m_detail(m_exec) {}

void FTXUIDisplay::bindCommandSource(CommandExecutor* exec) {
    m_exec = exec;
}

void FTXUIDisplay::setPrompt(const std::string& prompt) {
    m_prompt = stripAnsi(prompt);
}

void FTXUIDisplay::setActive(View* v) {
    std::lock_guard lock(m_routeMutex);
    m_active = v;
}

void FTXUIDisplay::run(std::function<bool(const std::string&)> onCommand) {
    m_quit = false;
    while (!m_quit) {
        setActive(&m_grid);
        m_grid.run(onCommand);
        setActive(nullptr);

        if (m_quit) break;

        if (m_exec && m_exec->isSelected()) {
            setActive(&m_detail);
            m_detail.run(onCommand);
            setActive(nullptr);
        }
    }
}

void FTXUIDisplay::onStateChanged() {
    std::lock_guard lock(m_routeMutex);
    if (m_active) m_active->onStateChanged();
}

void FTXUIDisplay::onLog(const Loggable& entry) {
    std::lock_guard lock(m_routeMutex);
    if (m_active) m_active->onLog(entry);
}

void FTXUIDisplay::quit() {
    m_quit = true;
    std::lock_guard lock(m_routeMutex);
    if (m_active) m_active->requestExit();
}
