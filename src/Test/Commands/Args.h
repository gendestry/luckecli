#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include "Utils/Text/String.h"

namespace Test::Commands
{

    // Typed view over a tokenized command line. Handlers take an `Args` instead
    // of a raw vector<string>, so the repeated index math / size checks / int
    // parsing / "key <value>" walking all live here once instead of in every
    // handler. Owns its tokens (cheap; command lines are tiny).
    class Args
    {
        std::vector<std::string> tokens_;

    public:
        Args() = default;
        explicit Args(std::vector<std::string> tokens) : tokens_(std::move(tokens)) {}

        // ── basics ──────────────────────────────────────────────
        size_t size() const { return tokens_.size(); }
        bool empty() const { return tokens_.empty(); }

        // The command word (tokens_[0]) or "" if the line was empty.
        const std::string &command() const
        {
            static const std::string none;
            return tokens_.empty() ? none : tokens_[0];
        }

        // Positional token or "" if out of range — never throws / OOB.
        const std::string &operator[](size_t i) const
        {
            static const std::string none;
            return i < tokens_.size() ? tokens_[i] : none;
        }

        bool has(size_t i) const { return i < tokens_.size(); }

        // ── typed access ────────────────────────────────────────
        bool isInt(size_t i) const
        {
            return i < tokens_.size() && ::Utils::String::isInt(tokens_[i]);
        }

        std::optional<int> getInt(size_t i) const
        {
            if (!isInt(i))
                return std::nullopt;
            return std::stoi(tokens_[i]);
        }

        // ── key/value access ────────────────────────────────────
        // For "ssid <value> pass <value>"-style lines: returns the token after
        // the first occurrence of `key`. nullopt if `key` is absent or has no
        // following value. Matches against any of the given aliases.
        std::optional<std::string> value(std::initializer_list<std::string_view> keys) const
        {
            for (size_t i = 0; i + 1 < tokens_.size(); ++i)
                for (auto k : keys)
                    if (tokens_[i] == k)
                        return tokens_[i + 1];
            return std::nullopt;
        }

        std::optional<std::string> value(std::string_view key) const
        {
            return value({key});
        }

        // ── raw escape hatch ────────────────────────────────────
        const std::vector<std::string> &raw() const { return tokens_; }

        auto begin() const { return tokens_.begin(); }
        auto end() const { return tokens_.end(); }
    };

}
