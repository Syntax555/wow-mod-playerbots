/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_EARNEDAUCTIONTRAVEL_H
#define PLAYERBOTS_EARNEDAUCTIONTRAVEL_H

#include "TravelMgr.h"

class Creature;
class PlayerbotAI;

namespace EarnedAuctionTravel
{
void LoadMarkets();
bool Ready(PlayerbotAI* ai);
bool CompatibleAuctioneer(PlayerbotAI* ai, Creature* auctioneer);
WorldPosition PlanTrip(PlayerbotAI* ai);
bool ContinueTrip(PlayerbotAI* ai);
void FinishTrip(PlayerbotAI* ai);
void AbortTrip(PlayerbotAI* ai);
void PostponeVisit(PlayerbotAI* ai);
void RejectVisit(PlayerbotAI* ai, ObjectGuid target);
ObjectGuid RejectedTarget(PlayerbotAI* ai);
}  // namespace EarnedAuctionTravel
#endif
