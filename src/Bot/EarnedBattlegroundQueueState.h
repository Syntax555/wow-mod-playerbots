/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_EARNEDBATTLEGROUNDQUEUESTATE_H
#define PLAYERBOTS_EARNEDBATTLEGROUNDQUEUESTATE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

#include "SharedDefines.h"

class EarnedBattlegroundQueueState
{
public:
    static constexpr uint8 ERA_COUNT = 3;

    void Record(uint32 queue, uint32 bracket, uint8 era, TeamId team, bool isBot, bool waiting, uint32 instanceId)
    {
        if (!IsValid(queue, bracket, era, team))
            return;

        auto& entry = _queues[queue][bracket][era];
        entry.participants[team].fetch_add(1, std::memory_order_relaxed);
        if (!isBot)
            ++entry.players;
        if (waiting)
            entry.activeQueue = true;
        if (instanceId &&
            std::find(entry.instances.begin(), entry.instances.end(), instanceId) == entry.instances.end())
            entry.instances.push_back(instanceId);
    }

    bool NeedsPlayer(uint32 queue, uint32 bracket, uint8 era, TeamId team, uint32 teamSize) const
    {
        if (!IsValid(queue, bracket, era, team))
            return false;

        auto const& entry = _queues[queue][bracket][era];
        uint32 target = teamSize * (uint32(entry.activeQueue) + uint32(entry.instances.size()));
        return entry.players && entry.participants[team].load(std::memory_order_relaxed) < target;
    }

    bool TryReserve(uint32 queue, uint32 bracket, uint8 era, TeamId team, uint32 count, uint32 teamSize)
    {
        if (!IsValid(queue, bracket, era, team) || !count)
            return false;

        auto& entry = _queues[queue][bracket][era];
        uint32 target = teamSize * (uint32(entry.activeQueue) + uint32(entry.instances.size()));
        if (!entry.players || count > target)
            return false;

        uint32 current = entry.participants[team].load(std::memory_order_relaxed);
        while (current <= target - count)
            if (entry.participants[team].compare_exchange_weak(current, current + count, std::memory_order_relaxed))
                return true;
        return false;
    }

private:
    struct Entry
    {
        std::array<std::atomic<uint32>, 2> participants{};
        uint32 players = 0;
        bool activeQueue = false;
        std::vector<uint32> instances;
    };

    static bool IsValid(uint32 queue, uint32 bracket, uint8 era, TeamId team)
    {
        return queue < MAX_BATTLEGROUND_QUEUE_TYPES && bracket < MAX_BATTLEGROUND_BRACKETS && era < ERA_COUNT &&
               (team == TEAM_ALLIANCE || team == TEAM_HORDE);
    }

    std::array<std::array<std::array<Entry, ERA_COUNT>, MAX_BATTLEGROUND_BRACKETS>, MAX_BATTLEGROUND_QUEUE_TYPES>
        _queues{};
};

#endif
