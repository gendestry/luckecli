#pragma once
#include <string>

#include "core/commands/Args.h"
#include "Utils/Text/String.h"

namespace core::commands
{

    // Turns a raw user line into typed Args. For now this is the existing
    // split-on-spaces behaviour, wrapped so there's a single place to later add
    // quote handling ("ssid \"my net\"") without touching any handler.
    inline Args tokenize(const std::string &line)
    {
        return Args(::Utils::String::split(::Utils::String::normalize_spaces(line), " "));
    }

}
