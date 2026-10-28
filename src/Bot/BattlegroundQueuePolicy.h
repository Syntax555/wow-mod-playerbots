/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BATTLEGROUNDQUEUEPOLICY_H
#define PLAYERBOTS_BATTLEGROUNDQUEUEPOLICY_H

#include "SharedDefines.h"

class Player;

constexpr bool IsPlayerbotBattlegroundQueueAllowed(bool vanillaOnly, BattlegroundQueueTypeId queueTypeId)
{
    return !vanillaOnly || queueTypeId == BATTLEGROUND_QUEUE_AV || queueTypeId == BATTLEGROUND_QUEUE_WS ||
           queueTypeId == BATTLEGROUND_QUEUE_AB;
}

constexpr bool IsPlayerbotEarnedBattlegroundQueueSupported(BattlegroundQueueTypeId queueTypeId)
{
    return queueTypeId == BATTLEGROUND_QUEUE_AV || queueTypeId == BATTLEGROUND_QUEUE_WS ||
           queueTypeId == BATTLEGROUND_QUEUE_AB || queueTypeId == BATTLEGROUND_QUEUE_EY ||
           queueTypeId == BATTLEGROUND_QUEUE_IC;
}

constexpr bool IsPlayerbotBattlegroundQueueAllowed(bool vanillaOnly, bool earnedEra,
                                                   BattlegroundQueueTypeId queueTypeId)
{
    return IsPlayerbotBattlegroundQueueAllowed(vanillaOnly, queueTypeId) &&
           (!earnedEra || IsPlayerbotEarnedBattlegroundQueueSupported(queueTypeId));
}

bool IsPlayerbotBattlegroundQueueAllowed(Player* player, bool vanillaOnly, bool earnedEra,
                                         BattlegroundQueueTypeId queueTypeId);
uint8 GetPlayerbotBattlegroundEra(Player* player);

#endif
