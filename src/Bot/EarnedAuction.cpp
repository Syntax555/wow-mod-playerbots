/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#include "EarnedAuction.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "AuctionHouseMgr.h"
#include "Bag.h"
#include "BudgetValues.h"
#include "CharacterCache.h"
#include "EarnedAuctionPolicy.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Item.h"
#include "Mail.h"
#include "MaintenanceValues.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "PlayerbotOperation.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Playerbots.h"
#include "QuestDef.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace EarnedAuction
{
namespace
{
// The inventory helpers inspect the bot's own map-owned state. Auction and mail
// collections are inspected only by EarnedAuctionVisit::Execute on the world thread.
std::vector<ObjectGuid> BagItems(Player* bot)
{
    std::vector<ObjectGuid> items;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            items.push_back(item->GetGUID());
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        if (Bag* bag = dynamic_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)))
            for (uint32 position = 0; position < bag->GetBagSize(); ++position)
                if (Item* item = bag->GetItemByPos(position))
                    items.push_back(item->GetGUID());
    return items;
}

ItemUsage Usage(PlayerbotAI* ai, Item* item)
{
    // Random suffixes/properties are part of the item actually offered, not a
    // newly rolled template. Refresh between transactions to reflect changed inventory.
    std::string const qualifier =
        std::to_string(item->GetEntry()) + "," + std::to_string(item->GetItemRandomPropertyId());
    Value<ItemUsage>* value = ai->GetAiObjectContext()->GetValue<ItemUsage>("item usage", qualifier);
    value->Reset();
    return value->Get();
}

// Random bots without a player do not craft. Human-owned characters keep the
// reagents of their recipes.
bool Crafts(PlayerbotAI* ai) { return !sRandomPlayerbotMgr.IsRandomBot(ai->GetBot()) || ai->HasGameClientMaster(); }

// A decision inspects several bag items without changing quests or learned spells.
// Build their protection sets lazily once for that decision, never across ticks,
// players or map/world threads. Inventory-dependent pet-food reserves remain live.
class ProgressionSupplies
{
public:
    explicit ProgressionSupplies(PlayerbotAI* ai) : _bot(ai->GetBot()), _crafts(Crafts(ai)) {}

    bool NeededForQuest(Item* item)
    {
        // Resolve this item's create-item effects at most once, and only when
        // an active item objective makes them relevant.
        bool effectsLoaded = false;
        std::vector<uint32> createdItems;
        auto needed = [&]()
        {
            if (_questItems.contains(item->GetEntry()))
                return true;
            if (_questObjectives.empty())
                return false;
            if (!effectsLoaded)
            {
                effectsLoaded = true;
                for (auto const& spell : item->GetTemplate()->Spells)
                {
                    SpellInfo const* info = spell.SpellId > 0 ? sSpellMgr->GetSpellInfo(spell.SpellId) : nullptr;
                    if (!info)
                        continue;
                    for (auto const& effect : info->Effects)
                        if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
                            createdItems.push_back(effect.ItemType);
                }
            }
            return std::any_of(createdItems.begin(), createdItems.end(),
                               [&](uint32 created) { return _questObjectives.contains(created); });
        };
        if (needed())
            return true;
        // Resume after the quests already inspected by a previous item. Stop on
        // an early match rather than building the rest of an unneeded snapshot.
        while (_questSlot < MAX_QUEST_LOG_SIZE)
        {
            uint32 const id = _bot->GetQuestSlotQuestId(_questSlot++);
            Quest const* quest = id ? sObjectMgr->GetQuestTemplate(id) : nullptr;
            if (!quest)
                continue;
            if (uint32 const source = quest->GetSrcItemId())
                _questItems.insert(source);
            for (uint32 required : quest->RequiredItemId)
                if (required)
                {
                    _questItems.insert(required);
                    _questObjectives.insert(required);
                }
            for (uint32 source : quest->ItemDrop)
                if (source)
                    _questItems.insert(source);
            if (needed())
                return true;
        }
        return false;
    }

    bool IsReagent(uint32 itemId)
    {
        if (_reagents.contains(itemId))
            return true;
        if (_reagentsLoaded)
            return false;
        auto const& spells = _bot->GetSpellMap();
        if (!_reagentsStarted)
        {
            _reagentsStarted = true;
            _reagentCursor = spells.cbegin();
        }
        // Never list _reagents required by a currently learned active spell. Bots that
        // do not craft leave recipe reagents to ordinary item usage.
        // An early match is cached without scanning later unrelated spells.
        while (_reagentCursor != spells.cend())
        {
            auto const& [spellId, spell] = *_reagentCursor++;
            if (spell->State == PLAYERSPELL_REMOVED || !spell->Active)
                continue;
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (!info || info->IsPassive() || (!_crafts && IsProfessionSpell(spellId)))
                continue;
            for (uint8 reagent = 0; reagent < MAX_SPELL_REAGENTS; ++reagent)
                if (info->Reagent[reagent] > 0)
                    _reagents.insert(uint32(info->Reagent[reagent]));
            if (_reagents.contains(itemId))
                return true;
        }
        _reagentsLoaded = true;
        return false;
    }

private:
    Player* _bot;
    bool _crafts;
    uint8 _questSlot = 0;
    bool _reagentsStarted = false;
    bool _reagentsLoaded = false;
    decltype(std::declval<Player const&>().GetSpellMap().cbegin()) _reagentCursor;
    std::unordered_set<uint32> _questItems;
    std::unordered_set<uint32> _questObjectives;
    std::unordered_set<uint32> _reagents;
};

bool IsPetFood(Player* bot, ItemTemplate const* proto)
{
    if (!proto->FoodType || proto->FoodType > 32)
        return false;
    if (Pet* pet = bot->GetPet(); pet && pet->HaveInDiet(proto))
        return true;

    // Dismissed, stabled and temporarily unsummoned pets still need their food.
    PetStable const* stable = bot->GetPetStable();
    if (!stable)
        return false;
    auto inDiet = [proto](PetStable::PetInfo const& pet)
    {
        if (pet.Type != HUNTER_PET)
            return false;
        CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(pet.CreatureId);
        CreatureFamilyEntry const* family = creature ? sCreatureFamilyStore.LookupEntry(creature->family) : nullptr;
        return family && (family->petFoodMask & (1u << (proto->FoodType - 1)));
    };
    if (stable->CurrentPet && inDiet(*stable->CurrentPet))
        return true;
    return std::any_of(stable->StabledPets.begin(), stable->StabledPets.end(),
                       [&inDiet](auto const& pet) { return pet && inDiet(*pet); }) ||
           std::any_of(stable->UnslottedPets.begin(), stable->UnslottedPets.end(), inDiet);
}

bool NeededForProgression(PlayerbotAI* ai, Item* item, ProgressionSupplies& supplies)
{
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    if (!proto)
        return false;
    if (proto->Class == ITEM_CLASS_QUEST || proto->Class == ITEM_CLASS_KEY || proto->Bonding == BIND_QUEST_ITEM)
        return true;
    Player* bot = ai->GetBot();
    // The upstream usage predicate asks whether more quest loot is needed;
    // after collecting enough it can classify the completed objective as AH
    // surplus. Keep active objectives and their source items until turn-in.
    if (supplies.NeededForQuest(item))
        return true;
    // Keep the last pet-food supply, even when the generic usage classifier
    // regards it as a food the hunter no longer needs for themself.
    if (IsPetFood(bot, proto) &&
        std::uint64_t(bot->GetItemCount(item->GetEntry(), false)) < std::uint64_t(item->GetCount()) + 20)
        return true;

    return supplies.IsReagent(item->GetEntry());
}

bool CanList(PlayerbotAI* ai, Item* item)
{
    if (!item || item->IsEquipped() || item->IsInTrade() || !item->CanBeTraded() || item->IsNotEmptyBag() ||
        item->IsSoulBound() || item->GetOwnerGUID() != ai->GetBot()->GetGUID() || !item->GetCount() ||
        item->GetUInt32Value(ITEM_FIELD_DURATION))
        return false;
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || !proto->SellPrice || proto->Quality < ITEM_QUALITY_NORMAL || proto->Class == ITEM_CLASS_QUEST ||
        proto->Class == ITEM_CLASS_KEY || proto->HasFlag(ITEM_FLAG_CONJURED) || proto->Bonding == BIND_QUEST_ITEM)
        return false;

    return true;
}

// Each bot's unsold auction stock, learned on the world thread from its own expired
// receipts and from listings it refused. Map-thread surplus decisions read it, so
// access is locked. The record lives with the server process.
class UnsoldStock
{
public:
    using Key = std::tuple<ObjectGuid::LowType, uint32, int32>;

    void Record(Key const& key, time_t now, std::int64_t round, std::int64_t listing)
    {
        std::unique_lock lock(_lock);
        std::erase_if(_records, [&](auto const& record) { return UnsoldForgotten(record.second, now, listing); });
        CountUnsold(_records[key], now, round);
    }

    // A sale shows demand for the item, whatever its random property.
    void Sold(ObjectGuid::LowType bot, uint32 entry)
    {
        std::unique_lock lock(_lock);
        _records.erase(_records.lower_bound({bot, entry, std::numeric_limits<int32>::min()}),
                       _records.upper_bound({bot, entry, std::numeric_limits<int32>::max()}));
    }

    bool Allowed(Key const& key, time_t now, std::int64_t listing) const
    {
        std::shared_lock lock(_lock);
        auto const found = _records.find(key);
        return found == _records.end() || RelistAllowed(found->second, now, listing);
    }

private:
    mutable std::shared_mutex _lock;
    std::map<Key, UnsoldRecord> _records;
};

UnsoldStock& Unsold()
{
    static UnsoldStock stock;
    return stock;
}

UnsoldStock::Key StockKey(Player* bot, Item* item)
{
    return {bot->GetGUID().GetCounter(), item->GetEntry(), item->GetItemRandomPropertyId()};
}

// The core's listing duration, which Rate.Auction.Time scales.
std::int64_t ListingSeconds()
{
    double const seconds = double(MIN_AUCTION_TIME) * sWorld->getRate(RATE_AUCTION_TIME);
    return std::isfinite(seconds) && seconds > 0 ? std::int64_t(std::min<double>(seconds, UINT32_MAX)) : 0;
}

bool RelistAllowed(PlayerbotAI* ai, Item* item)
{
    return Unsold().Allowed(StockKey(ai->GetBot(), item), GameTime::GetGameTime().count(), ListingSeconds());
}

// The core's own cut of a sale at this price.
uint32 SaleCut(AuctionHouseEntry const* house, uint32 price)
{
    AuctionEntry sale{};
    sale.bid = price;
    sale.auctionHouseEntry = house;
    return sale.GetAuctionCut();
}

uint32 NeutralDeposit(Item* item)
{
    AuctionHouseEntry const* neutral = AuctionHouseMgr::GetAuctionHouseEntryFromHouse(AuctionHouseId::Neutral);
    return neutral ? AuctionHouseMgr::GetAuctionDeposit(neutral, MIN_AUCTION_TIME, item, item->GetCount()) : 0;
}

// Whether a listing at the ordinary price could repay its deposit, judged by the
// neutral house's deposit and cut, the largest. The core's static rules read only
// item, DBC and configuration data, never live auction collections.
bool Listable(Item* item)
{
    float const rate = sWorld->getRate(RATE_AUCTION_DEPOSIT);
    if (!std::isfinite(rate) || rate < 0)
        return false;
    ItemTemplate const* proto = item->GetTemplate();
    uint32 const count = item->GetCount();
    ListingPrices const prices = PriceListing(proto->BuyPrice, proto->BuyCount, proto->SellPrice, count, 0);
    AuctionHouseEntry const* neutral = AuctionHouseMgr::GetAuctionHouseEntryFromHouse(AuctionHouseId::Neutral);
    return prices.buyout && neutral &&
           WorthListing(prices.buyout, SaleCut(neutral, prices.buyout), uint64(proto->SellPrice) * count,
                        NeutralDeposit(item));
}

bool TradableSurplus(PlayerbotAI* ai, Item* item, ProgressionSupplies& supplies)
{
    // Item usage can inspect equipment, skills and inventories. Reject items that
    // cannot be listed before evaluating it.
    return CanList(ai, item) && RelistAllowed(ai, item) && Listable(item) && Usage(ai, item) == ITEM_USAGE_AH &&
           !NeededForProgression(ai, item, supplies);
}

uint32 FreeBudget(PlayerbotAI* ai, NeedMoneyFor category)
{
    Player* bot = ai->GetBot();
    Value<uint32>* value = ai->GetAiObjectContext()->GetValue<uint32>("free money for", uint32(category));
    value->Reset();
    return std::min(bot->GetMoney(), value->Get());
}

uint32 PurchaseBudget(PlayerbotAI* ai, ItemUsage usage)
{
    NeedMoneyFor category = NeedMoneyFor::gear;
    if (usage == ITEM_USAGE_AMMO)
        category = NeedMoneyFor::ammo;
    else if (usage == ITEM_USAGE_USE)
        category = NeedMoneyFor::consumables;
    else if (usage == ITEM_USAGE_SKILL)
        category = NeedMoneyFor::tradeskill;
    uint32 const free = FreeBudget(ai, category);
    uint32 const supplies =
        ai->GetAiObjectContext()->GetValue<uint32>("money needed for", uint32(NeedMoneyFor::consumables))->Get();
    uint32 const available = category == NeedMoneyFor::consumables ? free : (free > supplies ? free - supplies : 0);
    // A single purchase must not consume most of a bot's earned savings.
    return std::min(available, ai->GetBot()->GetMoney() / 4);
}

bool Ready(PlayerbotAI* ai)
{
    if (!IsEnabled(ai))
        return false;
    Player* bot = ai->GetBot();
    return bot->IsInWorld() && bot->IsAlive() && !bot->IsInCombat() && !bot->IsInFlight() &&
           !bot->IsBeingTeleported() && !bot->InBattleground() && !bot->GetTradeData() &&
           ai->GetAiObjectContext()->GetValue<bool>("can move around")->Get();
}

bool AuctionReceipt(Mail const* mail, time_t now)
{
    return mail && ReceiptReady(mail->messageType == MAIL_AUCTION, mail->state == MAIL_STATE_DELETED, mail->COD,
                                mail->deliver_time, mail->expire_time, now);
}

bool CollectReceipts(Player* bot, ObjectGuid mailbox)
{
    WorldPacket inbox(CMSG_GET_MAIL_LIST);
    inbox << mailbox;
    bot->GetSession()->HandleGetMailList(inbox);
    time_t const now = GameTime::GetGameTime().count();
    std::vector<uint32> mailIds;
    for (Mail const* mail : bot->GetMails())
        if (AuctionReceipt(mail, now))
        {
            mailIds.push_back(mail->messageID);
            if (mailIds.size() >= MaxMailPerVisit)
                break;
        }

    bool collected = false;
    for (uint32 id : mailIds)
    {
        Mail* mail = bot->GetMail(id);
        if (!AuctionReceipt(mail, now))
            continue;
        std::optional<AuctionSubject> const subject = ParseAuctionSubject(mail->subject);
        if (subject && subject->response == AUCTION_SUCCESSFUL)
            Unsold().Sold(bot->GetGUID().GetCounter(), subject->item);
        bool const expired = subject && subject->response == AUCTION_EXPIRED;
        // The core mails a return when its auction expires, so stacks of one round share it.
        time_t const returned = mail->deliver_time;
        if (mail->money)
        {
            uint32 const before = mail->money;
            WorldPacket packet(CMSG_MAIL_TAKE_MONEY);
            packet << mailbox << id;
            bot->GetSession()->HandleMailTakeMoney(packet);
            mail = bot->GetMail(id);
            if (!mail)
                continue;
            collected = collected || mail->money < before;
        }
        std::vector<uint32> itemGuids;
        for (MailItemInfo const& item : mail->items)
            itemGuids.push_back(item.item_guid);
        for (uint32 itemGuid : itemGuids)
        {
            Item* item = bot->GetMItem(itemGuid);
            ItemPosCountVec destination;
            if (!item || bot->CanStoreItem(NULL_BAG, NULL_SLOT, destination, item, false) != EQUIP_ERR_OK)
                continue;
            UnsoldStock::Key const stock = StockKey(bot, item);
            WorldPacket packet(CMSG_MAIL_TAKE_ITEM);
            packet << mailbox << id << itemGuid;
            bot->GetSession()->HandleMailTakeItem(packet);
            // The core may merge/delete the attachment; never reuse its pointer.
            bool const taken = !bot->GetMItem(itemGuid);
            collected = collected || taken;
            // A returned stack counts once per listing round, timed by its return.
            if (expired)
                Unsold().Record(stock, returned, ListingSeconds(), ListingSeconds());
        }
        mail = bot->GetMail(id);
        // HandleMailDelete does not protect attachments. Only delete after
        // observing successful core retrieval of every attachment and all money.
        if (AuctionReceipt(mail, now) && !mail->money && mail->items.empty())
        {
            WorldPacket packet(CMSG_MAIL_DELETE);
            packet << mailbox << id << uint32(0);
            bot->GetSession()->HandleMailDelete(packet);
        }
    }
    return collected;
}

using StockItem = std::pair<uint32, int32>;

StockItem Stock(Item const* item) { return {item->GetEntry(), item->GetItemRandomPropertyId()}; }

struct MarketState
{
    uint32 listings = 0;
    std::map<StockItem, uint32> listingsByItem;
    bool pendingPurchase = false;
};

MarketState OwnMarketState(Player* bot)
{
    MarketState state;
    std::unordered_set<AuctionHouseObject*> visited;
    for (AuctionHouseId houseId : {AuctionHouseId::Alliance, AuctionHouseId::Horde, AuctionHouseId::Neutral})
    {
        AuctionHouseObject* house = sAuctionMgr->GetAuctionsMapByHouseId(houseId);
        if (!house || !visited.insert(house).second)
            continue;
        // Ownership census is deliberately exact, including auctions from an
        // earlier server session. No item valuation occurs in this inexpensive pass.
        for (auto const& [id, auction] : house->GetAuctions())
        {
            if (auction->owner == bot->GetGUID())
            {
                ++state.listings;
                if (Item const* listed = sAuctionMgr->GetAItem(auction->item_guid))
                    ++state.listingsByItem[Stock(listed)];
            }
            if (auction->bidder == bot->GetGUID())
                state.pendingPurchase = true;
            if (state.listings >= MaxListings && state.pendingPurchase)
                return state;
        }
    }
    time_t const now = GameTime::GetGameTime().count();
    for (Mail const* mail : bot->GetMails())
        if (mail && mail->messageType == MAIL_AUCTION && mail->state != MAIL_STATE_DELETED && mail->expire_time > now &&
            !mail->items.empty())
        {
            // Returned stock is no purchase; an unreadable subject counts as one.
            std::optional<AuctionSubject> const subject = ParseAuctionSubject(mail->subject);
            if (!subject)
                state.pendingPurchase = true;
            else if (subject->response == AUCTION_WON)
                for (MailItemInfo const& attachment : mail->items)
                {
                    Item* item = bot->GetMItem(attachment.item_guid);
                    if (!item || !item->GetTemplate())
                    {
                        state.pendingPurchase = true;
                        continue;
                    }
                    // An ownership limit is not cleared by making bag space. Keep the
                    // attachment for ordinary collection without blocking other buys.
                    InventoryResult const result = bot->CanTakeMoreSimilarItems(item);
                    if (result != EQUIP_ERR_CANT_CARRY_MORE_OF_THIS &&
                        result != EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED)
                        state.pendingPurchase = true;
                }
        }
    return state;
}

std::vector<uint32> SampleMarket(AuctionHouseObject* house, ObjectGuid botGuid)
{
    std::vector<uint32> ids;
    auto const& auctions = house->GetAuctions();
    if (auctions.empty())
        return ids;
    // Different bots examine different rotating slices, rather than all
    // competing for the first item in a large auction house.
    std::uint64_t const seed =
        std::uint64_t(GameTime::GetGameTime().count() / 60) + std::uint64_t(botGuid.GetCounter()) * 2654435761u;
    std::uint64_t const low = auctions.begin()->first;
    std::uint64_t const span = std::uint64_t(auctions.rbegin()->first) - low + 1;
    uint32 const slice = uint32(low + seed % span);
    auto it = auctions.lower_bound(slice);
    if (it == auctions.end())
        it = auctions.begin();
    auto const first = it;
    do
    {
        ids.push_back(it->first);
        if (ids.size() >= MaxMarketScan)
            break;
        if (++it == auctions.end())
            it = auctions.begin();
    } while (it != first);
    return ids;
}

// The cheapest unit buyout of each item the bot carries, among the whole house's
// live offers from other accounts. Its own and same-account listings are no
// competition to undercut.
std::map<StockItem, uint32> MarketPrices(Player* bot, AuctionHouseObject* house)
{
    std::set<uint32> carried;
    for (ObjectGuid guid : BagItems(bot))
        if (Item* item = bot->GetItemByGuid(guid))
            carried.insert(item->GetEntry());
    std::map<StockItem, uint32> prices;
    time_t const now = GameTime::GetGameTime().count();
    for (auto const& [id, auction] : house->GetAuctions())
    {
        if (!carried.contains(auction->item_template) || auction->expire_time <= now || !auction->buyout ||
            !auction->itemCount)
            continue;
        uint32 const ownerAccount = sCharacterCache->GetCharacterAccountIdByGuid(auction->owner);
        Item* offer = sAuctionMgr->GetAItem(auction->item_guid);
        uint32 const unit = auction->buyout / auction->itemCount;
        if (!ownerAccount || ownerAccount == bot->GetSession()->GetAccountId() || !offer || !unit)
            continue;
        auto const [found, inserted] = prices.try_emplace(Stock(offer), unit);
        if (!inserted && unit < found->second)
            found->second = unit;
    }
    return prices;
}

bool SellSurplus(PlayerbotAI* ai, Creature* auctioneer, AuctionHouseObject* house, MarketState const& state)
{
    bool sold = false;
    uint32 listed = 0;
    uint32 listings = state.listings;
    std::map<StockItem, uint32> listingsByItem = state.listingsByItem;
    AuctionHouseEntry const* entry = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(auctioneer->GetFaction());
    if (!entry)
        return false;
    Player* bot = ai->GetBot();
    std::map<StockItem, uint32> const market = MarketPrices(bot, house);
    std::set<StockItem> refused;
    time_t const now = GameTime::GetGameTime().count();
    ProgressionSupplies supplies(ai);
    for (ObjectGuid guid : BagItems(bot))
    {
        if (listed >= MaxSalesPerVisit || listings >= MaxListings)
            break;
        Item* item = bot->GetItemByGuid(guid);
        if (!TradableSurplus(ai, item, supplies) || listingsByItem[Stock(item)] >= MaxSameItemListings)
            continue;
        uint32 const count = item->GetCount();
        ItemTemplate const* proto = item->GetTemplate();
        auto const offer = market.find(Stock(item));
        ListingPrices const prices = PriceListing(proto->BuyPrice, proto->BuyCount, proto->SellPrice, count,
                                                  offer == market.end() ? 0 : offer->second);
        uint32 const deposit = sAuctionMgr->GetAuctionDeposit(entry, MIN_AUCTION_TIME, item, count);
        if (!prices.buyout ||
            !WorthListing(prices.buyout, SaleCut(entry, prices.buyout), uint64(proto->SellPrice) * count, deposit))
        {
            // This market leaves no sale that repays the deposit. Refusals count like
            // unsold listings, once per item and visit, so the vendor takes the stock.
            if (refused.insert(Stock(item)).second)
                Unsold().Record(StockKey(bot, item), now, 0, ListingSeconds());
            continue;
        }
        uint32 const budget = FreeBudget(ai, NeedMoneyFor::anything);
        if (deposit > budget || !bot->HasEnoughMoney(deposit))
            continue;
        StockItem const stock = Stock(item);
        WorldPacket packet(CMSG_AUCTION_SELL_ITEM);
        packet << auctioneer->GetGUID() << uint32(1) << guid << count;
        packet << prices.bid << prices.buyout << uint32(MIN_AUCTION_TIME / MINUTE);
        bot->GetSession()->HandleAuctionSellItem(packet);
        // A whole-stack listing removes the owned bag item. Failed listings
        // retain it, and must not be counted or have their gold restored.
        if (!bot->GetItemByGuid(guid))
        {
            sold = true;
            ++listed;
            ++listings;
            ++listingsByItem[stock];
        }
    }
    return sold;
}

// Natural bots gather but do not craft. Mining and skinning tools carry these
// TotemCategory.dbc tool categories, multi-tools included; fishing takes a pole.
bool GatheringTool(ItemTemplate const* proto)
{
    if (proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        return true;
    switch (proto->TotemCategory)
    {
        case 161:  // Gnomish Army Knife
        case 165:  // Mining Pick
        case 166:  // Skinning Knife
        case 167:  // Hammer Pick
        case 168:  // Bladed Pickaxe
            return true;
        default:
            return false;
    }
}

uint32 VisitInterval() { return std::clamp<uint32>(sPlayerbotAIConfig.earnedAuctionInterval, 60, 3600); }

// Bots do not outbid a player's lead.
bool BotBidder(ObjectGuid bidder)
{
    uint32 const account = sCharacterCache->GetCharacterAccountIdByGuid(bidder);
    return account && sPlayerbotAIConfig.IsInRandomAccountList(account);
}

bool BuyUseful(PlayerbotAI* ai, Creature* auctioneer, AuctionHouseObject* house, std::vector<uint32> const& sample)
{
    Player* bot = ai->GetBot();
    time_t const now = GameTime::GetGameTime().count();
    uint32 bestId = 0;
    OfferRank best;
    for (uint32 id : sample)
    {
        AuctionEntry* auction = house->GetAuction(id);
        // The core refuses a buyout below the start bid, and a bid-only auction waits for
        // its bid window before its item is evaluated.
        if (!auction || auction->expire_time <= now || auction->owner == bot->GetGUID() ||
            auction->bidder == bot->GetGUID() || (auction->buyout && auction->buyout < auction->startbid) ||
            (!auction->buyout && !BidWindow(auction->expire_time - now, VisitInterval())))
            continue;
        uint32 price = auction->buyout;
        uint32 const ownerAccount = sCharacterCache->GetCharacterAccountIdByGuid(auction->owner);
        if (!ownerAccount || ownerAccount == bot->GetSession()->GetAccountId())
            continue;
        Item* item = sAuctionMgr->GetAItem(auction->item_guid);
        if (!item || item->GetCount() != auction->itemCount || !auction->itemCount)
            continue;
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->HasFlag(ITEM_FLAG_CONJURED) || proto->Class == ITEM_CLASS_QUEST ||
            proto->Class == ITEM_CLASS_KEY || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;
        ItemUsage const usage = Usage(ai, item);
        uint32 tier = 0;
        if (usage == ITEM_USAGE_AMMO)
            tier = 1;
        else if (usage == ITEM_USAGE_USE && proto->Class == ITEM_CLASS_CONSUMABLE)
            tier = 2;
        else if (usage == ITEM_USAGE_SKILL && GatheringTool(proto))
            tier = 3;
        else if (usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_REPLACE)
            continue;
        uint32 const budget = PurchaseBudget(ai, usage);
        uint32 const ceiling = PriceCeiling(proto->BuyPrice, proto->BuyCount, proto->SellPrice, auction->itemCount);
        if (!Affordable(auction->buyout, budget, bot->GetMoney()) || !WithinPriceCeiling(auction->buyout, ceiling))
        {
            // Without an acceptable buyout, bid the core's minimum on an auction that ends
            // before the next visits, unless a player leads it.
            if (!BidWindow(auction->expire_time - now, VisitInterval()) ||
                (auction->bidder && !BotBidder(auction->bidder)))
                continue;
            price = MinimumBid(auction->startbid, auction->bid, AuctionEntry::CalculateAuctionOutBid(auction->bid));
            if (!Affordable(price, budget, bot->GetMoney()) || !WithinPriceCeiling(price, ceiling))
                continue;
        }
        ItemPosCountVec destination;
        if (bot->CanStoreItem(NULL_BAG, NULL_SLOT, destination, item, false) != EQUIP_ERR_OK)
            continue;
        OfferRank const rank = RankOffer(tier, price, ceiling);
        if (!bestId || BetterOffer(rank, best))
        {
            bestId = id;
            best = rank;
        }
    }
    if (!bestId)
        return false;
    // No intervening world operation can change the offer in this world-thread
    // invocation. The core checks current ownership/balance/bid and script hooks again.
    uint32 const before = bot->GetMoney();
    WorldPacket packet(CMSG_AUCTION_PLACE_BID);
    packet << auctioneer->GetGUID() << bestId << best.price;
    bot->GetSession()->HandleAuctionPlaceBid(packet);
    // A successful buyout deletes the auction; never dereference its old pointer.
    // A successful bid keeps it listed with the bot leading.
    AuctionEntry const* placed = house->GetAuction(bestId);
    return placed ? placed->bidder == bot->GetGUID() : bot->GetMoney() < before;
}

class EarnedAuctionVisit final : public PlayerbotOperation
{
public:
    EarnedAuctionVisit(ObjectGuid bot, ObjectGuid target, bool mailbox)
        : botGuid(bot), targetGuid(target), isMailbox(mailbox)
    {
    }
    ObjectGuid GetBotGuid() const override { return botGuid; }
    std::string GetName() const override { return "Earned auction visit"; }
    bool Execute() override
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid);
        PlayerbotAI* ai = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        if (!Ready(ai))
            return false;
        WorldObject* target = nullptr;
        Creature* auctioneer = nullptr;
        if (isMailbox)
        {
            if (targetGuid.IsGameObject())
                target = bot->GetGameObjectIfCanInteractWith(targetGuid, GAMEOBJECT_TYPE_MAILBOX);
            else if (targetGuid.IsAnyTypeCreature())
                target = bot->GetNPCIfCanInteractWith(targetGuid, UNIT_NPC_FLAG_MAILBOX);
        }
        else if (MeetsAuctionLevel(bot))
        {
            auctioneer = bot->GetNPCIfCanInteractWith(targetGuid, UNIT_NPC_FLAG_AUCTIONEER);
            target = auctioneer;
        }
        if (!target || !bot->IsWithinDistInMap(target, INTERACTION_DISTANCE))
            return false;
        // This map is touched exclusively by the world thread. Separate auction
        // and mailbox clocks allow collecting delivered goods after a purchase.
        static std::map<std::pair<ObjectGuid, bool>, time_t> lastVisits;
        time_t const now = GameTime::GetGameTime().count();
        auto const key = std::make_pair(botGuid, isMailbox);
        auto it = lastVisits.find(key);
        uint32 const interval = VisitInterval();
        if (it != lastVisits.end() && now - it->second < interval)
            return false;
        lastVisits[key] = now;
        if (isMailbox)
            return CollectReceipts(bot, targetGuid);
        AuctionHouseObject* house = sAuctionMgr->GetAuctionsMap(auctioneer->GetFaction());
        if (!house)
            return false;
        MarketState const state = OwnMarketState(bot);
        std::vector<uint32> const sample = SampleMarket(house, botGuid);
        bool const sold = SellSurplus(ai, auctioneer, house, state);
        bool const bought = !state.pendingPurchase && BuyUseful(ai, auctioneer, house, sample);
        return sold || bought;
    }

private:
    ObjectGuid botGuid;
    ObjectGuid targetGuid;
    bool isMailbox;
};

bool ShouldReserve(PlayerbotAI* ai, Item* item, ItemUsage usage, ProgressionSupplies& supplies)
{
    if (!ai || !ai->GetBot() || !sPlayerbotAIConfig.naturalProgression || !item)
        return false;
    // These are progression supplies rather than market surplus. Keep them
    // even when earned trading is disabled or emergency vendoring is needed.
    if (NeededForProgression(ai, item, supplies))
        return true;
    if (!IsEnabled(ai) || usage != ITEM_USAGE_AH || !MeetsAuctionLevel(ai->GetBot()) || !CanList(ai, item))
        return false;
    // Stock that keeps expiring unsold goes to the vendor instead of paying more deposits.
    if (!RelistAllowed(ai, item))
        return false;
    // Broke bots and full bags may vendor surplus rather than stalling ordinary
    // leveling. There is no gold grant to pay auction deposits.
    if (ai->GetAiObjectContext()->GetValue<uint8>("bag space")->Get() >= 80)
        return false;
    // Stock that could not repay its deposit goes to the vendor.
    if (!Listable(item))
        return false;
    return FreeBudget(ai, NeedMoneyFor::anything) >= NeutralDeposit(item);
}

}  // namespace

bool IsEnabled(PlayerbotAI* ai)
{
    return ai && sPlayerbotAIConfig.naturalProgression && sPlayerbotAIConfig.earnedAuctions && ai->GetBot() &&
           sRandomPlayerbotMgr.IsRandomBot(ai->GetBot()) && !ai->HasGameClientMaster();
}

bool HasSurplus(PlayerbotAI* ai)
{
    if (!IsEnabled(ai))
        return false;
    ProgressionSupplies supplies(ai);
    for (ObjectGuid guid : BagItems(ai->GetBot()))
        if (Item* item = ai->GetBot()->GetItemByGuid(guid))
            if (TradableSurplus(ai, item, supplies))
                return true;
    return false;
}

bool ShouldReserveForAuction(PlayerbotAI* ai, Item* item, ItemUsage usage)
{
    if (!ai || !ai->GetBot())
        return false;
    ProgressionSupplies supplies(ai);
    return ShouldReserve(ai, item, usage, supplies);
}

bool SellsToVendor(PlayerbotAI* ai, Item* item, ItemUsage usage)
{
    if (usage == ITEM_USAGE_VENDOR || usage == ITEM_USAGE_AH)
        return true;
    // Natural upkeep has no disenchanting, so a random bot sells outgrown bound gear
    // instead of keeping it in its bags. Vendors refuse gear without a price, gear the
    // bot cannot wear yet may still become an upgrade, and bots in a real guild keep
    // rare gear as they would from disenchanting.
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    return usage == ITEM_USAGE_DISENCHANT && proto && proto->SellPrice && item->IsSoulBound() &&
           sPlayerbotAIConfig.naturalProgression && ai && ai->GetBot() &&
           proto->RequiredLevel <= ai->GetBot()->GetLevel() && sRandomPlayerbotMgr.IsRandomBot(ai->GetBot()) &&
           !ai->HasGameClientMaster() && (proto->Quality <= ITEM_QUALITY_UNCOMMON || !ai->IsInRealGuild());
}

bool HasVendorSurplus(PlayerbotAI* ai)
{
    if (!ai || !ai->GetBot())
        return false;
    ProgressionSupplies supplies(ai);
    for (ObjectGuid guid : BagItems(ai->GetBot()))
        if (Item* item = ai->GetBot()->GetItemByGuid(guid))
        {
            ItemUsage const usage = Usage(ai, item);
            if (SellsToVendor(ai, item, usage) && !ShouldReserve(ai, item, usage, supplies))
                return true;
        }
    return false;
}

// The core checks the auction window's level requirement only when it greets a
// player at an auctioneer. The handlers a visit calls do not repeat it.
bool MeetsAuctionLevel(Player* bot) { return bot->GetLevel() >= sWorld->getIntConfig(CONFIG_AUCTION_LEVEL_REQ); }

bool QueueVisit(PlayerbotAI* ai, ObjectGuid target, bool mailbox)
{
    if (!Ready(ai) || !target || (!mailbox && !MeetsAuctionLevel(ai->GetBot())))
        return false;
    return PlayerbotWorldThreadProcessor::instance().QueueOperation(
        std::make_unique<EarnedAuctionVisit>(ai->GetBot()->GetGUID(), target, mailbox));
}
}  // namespace EarnedAuction
