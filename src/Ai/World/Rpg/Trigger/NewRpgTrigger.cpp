/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MaintenanceValues.h"
#include "NewRpgTriggers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "TravelMgr.h"

bool NewRpgStatusTrigger::IsActive() { return status == botAI->rpgInfo.GetStatus(); }

bool NewRpgNaturalMaintenanceTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.naturalProgression)
        return false;
    NaturalMaintenanceTask const task = AI_VALUE(NaturalMaintenanceTask, "natural maintenance");
    return !task.target.IsEmpty() || task.type == NaturalMaintenanceType::AUCTION_TRAVEL;
}

bool NewRpgDiscoverFlightTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.discoverFlightMasters || !bot->IsAlive() || bot->IsInCombat() || bot->IsInFlight() ||
        bot->GetTransport())
        return false;

    // Not while boarding or using a link of a route.
    ContinentRouteState const& route = botAI->continentRoute;
    if (route.hasLink && route.usedAt)
        return false;

    return sTravelMgr.FindUndiscoveredFlightMaster(bot, botAI->rpgInfo.flightMasterRejectedAt) != nullptr;
}
