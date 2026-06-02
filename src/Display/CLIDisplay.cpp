#include "CLIDisplay.h"
#include "Diary/Loggable.h"
#include <iostream>

void CLIDisplay::run(std::function<bool(const std::string&)> onCommand) {
    std::cout << m_prompt << std::flush;
    std::string line;
    while (!m_quit && std::getline(std::cin, line)) {
        if (onCommand(line)) break;
        std::cout << m_prompt << std::flush;
    }
}

void CLIDisplay::setPrompt(const std::string& prompt) {
    m_prompt = prompt;
}

void CLIDisplay::onLog(const Loggable& entry) {
    std::cout << entry.toString();
}

void CLIDisplay::quit() {
    m_quit = true;
}
