//
// Created by bobi on 20. 05. 26.
//

#pragma once
#include <format>
#include <string>

#include "Utils/Colors/Font.h"

struct Loggable {
    enum Type : uint8_t {
        NOTSET     = 0U,
        DEBUG      = 1U,
        PRINT      = 2U,
        ERROR      = 3U,
    } type;
    std::string message;
    std::string scope;

    std::string typeToString() const {
        switch (type) {
            case Loggable::DEBUG:
                return "DEBUG";
            case Loggable::PRINT:
                return "PRINT";
            case Loggable::ERROR:
                return "ERROR";
            case Loggable::NOTSET:
                return "NOTSET";
        }
        return "UNKNOWN";
    }

    std::string toString() const {
        if (type == DEBUG) {
            return std::format("{}{}{}{}", Utils::Font::colorItalic, Utils::Font::colorByRGB(140,140,140), message, Utils::Font::colorReset);
        }

        return message;
    }

    std::string operator<<(const Loggable& l) const{
        return l.toString();
    }
};
