/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EarnedAuctionTravel.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "AiObjectContext.h"
#include "AuctionHouseMgr.h"
#include "BudgetValues.h"
#include "Creature.h"
#include "DBCStores.h"
#include "EarnedAuction.h"
#include "EarnedAuctionTravelPolicy.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Timer.h"
#include "World.h"

namespace EarnedAuctionTravel
{
namespace
{
using MarketCache = std::unordered_map<uint32, std::vector<MarketSite>>;
std::atomic<std::shared_ptr<MarketCache const>> marketCache;

uint32 PurchaseSavings(PlayerbotAI* ai)
{
    AiObjectContext* context = ai->GetAiObjectContext();
    uint32 const gear = context->GetValue<uint32>("free money for", uint32(NeedMoneyFor::gear))->Get();
    uint32 const supplies = context->GetValue<uint32>("money needed for", uint32(NeedMoneyFor::consumables))->Get();
    uint32 const consumables = context->GetValue<uint32>("free money for", uint32(NeedMoneyFor::consumables))->Get();
    return std::min(std::max(gear > supplies ? gear - supplies : 0, consumables), ai->GetBot()->GetMoney() / 4);
}

bool FriendlyMarket(Player* bot, MarketSite const& market)
{
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(market.factionTemplateId);
    return faction && bot->GetFactionReactionTo(faction, bot) >= REP_NEUTRAL;
}
}  // namespace

void LoadMarkets()
{
    auto cache = std::make_shared<MarketCache>();
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(data.id);
        MapEntry const* map = sMapStore.LookupEntry(data.mapid);
        if (!creature || !map || map->IsDungeon() || !data.spawnMask)
            continue;
        uint32 npcFlags = 0;
        uint32 unitFlags = 0;
        uint32 dynamicFlags = 0;
        ObjectMgr::ChooseCreatureFlags(creature, npcFlags, unitFlags, dynamicFlags, &data);
        if (!(npcFlags & UNIT_NPC_FLAG_AUCTIONEER))
            continue;
        AuctionHouseEntry const* house = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(creature->faction);
        if (!house)
            continue;
        (*cache)[data.mapid].push_back({data.mapid, data.phaseMask, house->houseId, creature->faction, data.posX,
                                        data.posY, data.posZ, data.orientation});
    }
    // Publish immutable spawn metadata; never inspect the world's spawn store from a bot map thread.
    marketCache.store(std::move(cache), std::memory_order_release);
}

bool Ready(PlayerbotAI* ai)
{
    if (!EarnedAuction::IsEnabled(ai))
        return false;
    Player* bot = ai->GetBot();
    return bot->GetLevel() >= MinimumLevel && bot->IsInWorld() && bot->IsAlive() && !bot->GetGroup() &&
           !bot->IsInCombat() && !bot->IsInFlight() && !bot->IsBeingTeleported() && !bot->InBattleground() &&
           !bot->GetTradeData() && bot->GetMap() && !bot->GetMap()->IsDungeon() && ai->CanMove() &&
           ai->GetAiObjectContext()->GetValue<bool>("can move around")->Get();
}

bool CompatibleAuctioneer(PlayerbotAI* ai, Creature* auctioneer)
{
    if (!ai || !ai->GetBot() || !EarnedAuction::MeetsAuctionLevel(ai->GetBot()) || !auctioneer ||
        !auctioneer->IsAlive() || !auctioneer->HasNpcFlag(UNIT_NPC_FLAG_AUCTIONEER) ||
        auctioneer->IsHostileTo(ai->GetBot()))
        return false;
    AuctionHouseEntry const* botHouse =
        AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(ai->GetBot()->GetFaction());
    AuctionHouseEntry const* marketHouse =
        AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(auctioneer->GetFaction());
    return botHouse && marketHouse && CompatibleHouse(botHouse->houseId, marketHouse->houseId);
}

void FinishTrip(PlayerbotAI* ai)
{
    if (!ai || !ai->rpgInfo.earnedAuctionTravelActive)
        return;
    auto& info = ai->rpgInfo;
    info.earnedAuctionTravelActive = false;
    info.earnedAuctionTravelLastAttempt = getMSTime();
    info.earnedAuctionTravelClockStarted = true;
    info.earnedAuctionTravelDestination = {};
}

bool ContinueTrip(PlayerbotAI* ai)
{
    if (!ai || !ai->rpgInfo.earnedAuctionTravelActive)
        return false;
    auto& info = ai->rpgInfo;
    if (!Ready(ai) || info.earnedAuctionTravelDestination.GetMapId() != ai->GetBot()->GetMapId() ||
        TravelDeadlineReached(GetMSTimeDiffToNow(info.earnedAuctionTravelStarted)))
    {
        AbortTrip(ai);
        return false;
    }
    return true;
}

void AbortTrip(PlayerbotAI* ai)
{
    if (ai && ai->rpgInfo.earnedAuctionTravelActive)
        PostponeVisit(ai);
}

void PostponeVisit(PlayerbotAI* ai)
{
    if (!ai)
        return;
    FinishTrip(ai);
    ai->rpgInfo.earnedAuctionLastVisit = getMSTime();
    ai->rpgInfo.earnedAuctionVisitDelay = TravelBackoffMs;
    // A failed turn passes to the other visit kind, as a completed one does.
    ai->rpgInfo.earnedAuctionVisitMailbox = !ai->rpgInfo.earnedAuctionVisitMailbox;
}

// An unreachable mailbox or auctioneer would otherwise be chosen for every visit
// while it stays nearest. Skip it for a while, so another can serve.
void RejectVisit(PlayerbotAI* ai, ObjectGuid target)
{
    if (!ai)
        return;
    PostponeVisit(ai);
    ai->rpgInfo.earnedAuctionRejected = target;
    ai->rpgInfo.earnedAuctionRejectedAt = getMSTime();
}

ObjectGuid RejectedTarget(PlayerbotAI* ai)
{
    auto const& info = ai->rpgInfo;
    return RejectionActive(GetMSTimeDiffToNow(info.earnedAuctionRejectedAt)) ? info.earnedAuctionRejected
                                                                             : ObjectGuid::Empty;
}

WorldPosition PlanTrip(PlayerbotAI* ai)
{
    if (ContinueTrip(ai))
        return ai->rpgInfo.earnedAuctionTravelDestination;
    if (!Ready(ai))
        return {};
    auto& info = ai->rpgInfo;
    if (info.earnedAuctionTravelClockStarted &&
        !TravelBackoffElapsed(GetMSTimeDiffToNow(info.earnedAuctionTravelLastAttempt)))
        return {};
    info.earnedAuctionTravelClockStarted = true;
    info.earnedAuctionTravelLastAttempt = getMSTime();
    Player* bot = ai->GetBot();
    if (!CanStartTravel(bot->GetLevel(), sWorld->getIntConfig(CONFIG_AUCTION_LEVEL_REQ), EarnedAuction::HasSurplus(ai),
                        PurchaseSavings(ai)))
        return {};
    auto const cache = marketCache.load(std::memory_order_acquire);
    if (!cache)
        return {};
    auto const markets = cache->find(bot->GetMapId());
    if (markets == cache->end())
        return {};
    AuctionHouseEntry const* botHouse = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(bot->GetFaction());
    if (!botHouse)
        return {};
    MarketOrigin const origin{bot->GetMapId(),     bot->GetPhaseMask(), botHouse->houseId,
                              bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()};
    auto const nearest = NearestMarketIndex(markets->second, origin,
                                            [bot](MarketSite const& site) { return FriendlyMarket(bot, site); });
    if (!nearest)
        return {};
    MarketSite const& market = markets->second[*nearest];
    info.earnedAuctionTravelDestination = WorldPosition(market.mapId, market.x, market.y, market.z, market.orientation);
    info.earnedAuctionTravelStarted = getMSTime();
    info.earnedAuctionTravelActive = true;
    return info.earnedAuctionTravelDestination;
}
}  // namespace EarnedAuctionTravel
