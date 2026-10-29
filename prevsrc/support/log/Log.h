#pragma once
#include <list>
#include <format>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "support/log/Loggable.h"


class Log {
    using LogLevel = Loggable::Type;
    static std::mutex logMutex;

    std::string m_scope = "";

    Loggable::Type m_CallbackThreshold = LogLevel::NOTSET;
    std::function<bool(const Loggable&)> m_onChangeCallback;
    static Loggable::Type s_CallbackThreshold;
    static std::function<void(const Loggable&)> s_onChangeCallback;

    static std::list<Loggable> s_log;
    std::list<std::reference_wrapper<Loggable>> m_log;

    static std::unordered_map<Loggable::Type, std::list<std::reference_wrapper<Loggable>>> map;
    static std::unordered_map<std::string, std::list<std::reference_wrapper<Loggable>>> mapScopes;

    std::string scopePadding() const {return "";}

    template<typename... Args>
    void append(Loggable::Type type, bool newline, const std::string& format, Args&&... args) {
        std::lock_guard lock(logMutex);
        auto msg = std::vformat(format, std::make_format_args(args...));
        std::string message;
        if (newline) {
            message = std::format("{}{}\n", scopePadding(), msg);
        }
        else {
            message = std::format("{}{}", scopePadding(), msg);
        }
        s_log.emplace_back(Loggable{type, message, m_scope});
        auto& blog = s_log.back();
        m_log.push_back(blog);
        map[type].push_back(blog);
        mapScopes[blog.scope].push_back(blog);

        const auto usedLevel = m_CallbackThreshold == Loggable::NOTSET ? s_CallbackThreshold : m_CallbackThreshold;
        if (type >= usedLevel) {
            if (!m_onChangeCallback(blog) && type >= s_CallbackThreshold) {
                s_onChangeCallback(blog);
            }
        }
    }
public:
    Log(const std::string& currentScope = "", const std::function<bool(const Loggable&)>& localOnChangeCallback = [](const Loggable&){ return false; })
        : m_scope(currentScope), m_onChangeCallback(localOnChangeCallback)
    {}

    void setLocalLevel(LogLevel type) {
        m_CallbackThreshold = type;
    }

    static void setGlobalLevel(LogLevel type) {
        s_CallbackThreshold = type;
    }

    void setCallback(const std::function<bool(const Loggable&)>& callback) {
        m_onChangeCallback = std::move(callback);
    }

    static void setGlobalCallback(std::function<void(const Loggable&)> callback = [](const Loggable&){}) {
        // Lock so the swap can't race with append() invoking the callback — lets
        // shutdown reset this to a no-op before the bound display is destroyed.
        std::lock_guard lock(logMutex);
        s_onChangeCallback = std::move(callback);
    }

    static const std::list<Loggable>& getLog() {
        return s_log;
    }

    const std::list<std::reference_wrapper<Loggable>>& getLocalLog() const {
        return m_log;
    }

    static const std::unordered_map<Loggable::Type, std::list<std::reference_wrapper<Loggable>>>& getMap() {
        return map;
    }

    template<typename... Args>
    void print(const std::string& format, Args&&... args)
    {
        append(Loggable::PRINT, false, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void println(const std::string& format, Args&&... args)
    {
        append(Loggable::PRINT, true, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& format, Args&&... args)
    {
        append(Loggable::DEBUG, true, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& format, Args&&... args)
    {
        append(Loggable::ERROR, true, format, std::forward<Args>(args)...);
    }

};
