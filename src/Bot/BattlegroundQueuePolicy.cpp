/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BattlegroundQueuePolicy.h"

#include "BattlegroundMgr.h"

#if __has_include("IndividualProgressionBattlegrounds.h")
#include "IndividualProgressionBattlegrounds.h"
#endif

bool IsPlayerbotBattlegroundQueueAllowed(Player* player, bool vanillaOnly, bool earnedEra,
                                         BattlegroundQueueTypeId queueTypeId)
{
    if (!IsPlayerbotBattlegroundQueueAllowed(vanillaOnly, earnedEra, queueTypeId))
        return false;

    if (!earnedEra)
        return true;

#if __has_include("IndividualProgressionBattlegrounds.h")
    return player && IndividualProgression_CanJoinBattleground(player, BattlegroundMgr::BGTemplateId(queueTypeId));
#else
    (void)player;
    return false;
#endif
}

uint8 GetPlayerbotBattlegroundEra(Player* player)
{
#if __has_include("IndividualProgressionBattlegrounds.h")
    return player ? IndividualProgression_BattlegroundEra(player) : uint8(255);
#else
    (void)player;
    return 255;
#endif
}
