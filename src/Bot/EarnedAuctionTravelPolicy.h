/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_EARNEDAUCTIONTRAVELPOLICY_H
#define PLAYERBOTS_EARNEDAUCTIONTRAVELPOLICY_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace EarnedAuctionTravel
{
constexpr std::uint32_t MinimumLevel = 10;
constexpr std::uint32_t MinimumPurchaseCopper = 100;
constexpr std::uint32_t MaximumTravelTimeMs = 10 * 60 * 1000;
constexpr std::uint32_t TravelBackoffMs = 15 * 60 * 1000;
constexpr std::uint32_t RejectedTargetMs = 60 * 60 * 1000;
constexpr float MaximumTravelDistance = 5000.0f;
constexpr std::uint32_t NeutralHouseId = 7;

struct MarketSite
{
    std::uint32_t mapId = 0;
    std::uint32_t phaseMask = 0;
    std::uint32_t houseId = 0;
    std::uint32_t factionTemplateId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float orientation = 0.0f;
};

struct MarketOrigin
{
    std::uint32_t mapId = 0;
    std::uint32_t phaseMask = 0;
    std::uint32_t houseId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline bool CanStartTravel(std::uint32_t level, std::uint32_t auctionLevel, bool hasSurplus,
                           std::uint32_t spendableCopper)
{
    return level >= std::max(MinimumLevel, auctionLevel) && (hasSurplus || spendableCopper >= MinimumPurchaseCopper);
}

inline bool CompatibleHouse(std::uint32_t botHouseId, std::uint32_t marketHouseId)
{
    return botHouseId && marketHouseId && (marketHouseId == botHouseId || marketHouseId == NeutralHouseId);
}

inline float DistanceSquared(MarketOrigin const& bot, MarketSite const& market)
{
    float const dx = bot.x - market.x;
    float const dy = bot.y - market.y;
    float const dz = bot.z - market.z;
    return dx * dx + dy * dy + dz * dz;
}

inline bool EligibleMarket(MarketOrigin const& bot, MarketSite const& market)
{
    float const distance = DistanceSquared(bot, market);
    return market.mapId == bot.mapId && (market.phaseMask & bot.phaseMask) &&
           CompatibleHouse(bot.houseId, market.houseId) && std::isfinite(distance) &&
           distance <= MaximumTravelDistance * MaximumTravelDistance;
}

template <typename FriendlyPredicate>
std::optional<std::size_t> NearestMarketIndex(std::span<MarketSite const> markets, MarketOrigin const& bot,
                                              FriendlyPredicate friendly)
{
    std::optional<std::size_t> nearest;
    float nearestDistance = MaximumTravelDistance * MaximumTravelDistance;
    for (std::size_t i = 0; i < markets.size(); ++i)
    {
        MarketSite const& market = markets[i];
        if (!EligibleMarket(bot, market) || !friendly(market))
            continue;
        float const distance = DistanceSquared(bot, market);
        if (!nearest || distance < nearestDistance)
        {
            nearest = i;
            nearestDistance = distance;
        }
    }
    return nearest;
}

inline bool TravelDeadlineReached(std::uint32_t elapsedMs) { return elapsedMs >= MaximumTravelTimeMs; }

inline bool TravelBackoffElapsed(std::uint32_t elapsedMs) { return elapsedMs >= TravelBackoffMs; }

inline bool RejectionActive(std::uint32_t elapsedMs) { return elapsedMs < RejectedTargetMs; }
}  // namespace EarnedAuctionTravel
#endif
