/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EARNEDLEVELBRACKETPOLICY_H
#define PLAYERBOTS_EARNEDLEVELBRACKETPOLICY_H

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

// Stable GUID buckets reserve bots for earned caps without changing character state.
class EarnedLevelBracketPolicy
{
    static constexpr unsigned BucketCount = 100;
    static constexpr unsigned MaxLevel = 80;
    static constexpr std::uint32_t GuidHashMultiplier = 2654435761;

public:
    bool Load(std::string_view config)
    {
        _caps = {};
        std::array<std::uint8_t, BucketCount> caps{};
        std::array<bool, MaxLevel + 1> used{};
        unsigned total = 0;
        config = Trim(config);
        while (!config.empty())
        {
            auto const comma = config.find(',');
            auto const entry = config.substr(0, comma);
            auto const colon = entry.find(':');
            unsigned level = 0;
            unsigned percentage = 0;
            if (colon == std::string_view::npos || !Number(entry.substr(0, colon), level) ||
                !Number(entry.substr(colon + 1), percentage) || level < 1 || level > MaxLevel || percentage < 1 ||
                percentage > BucketCount || total + percentage > BucketCount || used[level])
                return false;

            used[level] = true;
            for (unsigned end = total + percentage; total < end; ++total)
                caps[total] = static_cast<std::uint8_t>(level);

            if (comma == std::string_view::npos)
                break;
            config = Trim(config.substr(comma + 1));
            if (config.empty())
                return false;
        }
        _caps = caps;
        return true;
    }

    std::uint8_t GetCap(std::uint32_t guid, std::uint8_t currentLevel) const
    {
        // Unsigned wraparound is deliberate and stable across platforms/restarts.
        auto const cap = _caps[(guid * GuidHashMultiplier) % _caps.size()];
        return currentLevel <= cap ? cap : 0;  // Never downgrade an existing higher-level bot.
    }

private:
    static std::string_view Trim(std::string_view value)
    {
        auto const first = value.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos)
            return {};
        return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    }

    static bool Number(std::string_view value, unsigned& number)
    {
        value = Trim(value);
        if (value.empty())
            return false;
        auto const result = std::from_chars(value.data(), value.data() + value.size(), number);
        return result.ec == std::errc{} && result.ptr == value.data() + value.size();
    }

    std::array<std::uint8_t, BucketCount> _caps{};
};

#endif
