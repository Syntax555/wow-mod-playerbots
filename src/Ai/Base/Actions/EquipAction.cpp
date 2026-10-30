/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EquipAction.h"
#include "Event.h"
#include "ItemCountValue.h"
#include "ItemPackets.h"
#include "ItemUsageValue.h"
#include "ItemVisitors.h"
#include "Playerbots.h"
#include "StatsWeightCalculator.h"
#include <algorithm>
#include <utility>

bool EquipAction::Execute(Event event)
{
    std::string const text = event.getParam();
    ItemIds ids = chat->parseItems(text);
    EquipItems(ids);
    return true;
}

void EquipAction::EquipItems(ItemIds ids)
{
    for (ItemIds::iterator i = ids.begin(); i != ids.end(); i++)
    {
        FindItemByIdVisitor visitor(*i);
        EquipItem(&visitor);
    }
}

// General bag upgrades preserve quivers, ammo pouches and profession bags.
uint8 EquipAction::GetSmallestBagSlot(Player* bot)
{
    uint8 curBag = NULL_SLOT;
    uint32 curSlots = 0;
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        Bag const* const pBag = bot->GetBagByPos(bag);
        if (pBag)
        {
            ItemTemplate const* proto = pBag->GetTemplate();
            if (proto->Class != ITEM_CLASS_CONTAINER || proto->SubClass != ITEM_SUBCLASS_CONTAINER)
                continue;
            if (curBag != NULL_SLOT && curSlots < pBag->GetBagSize())
                continue;

            curBag = bag;
            curSlots = pBag->GetBagSize();
        }
        else
            return bag;
    }

    return curBag;
}

// A two-hander leaves the off hand free only under Titan's Grip, and never for staves or polearms.
bool EquipAction::FillsBothHands(Player* bot, ItemTemplate const* proto)
{
    return proto->InventoryType == INVTYPE_2HWEAPON &&
           (!bot->CanTitanGrip() || (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                                     proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                                     proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2));
}

// Best owned item the off hand could hold once a one-hander replaces a two-hander.
float EquipAction::BestOffhandScore(PlayerbotAI* botAI, StatsWeightCalculator& calculator, uint32 candidateId)
{
    Player* bot = botAI->GetBot();
    float best = 0.0f;
    for (Item* item : botAI->GetInventoryItems())
    {
        ItemTemplate const* proto = item->GetTemplate();
        uint32 type = proto->InventoryType;
        bool offhand = type == INVTYPE_SHIELD || type == INVTYPE_HOLDABLE ||
                       (bot->CanDualWield() && (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONOFFHAND));
        if (!offhand || item->GetEntry() == candidateId || item->IsBroken() || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;

        best = std::max(best, calculator.CalculateItem(item->GetEntry(), item->GetItemRandomPropertyId(),
                                                       EQUIPMENT_SLOT_OFFHAND));
    }

    return best;
}

void EquipAction::EquipItem(FindItemVisitor* visitor)
{
    IterateItems(visitor);
    std::vector<Item*> items = visitor->GetResult();
    if (!items.empty())
        EquipItem(*items.begin());
}

void EquipAction::EquipItem(Item* item, uint8 forcedSlot)
{
    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    ItemTemplate const* itemProto = item->GetTemplate();
    uint32 itemId = itemProto->ItemId;
    uint8 invType = itemProto->InventoryType;

    // Handle ammunition separately
    if (invType == INVTYPE_AMMO)
    {
        bot->SetAmmo(itemId);
        std::ostringstream out;
        out << "equipping " << chat->FormatItem(itemProto);
        botAI->TellMaster(out);
        return;
    }

    // Handle bags first
    bool equippedBag = false;
    if (itemProto->Class == ITEM_CLASS_CONTAINER)
    {
        // Attempt to equip as a bag
        uint8 newBagSlot = GetSmallestBagSlot(bot);

        if (newBagSlot == NULL_SLOT)
            return;

        uint16 src = ((bagIndex << 8) | slot);
        uint16 dst = ((INVENTORY_SLOT_BAG_0 << 8) | newBagSlot);
        bot->SwapItem(src, dst);
        equippedBag = true;
    }

    // If we didn't equip as a bag, try to equip as gear
    if (!equippedBag)
    {
        // Ranged weapons aren't handled by the rest of the weapon equip logic
        // Handle them early here to avoid issues.
        if (invType == INVTYPE_RANGED || invType == INVTYPE_THROWN || invType == INVTYPE_RANGEDRIGHT)
        {
            WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
            ObjectGuid itemguid = item->GetGUID();
            packet << itemguid << uint8(EQUIPMENT_SLOT_RANGED);

            WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
            nicePacket.Read();
            bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);

            std::ostringstream out;
            out << "Equipping " << chat->FormatItem(itemProto) << " in ranged slot";
            botAI->TellMaster(out);
            return;
        }

        uint8 dstSlot = forcedSlot == NULL_SLOT ? botAI->FindEquipSlot(itemProto, NULL_SLOT, true) : forcedSlot;

        // Check if the item is a weapon and whether the bot can dual wield or use Titan Grip
        bool isWeapon = (itemProto->Class == ITEM_CLASS_WEAPON);
        bool canTitanGrip = bot->CanTitanGrip();
        bool canDualWield = bot->CanDualWield();

        bool isTwoHander = (invType == INVTYPE_2HWEAPON);
        bool isValidTGWeapon = false;
        if (canTitanGrip && isTwoHander)
        {
            // Titan Grip-valid 2H weapon subclasses: Axe2, Mace2, Sword2
            isValidTGWeapon = (itemProto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
                               itemProto->SubClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
                               itemProto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD2);
        }

        // Check if the main hand currently has a 2H weapon equipped
        Item* currentMHItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        bool have2HWeaponEquipped = (currentMHItem && currentMHItem->GetTemplate()->InventoryType == INVTYPE_2HWEAPON);

        // bool canDualWieldOrTG = (canDualWield || (canTitanGrip && isTwoHander));
        bool canDualWieldOrTG = (canDualWield || isTwoHander);

        // If this is a weapon and we can dual wield or Titan Grip, check if we can improve main/off-hand setup
        if (isWeapon && canDualWieldOrTG && forcedSlot == NULL_SLOT)
        {
            // Fetch current main hand and offhand items
            Item* mainHandItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            Item* offHandItem  = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);

            // Set up the stats calculator once and reuse results for performance
            StatsWeightCalculator calculator(bot);
            calculator.SetWeaponPairs(true);

            // Calculate item scores once and store them
            float newItemScore = calculator.CalculateItem(itemId, item->GetItemRandomPropertyId(), EQUIPMENT_SLOT_MAINHAND);
            float mainHandScore = mainHandItem
                ? calculator.CalculateItem(mainHandItem->GetTemplate()->ItemId, mainHandItem->GetItemRandomPropertyId(),
                                           EQUIPMENT_SLOT_MAINHAND) : 0.0f;
            float offHandScore = offHandItem
                ? calculator.CalculateItem(offHandItem->GetTemplate()->ItemId, offHandItem->GetItemRandomPropertyId(),
                                           EQUIPMENT_SLOT_OFFHAND) : 0.0f;

            // Determine where this weapon can go
            bool canGoMain = (invType == INVTYPE_WEAPON ||
                              invType == INVTYPE_WEAPONMAINHAND ||
                              isTwoHander);

            bool canTGOff = false;
            if (canTitanGrip && isTwoHander && isValidTGWeapon)
                canTGOff = true;

            bool canGoOff = (invType == INVTYPE_WEAPON ||
                             invType == INVTYPE_WEAPONOFFHAND ||
                             canTGOff);

            // Check if the main hand item can go to offhand if needed
            bool mainHandCanGoOff = false;
            if (mainHandItem)
            {
                ItemTemplate const* mhProto = mainHandItem->GetTemplate();
                bool mhIsValidTG = false;
                if (canTitanGrip && mhProto->InventoryType == INVTYPE_2HWEAPON)
                {
                    mhIsValidTG = (mhProto->SubClass == ITEM_SUBCLASS_WEAPON_AXE2 ||
                                   mhProto->SubClass == ITEM_SUBCLASS_WEAPON_MACE2 ||
                                   mhProto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD2);
                }

                mainHandCanGoOff = (mhProto->InventoryType == INVTYPE_WEAPON ||
                                    mhProto->InventoryType == INVTYPE_WEAPONOFFHAND ||
                                    (mhProto->InventoryType == INVTYPE_2HWEAPON && mhIsValidTG));
            }

            // Priority 1: Replace main hand if the new weapon is strictly better
            // and if conditions allow (e.g. no conflicting 2H logic)
            bool betterThanMH = (newItemScore > mainHandScore);
            // Hands are compared as a pair, as in the upgrade evaluation.
            if (FillsBothHands(bot, itemProto))
                betterThanMH = newItemScore > mainHandScore + offHandScore;
            else if (mainHandItem && FillsBothHands(bot, mainHandItem->GetTemplate()))
                betterThanMH = newItemScore + BestOffhandScore(botAI, calculator, itemId) > mainHandScore;
            // If a one-handed weapon is better, we can still use it instead of a two-handed weapon
            bool mhConditionOK = (invType != INVTYPE_2HWEAPON ||
                      (isTwoHander && !canTitanGrip) ||
                      (canTitanGrip && isValidTGWeapon));

            if (canGoMain && betterThanMH && mhConditionOK)
            {
                // Equip new weapon in main hand
                {
                    WorldPacket eqPacket(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
                    ObjectGuid newItemGuid = item->GetGUID();
                    eqPacket << newItemGuid << uint8(EQUIPMENT_SLOT_MAINHAND);
                    WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(eqPacket));
                    nicePacket.Read();
                    bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
                }

                // Try moving old main hand weapon to offhand if beneficial
                if (mainHandItem && mainHandCanGoOff &&
                    (!offHandItem || calculator.CalculateItem(mainHandItem->GetEntry(), mainHandItem->GetItemRandomPropertyId(),
                                                              EQUIPMENT_SLOT_OFFHAND) > offHandScore))
                {
                    ItemTemplate const* oldMHProto = mainHandItem->GetTemplate();

                    WorldPacket offhandPacket(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
                    ObjectGuid oldMHGuid = mainHandItem->GetGUID();
                    offhandPacket << oldMHGuid << uint8(EQUIPMENT_SLOT_OFFHAND);
                    WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(offhandPacket));
                    nicePacket.Read();
                    bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);

                    std::ostringstream moveMsg;
                    moveMsg << "Main hand upgrade found. Moving " << chat->FormatItem(oldMHProto) << " to offhand";
                    botAI->TellMaster(moveMsg);
                }

                std::ostringstream out;
                out << "Equipping " << chat->FormatItem(itemProto) << " in main hand";
                botAI->TellMaster(out);
                return;
            }

            // Priority 2: If not better than main hand, check if better than offhand
            else if (canGoOff && calculator.CalculateItem(itemId, item->GetItemRandomPropertyId(),
                                                          EQUIPMENT_SLOT_OFFHAND) > offHandScore)
            {
                // Equip in offhand
                WorldPacket eqPacket(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
                ObjectGuid newItemGuid = item->GetGUID();
                eqPacket << newItemGuid << uint8(EQUIPMENT_SLOT_OFFHAND);
                WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(eqPacket));
                nicePacket.Read();
                bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);

                std::ostringstream out;
                out << "Equipping " << chat->FormatItem(itemProto) << " in offhand";
                botAI->TellMaster(out);
                return;
            }
            else
            {
                // No improvement, do nothing
                return;
            }
        }

        // If not a special dual-wield/TG scenario or no improvement found, fall back to original logic
        if (forcedSlot == NULL_SLOT && (dstSlot == EQUIPMENT_SLOT_FINGER1 ||
            dstSlot == EQUIPMENT_SLOT_TRINKET1 ||
            (dstSlot == EQUIPMENT_SLOT_MAINHAND && canDualWield &&
                ((invType != INVTYPE_2HWEAPON && !have2HWeaponEquipped) || (canTitanGrip && isValidTGWeapon)))))
        {
            // Handle ring/trinket dual-slot logic
            Item* const equippedItems[2] = {
                bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot),
                bot->GetItemByPos(INVENTORY_SLOT_BAG_0, dstSlot + 1)
            };

            if (equippedItems[0])
            {
                if (equippedItems[1])
                {
                    // Both slots are full - replace the item the new one improves on most, if any.
                    // Each slot is scored on its own set pieces and cap headroom.
                    StatsWeightCalculator calc(bot);
                    int32 newItemRandomProp = item->GetItemRandomPropertyId();
                    float gain[2];
                    for (uint8 i = 0; i < 2; ++i)
                        gain[i] = calc.CalculateItem(itemId, newItemRandomProp, dstSlot + i) -
                                  calc.CalculateItem(equippedItems[i]->GetEntry(),
                                                     equippedItems[i]->GetItemRandomPropertyId(), dstSlot + i);

                    // Early return if new item is not better than either equipped item
                    if (gain[0] <= 0 && gain[1] <= 0)
                        return;

                    if (gain[1] > gain[0])
                        dstSlot++;
                }
                else
                {
                    // Second slot empty, use it
                    dstSlot++;
                }
            }
        }

        // Equip the item in the chosen slot
        {
            WorldPacket packet(CMSG_AUTOEQUIP_ITEM_SLOT, 2);
            ObjectGuid itemguid = item->GetGUID();
            packet << itemguid << dstSlot;
            WorldPackets::Item::AutoEquipItemSlot nicePacket(std::move(packet));
            nicePacket.Read();
            bot->GetSession()->HandleAutoEquipItemSlotOpcode(nicePacket);
        }
    }

    std::ostringstream out;
    out << "Equipping " << chat->FormatItem(itemProto);
    botAI->TellMaster(out);
}

bool EquipAction::EquipUpgrades()
{
    if (bot->IsInCombat())
        return false;

    CollectItemsVisitor visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_BAGS);

    StatsWeightCalculator calculator(bot);
    calculator.SetWeaponPairs(true);
    calculator.SetPvpSpec(sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()));
    std::vector<std::pair<Item*, float>> items;
    bool const shieldTank = PlayerbotAI::IsTank(bot) && (bot->getClass() == CLASS_WARRIOR || bot->getClass() == CLASS_PALADIN);
    bool haveShield = false;
    bool haveOnehand = false;
    if (Item* offhand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
        haveShield = !offhand->IsBroken() && offhand->GetTemplate()->InventoryType == INVTYPE_SHIELD;
    if (Item* mainhand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
        haveOnehand = !mainhand->IsBroken() && (mainhand->GetTemplate()->InventoryType == INVTYPE_WEAPON ||
                                              mainhand->GetTemplate()->InventoryType == INVTYPE_WEAPONMAINHAND);
    for (Item* item : visitor.items)
    {
        if (!item || !item->GetTemplate() || item->IsBroken() || item->GetTemplate()->InventoryType == INVTYPE_NON_EQUIP)
            continue;
        float score = calculator.CalculateItem(item->GetEntry(), item->GetItemRandomPropertyId());
        items.emplace_back(item, score);
        if (score > 0 && bot->CanUseItem(item->GetTemplate()) == EQUIP_ERR_OK)
        {
            uint32 type = item->GetTemplate()->InventoryType;
            if (type == INVTYPE_SHIELD)
                haveShield = true;
            if (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONMAINHAND)
                haveOnehand = true;
        }
    }
    bool const preferShield = shieldTank && haveShield && haveOnehand;
    // Main-hand changes can make an owned shield equippable during the same pass.
    auto mainHand = [](Item* item)
    {
        uint32 type = item->GetTemplate()->InventoryType;
        return type == INVTYPE_WEAPON || type == INVTYPE_WEAPONMAINHAND || type == INVTYPE_2HWEAPON;
    };
    std::stable_sort(items.begin(), items.end(), [&](auto const& left, auto const& right)
    {
        if (mainHand(left.first) != mainHand(right.first))
            return mainHand(left.first);
        return left.second > right.second;
    });
    for (auto const& [item, score] : items)
    {
        if (item->IsEquipped())
            continue;
        uint32 type = item->GetTemplate()->InventoryType;
        if (preferShield && type == INVTYPE_2HWEAPON)
            continue;
        uint8 forcedSlot = NULL_SLOT;
        if (preferShield && (mainHand(item) || type == INVTYPE_SHIELD))
        {
            forcedSlot = type == INVTYPE_SHIELD ? EQUIPMENT_SLOT_OFFHAND : EQUIPMENT_SLOT_MAINHAND;
            Item* current = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, forcedSlot);
            if (score <= 0)
                continue;
            if (current && !current->IsBroken() && current->GetTemplate()->InventoryType != INVTYPE_2HWEAPON &&
                (type != INVTYPE_SHIELD || current->GetTemplate()->InventoryType == INVTYPE_SHIELD))
            {
                float currentScore = calculator.CalculateItem(current->GetEntry(), current->GetItemRandomPropertyId(), forcedSlot);
                float slotScore = calculator.CalculateItem(item->GetEntry(), item->GetItemRandomPropertyId(), forcedSlot);
                if (slotScore <= currentScore || slotScore <= currentScore * sPlayerbotAIConfig.EquipUpgradeThreshold)
                    continue;
            }
        }
        std::string qualifier = std::to_string(item->GetEntry()) + "," + std::to_string(item->GetItemRandomPropertyId());
        ItemUsage usage = AI_VALUE2(ItemUsage, "item upgrade", qualifier);
        uint16 dest = 0;
        InventoryResult canEquip = botAI->CanEquipItem(forcedSlot, dest, item, true, true);
        if (canEquip != EQUIP_ERR_OK && canEquip != EQUIP_ERR_CANT_CARRY_MORE_OF_THIS)
            continue;
        if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BAD_EQUIP)
            EquipItem(item, forcedSlot);
        else if (forcedSlot != NULL_SLOT)
            EquipItem(item, forcedSlot);
    }
    return true;
}

bool EquipUpgradesPacketAction::Execute(Event event)
{
    if (!sPlayerbotAIConfig.AutoEquipUpgradeLoot && !sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;
    std::string const source = event.GetSource();
    if (source == "trade status")
    {
        WorldPacket p(event.getPacket());
        p.rpos(0);
        uint32 status;
        p >> status;

        if (status != TRADE_STATUS_TRADE_ACCEPT)
            return false;
    }

    else if (source == "item push result")
    {
        WorldPacket p(event.getPacket());
        p.rpos(0);
        ObjectGuid playerGuid;
        uint32 received, created, sendChatMessage, itemSlot, itemId;
        uint8 bagSlot;

        p >> playerGuid;
        p >> received;
        p >> created;
        p >> sendChatMessage;
        p >> bagSlot;
        p >> itemSlot;
        p >> itemId;

        ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
        if (item->InventoryType == INVTYPE_NON_EQUIP)
            return false;
    }

    return EquipUpgrades();
}

bool EquipUpgradeAction::Execute(Event /*event*/)
{
    return EquipUpgrades();
}
