/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_EARNEDAUCTIONPOLICY_H
#define PLAYERBOTS_EARNEDAUCTIONPOLICY_H

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>
#include <tuple>

namespace EarnedAuction
{
constexpr std::uint32_t MaxListings = 20;
constexpr std::uint32_t MaxSalesPerVisit = 2;
constexpr std::uint32_t MaxSameItemListings = 2;
constexpr std::uint32_t MaxMarketScan = 512;
constexpr std::uint32_t MaxMailPerVisit = 20;
constexpr std::uint32_t MaxCopper = 0x7fffffff;
// Stock that expires unsold this often is vendored instead of paying more deposits.
constexpr std::uint32_t MaxUnsoldListings = 2;
constexpr std::int64_t UnsoldMemorySeconds = 7 * 24 * 3600;

// A bounded valuation of count items keeps ordinary buyers from subsidising
// arbitrary asking prices: the vendor's price per item, as vendors sell BuyCount
// items for BuyPrice, or six times the vendor sell price, whichever is higher.
inline std::uint32_t PriceCeiling(std::uint32_t vendorBuy, std::uint32_t buyCount, std::uint32_t vendorSell,
                                  std::uint32_t count)
{
    std::uint64_t const bundle = std::max<std::uint32_t>(buyCount, 1);
    std::uint64_t const vendor = (std::uint64_t(vendorBuy) * count + bundle - 1) / bundle;
    std::uint64_t const resale = std::min<std::uint64_t>(std::uint64_t(vendorSell) * 6, MaxCopper) * count;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(std::max(vendor, resale), MaxCopper));
}

struct ListingPrices
{
    std::uint32_t bid = 0;
    std::uint32_t buyout = 0;
};

inline ListingPrices PriceListing(std::uint32_t vendorBuy, std::uint32_t buyCount, std::uint32_t vendorSell,
                                  std::uint32_t count, std::uint32_t marketUnitPrice)
{
    if (!vendorSell || !count)
        return {};
    // Undercut the cheapest comparable offer. A market at twice the vendor price or
    // below leaves no room, and a listing above it would not sell first.
    std::uint64_t const floor = std::min<std::uint64_t>(std::uint64_t(vendorSell) * 2, MaxCopper) * count;
    std::uint64_t const unit = marketUnitPrice ? marketUnitPrice - 1 : std::uint64_t(vendorSell) * 3;
    std::uint64_t const buyout = std::min<std::uint64_t>(std::min<std::uint64_t>(unit, MaxCopper) * count,
                                                         PriceCeiling(vendorBuy, buyCount, vendorSell, count));
    std::uint64_t const bid = std::max<std::uint64_t>(std::uint64_t(vendorSell) * count + 1, buyout * 4 / 5);
    if (!buyout || buyout < floor || buyout > MaxCopper || bid > buyout)
        return {};
    return {static_cast<std::uint32_t>(bid), static_cast<std::uint32_t>(buyout)};
}

// A sale, after the house cut, must beat vendoring the stack by at least the
// deposit, which an expired listing loses.
inline bool WorthListing(std::uint32_t buyout, std::uint32_t cut, std::uint64_t vendorValue, std::uint32_t deposit)
{
    return buyout > cut && std::uint64_t(buyout - cut) >= vendorValue + std::max<std::uint32_t>(deposit, 1);
}

inline bool Affordable(std::uint32_t price, std::uint32_t budget, std::uint32_t money)
{
    return price && price <= MaxCopper && price <= budget && price <= money;
}

inline bool WithinPriceCeiling(std::uint32_t price, std::uint32_t ceiling) { return price && price <= ceiling; }

// A bid wins only when its auction ends. Bots bid on auctions ending before their
// next visits, so gold another bidder outbids returns quickly.
inline bool BidWindow(std::int64_t timeLeft, std::uint32_t interval)
{
    return timeLeft > 0 && timeLeft <= 2 * std::int64_t(interval);
}

// The core's lowest acceptable bid: the start bid, or the current bid plus its increment.
inline std::uint32_t MinimumBid(std::uint32_t startbid, std::uint32_t bid, std::uint32_t increment)
{
    std::uint64_t const next = bid ? std::uint64_t(bid) + increment : startbid;
    return next <= MaxCopper ? static_cast<std::uint32_t>(next) : 0;
}

// One purchase per visit goes to the most useful tier: gear upgrades, ammunition,
// consumables, then gathering tools. Within a tier the offer cheapest against its
// valuation wins, so a cheap item does not beat a bargain upgrade.
struct OfferRank
{
    std::uint32_t tier = 0;
    std::uint64_t relative = 0;
    std::uint32_t price = 0;
};

inline OfferRank RankOffer(std::uint32_t tier, std::uint32_t price, std::uint32_t ceiling)
{
    return {tier, ceiling ? std::uint64_t(price) * 1000 / ceiling : UINT64_MAX, price};
}

inline bool BetterOffer(OfferRank offer, OfferRank best)
{
    return std::tie(offer.tier, offer.relative, offer.price) < std::tie(best.tier, best.relative, best.price);
}

struct AuctionSubject
{
    std::uint32_t item = 0;
    std::uint32_t response = 0;
    std::uint32_t auction = 0;
    std::uint32_t count = 0;
};

// Reads the core's auction mail subject, "item:0:response:auction:count".
inline std::optional<AuctionSubject> ParseAuctionSubject(std::string_view subject)
{
    std::array<std::uint32_t, 5> fields{};
    for (std::size_t index = 0; index < fields.size(); ++index)
    {
        bool const final = index + 1 == fields.size();
        std::size_t const end = final ? subject.size() : subject.find(':');
        if (!end || end == std::string_view::npos)
            return {};
        char const* const last = subject.data() + end;
        auto const [parsed, error] = std::from_chars(subject.data(), last, fields[index]);
        if (error != std::errc() || parsed != last)
            return {};
        subject.remove_prefix(final ? end : end + 1);
    }
    if (fields[1])
        return {};
    return AuctionSubject{fields[0], fields[2], fields[3], fields[4]};
}

struct UnsoldRecord
{
    std::uint32_t unsold = 0;
    std::int64_t last = 0;
};

// The memory outlasts the listing rounds it compares, however long they run.
inline bool UnsoldForgotten(UnsoldRecord record, std::int64_t now, std::int64_t listing)
{
    return now - record.last >= std::max<std::int64_t>(UnsoldMemorySeconds, 3 * listing);
}

inline bool RelistAllowed(UnsoldRecord record, std::int64_t now, std::int64_t listing)
{
    return record.unsold < MaxUnsoldListings || UnsoldForgotten(record, now, listing);
}

// Stacks listed in one round return within a listing duration of its first expiry
// and count once. A refusal, with no round, always counts.
inline void CountUnsold(UnsoldRecord& record, std::int64_t now, std::int64_t round)
{
    if (record.unsold && now - record.last < round)
        return;
    ++record.unsold;
    record.last = now;
}

inline bool ReceiptReady(bool auctionMail, bool deleted, std::uint32_t cod, std::int64_t delivered,
                         std::int64_t expires, std::int64_t now)
{
    return auctionMail && !deleted && !cod && delivered <= now && expires > now;
}
}  // namespace EarnedAuction
#endif
