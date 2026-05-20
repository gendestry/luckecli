//
// Created by bobi on 20. 05. 26.
//

#include "Log.h"

#include <iostream>

#include "Loggable.h"

std::mutex Log::logMutex;

Loggable::Type Log::s_CallbackThreshold = Loggable::Type::PRINT;
std::list<Loggable> Log::s_log;
std::unordered_map<Loggable::Type, std::list<std::reference_wrapper<Loggable>>> Log::map;
std::unordered_map<std::string, std::list<std::reference_wrapper<Loggable>>> Log::mapScopes;

std::function<void(const Loggable&)> Log::s_onChangeCallback = [](const Loggable& l) {
    std::cout << l.toString();
};