//
// Created by bobi on 17. 05. 26.
//

#ifndef LUCKECLI_HELP_H
#define LUCKECLI_HELP_H
#include <format>
#include <string>

#include "Utils/Colors/Font.h"

template<typename... Args>
std::string print(const std::string& format, Args&&... args)
{
    auto msg = std::vformat(format, std::make_format_args(args...));
    return msg;
}

template<typename... Args>
std::string println(const std::string& format, Args&&... args)
{
    auto msg = std::vformat(format, std::make_format_args(args...));
    return msg + "\n";
}

// template<typename... Args>
// std::string printColor(const std::string& color,
//                 const std::string& format,
//                 Args&&... args)
// {
//     auto msg = std::vformat(format, std::make_format_args(args...));
//     std::print("{}{}{}{}", scopePadding(), color, msg, Font::colorReset);
// }
//
// template<typename... Args>
// std::string printlnColor(const std::string& color,
//                   const std::string& format,
//                   Args&&... args)
// {
//     auto msg = std::vformat(format, std::make_format_args(args...));
//     std::println("{}{}{}{}", scopePadding(), color, msg, Font::colorReset);
// }

template<typename... Args>
std::string debug(const std::string& format, Args&&... args)
{
    auto msg = std::vformat(format, std::make_format_args(args...));
    return std::format("{}{}{}{}", Utils::Font::colorItalic, Utils::Font::colorByRGB(140,140,140), msg, Utils::Font::colorReset);
}

template<typename... Args>
std::string error(const std::string& format, Args&&... args)
{
    auto msg = std::vformat(format, std::make_format_args(args...));
    return std::format("{}{}{}", Utils::Font::colorRed, msg, Utils::Font::colorReset);
}

#endif //LUCKECLI_HELP_H