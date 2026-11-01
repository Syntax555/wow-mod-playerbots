/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TravelTriggers.h"

#include "ContinentRoutes.h"
#include "Playerbots.h"

bool NoTravelTargetTrigger::IsActive() { return !context->GetValue<TravelTarget*>("travel target")->Get()->isActive(); }

bool FarFromTravelTargetTrigger::IsActive()
{
    return context->GetValue<TravelTarget*>("travel target")->Get()->isTraveling();
}

bool MasterFarAwayTrigger::IsActive()
{
    if (!sPlayerbotAIConfig.continentTravel || IsSelfBot(bot) || !bot->IsAlive() || bot->IsInFlight() ||
        bot->GetMap()->Instanceable())
        return false;

    Player* master = botAI->GetMaster();
    if (!master || master == bot || !master->IsInWorld() || master->IsBeingTeleported() || master->IsInFlight() ||
        master->GetTransport() || master->GetMap()->Instanceable())
        return false;

    if (master->GetMapId() != bot->GetMapId())
        return sContinentRoutes.Connects(master->GetMapId()) && sContinentRoutes.Connects(bot->GetMapId());

    return bot->GetExactDist2d(master) > ContinentTravel::CompanionFarYards;
}
