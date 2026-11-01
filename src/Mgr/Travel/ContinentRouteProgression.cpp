/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ContinentRouteProgression.h"

#include "AreaDefines.h"
#include "DBCStores.h"
#include "Player.h"
#include "WorldSession.h"

#if __has_include("IndividualProgression.h")
#include "IndividualProgression.h"

namespace
{
// Individual Progression's starting-zone rule for characters without earned TBC.
bool IsTbcRaceStartingZone(uint32 zoneId)
{
    return zoneId == AREA_AZUREMYST_ISLE || zoneId == AREA_BLOODMYST_ISLE || zoneId == AREA_GHOSTLANDS ||
           zoneId == AREA_EVERSONG_WOODS || zoneId == AREA_THE_EXODAR || zoneId == AREA_SILVERMOON_CITY ||
           zoneId == AREA_AMMEN_VALE || zoneId == AREA_VEILED_SEA;
}
}  // namespace
#endif

bool ContinentRouteProgression::CanArrive(Player* bot, uint32 mapId, float x, float y, float z, uint32 zoneId)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    if (!entry || entry->Expansion() > bot->GetSession()->Expansion())
        return false;

#if __has_include("IndividualProgression.h")
    if (!sIndividualProgression->enabled || bot->IsGameMaster())
        return true;

    // Without earned TBC, Individual Progression reads the account from the database before it
    // applies this rule.
    if (mapId == MAP_OUTLAND && !sIndividualProgression->hasPassedProgression(bot, PROGRESSION_PRE_TBC))
        return (bot->getRace() == RACE_BLOODELF || bot->getRace() == RACE_DRAENEI) && IsTbcRaceStartingZone(zoneId);

    // Outside strict mode its account filters may query the login database.
    if (!sIndividualProgression->strictEarnedProgression)
        return mapId != MAP_NORTHREND || sIndividualProgression->hasPassedProgression(bot, PROGRESSION_TBC_TIER_5);

    return sIndividualProgression->CanEnterMap(bot, mapId, x, y, z);
#else
    (void)x;
    (void)y;
    (void)z;
    (void)zoneId;
    return true;
#endif
}
