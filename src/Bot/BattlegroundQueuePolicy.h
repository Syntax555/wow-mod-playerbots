/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BATTLEGROUNDQUEUEPOLICY_H
#define PLAYERBOTS_BATTLEGROUNDQUEUEPOLICY_H

#include "SharedDefines.h"

constexpr bool IsPlayerbotBattlegroundQueueAllowed(bool vanillaOnly, BattlegroundQueueTypeId queueTypeId)
{
    return !vanillaOnly || queueTypeId == BATTLEGROUND_QUEUE_AV || queueTypeId == BATTLEGROUND_QUEUE_WS ||
           queueTypeId == BATTLEGROUND_QUEUE_AB;
}

#endif
