//
// Created by bobi on 8. 05. 26.
//

#pragma once
#include <string>
#include <utility>
#include "Utils/Text/String.h"
#include "Utils/Text/Stream.h"

class JSONtemp {

    // ---------------------------
    // STRING HANDLING (SAFE)
    // ---------------------------

    static std::string escapeString(const std::string& s) {
        return Utils::String::concat("\"", s, "\"");
    }

    static std::string testString(const std::string& arg) {
        return escapeString(arg);
    }

    static std::string testString(const char* arg) {
        return escapeString(std::string(arg));
    }

    template <typename T>
    static std::string testString(const T& arg) {
        return Utils::String::format("{}", arg);
    }

    // ---------------------------
    // KEY-VALUE APPENDING
    // ---------------------------

    static void append(Utils::Text::Stream& out,
                        bool& first,
                        const std::string& key,
                        const std::string& value) {
        if (!first) out << ", ";
        first = false;

        out << testString(key) << ": " << testString(value);
    }

    template <typename T>
    static void append(Utils::Text::Stream& out,
                        bool& first,
                        const std::string& key,
                        const T& value) {
        if (!first) out << ", ";
        first = false;

        out << testString(key) << ": " << testString(value);
    }

    // ---------------------------
    // VARIADIC RECURSION
    // ---------------------------

    static void keyvalueImpl(Utils::Text::Stream&) {}

    template <typename T, typename... Args>
    static void keyvalueImpl(Utils::Text::Stream& out,
                              bool& first,
                              const std::string& key,
                              const T& value,
                              Args&&... args) {
        append(out, first, key, value);

        if constexpr (sizeof...(args) > 0) {
            keyvalueImpl(out, first, std::forward<Args>(args)...);
        }
    }

public:

    // ---------------------------
    // MAIN ENTRY
    // ---------------------------

    template <typename... Args>
    static std::string stringify(const std::string& request, Args&&... args) {
        Utils::Text::Stream out;

        out << R"({"request": )" << testString(request);

        if constexpr (sizeof...(args) > 0) {
            out << ", ";

            bool first = true;
            keyvalueImpl(out, first, std::forward<Args>(args)...);
        }

        out << "}";

        return out.end();
    }
};

//
// class JSONtemp {
//
//     template <typename T>
//     static std::string testString(const T& arg) {
//         return Utils::String::format("{}", arg);
//     }
//
//     static std::string testString(const std::string& arg) {
//         if (arg == "true" || arg == "false") { return arg;}
//         if (arg[0] == '"' && arg[arg.length() - 1] == '"') { return arg; }
//         std::cerr << arg << std::endl;
//         return Utils::String::concat("\"", arg, "\"");
//     }
//
//     static std::string keyvalue() {
//         return "";
//     }
//
//     static std::string keyvalue(std::string key, std::string value) {
//         Utils::Text::Stream ret;
//         ret << ", " << testString(key) << ": " << testString(value);
//         return ret.end();
//     }
//
//     template <typename T>
//     static std::string keyvalue(std::string key, T value) {
//         Utils::Text::Stream ret;
//         ret << ", " << testString(key) << ": " << testString(value);
//         return ret.end();
//     }
//
//     template <typename... Args>
//     static std::string keyvalue(std::string key, std::string value, Args&&... args) {
//         Utils::Text::Stream ret;
//         ret << keyvalue(key, value);
//         ret << keyvalue(args...);
//         return ret.end();
//     }
//
//     template <typename T, typename... Args>
//     static std::string keyvalue(std::string key, T value, Args&&... args) {
//         Utils::Text::Stream ret;
//         ret << keyvalue(key, value);
//         ret << keyvalue(args...);
//         return ret.end();
//     }
//
// public:
// template <typename... Args>
//     static std::string stringify(const std::string& request, Args&&... args) {
//         Utils::Text::Stream ret;
//         ret << R"({"request": )";
//         ret << testString(request);
//         ret << keyvalue(std::forward<Args>(args)...);
//         ret << "}";
//         return ret.end();
//     }
// };
