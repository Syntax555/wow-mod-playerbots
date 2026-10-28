/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MaintenanceValues.h"

#include <unordered_map>

#include "BudgetValues.h"
#include "ItemUsageValue.h"
#include "PlayerbotFactory.h"
#include "Playerbots.h"
#include "Trainer.h"

bool IsProfessionSpell(uint32 spellId)
{
    SkillLineAbilityMapBounds const bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    for (auto it = bounds.first; it != bounds.second; ++it)
        if (IsProfessionSkill(it->second->SkillLine))
            return true;
    return false;
}

bool CanRunNaturalMaintenance(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return sPlayerbotAIConfig.naturalProgression && bot->IsAlive() && !bot->IsInCombat() && !bot->IsInFlight() &&
           !botAI->HasGameClientMaster() && botAI->GetAiObjectContext()->GetValue<bool>("can move around")->Get();
}

NaturalMaintenanceTask NaturalMaintenanceValue::Calculate()
{
    if (!CanRunNaturalMaintenance(botAI))
        return {};

    bool const canSell = AI_VALUE(bool, "can sell");
    bool const needsRepair = AI_VALUE(bool, "should repair") && AI_VALUE(bool, "can repair");
    uint32 const spellBudget = AI_VALUE2(uint32, "free money for", uint32(NeedMoneyFor::spells));
    uint32 const consumableBudget = AI_VALUE2(uint32, "free money for", uint32(NeedMoneyFor::consumables));
    uint32 const ammoBudget = AI_VALUE2(uint32, "free money for", uint32(NeedMoneyFor::ammo));
    bool const needsFood = consumableBudget && AI_VALUE2(uint32, "item count", "food") < 5;
    bool const needsDrink =
        consumableBudget && bot->GetMaxPower(POWER_MANA) > 0 && AI_VALUE2(uint32, "item count", "drink") < 5;
    uint32 ammoSubclass = 0;
    if (ammoBudget && bot->getClass() == CLASS_HUNTER && AI_VALUE2(uint32, "item count", "ammo") < 200)
    {
        if (Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
        {
            uint32 const subclass = ranged->GetTemplate()->SubClass;
            if (subclass == ITEM_SUBCLASS_WEAPON_GUN)
                ammoSubclass = ITEM_SUBCLASS_BULLET;
            else if (subclass == ITEM_SUBCLASS_WEAPON_BOW || subclass == ITEM_SUBCLASS_WEAPON_CROSSBOW)
                ammoSubclass = ITEM_SUBCLASS_ARROW;
        }
    }

    // Only reagents for spells already learned can become a paid supply goal; recipes are not.
    std::unordered_map<uint32, uint32> neededReagents;
    if (spellBudget)
    {
        for (auto const& [spellId, spell] : bot->GetSpellMap())
        {
            if (spell->State == PLAYERSPELL_REMOVED || !spell->Active)
                continue;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            if (!info || info->IsPassive() || IsProfessionSpell(spellId))
                continue;

            for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                if (info->Reagent[i] > 0 && info->ReagentCount[i] > 0 &&
                    bot->GetItemCount(info->Reagent[i], false) < uint32(info->ReagentCount[i]) * 2)
                    neededReagents[info->Reagent[i]] = info->ReagentCount[i];
        }
    }

    bool const needsSupplies = needsFood || needsDrink || ammoSubclass || !neededReagents.empty();
    if (!canSell && !needsRepair && !spellBudget && !needsSupplies)
        return {};

    // An unreachable or failed NPC must not block the others while it stays nearest.
    ObjectGuid const rejected = botAI->rpgInfo.naturalMaintenanceRejected;
    botAI->rpgInfo.naturalMaintenanceRejected.Clear();

    // The nearby list is sorted by distance. Expensive inventory/trainer scans run only every 30 seconds.
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "possible new rpg targets"))
    {
        Creature* npc = botAI->GetCreature(guid);
        if (guid == rejected || !npc || !npc->IsAlive() || !npc->IsFriendlyTo(bot))
            continue;

        if (canSell && npc->HasNpcFlag(UNIT_NPC_FLAG_VENDOR))
            return {guid, NaturalMaintenanceType::SELL};

        if (needsRepair && npc->HasNpcFlag(UNIT_NPC_FLAG_REPAIR))
            return {guid, NaturalMaintenanceType::REPAIR};

        if (sPlayerbotAIConfig.AllowLearnTrainerSpells && npc->HasNpcFlag(UNIT_NPC_FLAG_TRAINER))
        {
            Trainer::Trainer* trainer = sObjectMgr->GetTrainer(npc->GetEntry());
            if (trainer && trainer->IsTrainerValidForPlayer(bot))
            {
                for (Trainer::Spell const& spell : trainer->GetSpells())
                {
                    // Most listed spells are known or unavailable; skip them before quoting a price.
                    if (!PlayerbotFactory::IsTrainerSpellAllowedForBot(bot, trainer, &spell) ||
                        !trainer->CanTeachSpell(bot, &spell))
                        continue;

                    uint32 const price = trainer->GetSpellCost(bot, npc, &spell);
                    if (price <= spellBudget && bot->HasEnoughMoney(price))
                        return {guid, NaturalMaintenanceType::TRAIN};
                }
            }
        }

        if (!needsSupplies || !npc->HasNpcFlag(UNIT_NPC_FLAG_VENDOR))
            continue;

        VendorItemData const* stock = npc->GetVendorItems();
        if (!stock)
            continue;

        for (VendorItem const* item : stock->m_items)
        {
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item->item);
            if (!proto || item->ExtendedCost)
                continue;

            // Match the wanted supply from template fields before the costlier usability check.
            std::string const type = (needsFood || needsDrink) && proto->Class == ITEM_CLASS_CONSUMABLE
                                         ? ItemUsageValue::GetConsumableType(proto, bot->GetMaxPower(POWER_MANA) > 0)
                                         : "";
            uint32 budget = 0;
            NeedMoneyFor budgetType = NeedMoneyFor::none;
            if ((needsFood && type == "food") || (needsDrink && type == "drink"))
            {
                budget = consumableBudget;
                budgetType = NeedMoneyFor::consumables;
            }
            else if (ammoSubclass && proto->Class == ITEM_CLASS_PROJECTILE && proto->SubClass == ammoSubclass)
            {
                budget = ammoBudget;
                budgetType = NeedMoneyFor::ammo;
            }
            else if (neededReagents.count(proto->ItemId))
            {
                budget = spellBudget;
                budgetType = NeedMoneyFor::spells;
            }
            else
                continue;

            if (bot->CanUseItem(proto) != EQUIP_ERR_OK ||
                (item->maxcount && npc->GetVendorItemCurrentCount(item) < proto->BuyCount))
                continue;

            uint32 const price = uint32(floor(proto->BuyPrice * bot->GetReputationPriceDiscount(npc)));
            if (price <= budget && bot->HasEnoughMoney(price))
                return {guid, NaturalMaintenanceType::BUY, proto->ItemId, uint32(budgetType)};
        }
    }

    return {};
}

bool CanMoveAroundValue::Calculate()
{
    if (bot->IsInCombat())
        return false;

    if (bot->GetTradeData())
        return false;

    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT))
        return false;

    if (!AI_VALUE(bool, "group ready"))
        return false;

    return true;
}

bool ShouldHomeBindValue::Calculate() { return AI_VALUE2(float, "distance", "home bind") > 1000.0f; }

bool ShouldRepairValue::Calculate() { return AI_VALUE(uint8, "durability") < 80; }

bool CanRepairValue::Calculate()
{
    return AI_VALUE(uint8, "durability") < 100 &&
           AI_VALUE(uint32, "repair cost") < AI_VALUE2(uint32, "free money for", (uint32)NeedMoneyFor::repair);
}

bool ShouldSellValue::Calculate() { return AI_VALUE(uint8, "bag space") > 80; }

bool CanSellValue::Calculate()
{
    return (AI_VALUE2(uint32, "item count", "usage " + std::to_string(ITEM_USAGE_VENDOR)) +
            AI_VALUE2(uint32, "item count", "usage " + std::to_string(ITEM_USAGE_AH))) > 1;
}

bool CanTrainValue::Calculate()
{
    return AI_VALUE2(uint32, "free money for", (uint32)NeedMoneyFor::spells) > 0;
}

bool CanFightEqualValue::Calculate() { return AI_VALUE(uint8, "durability") > 20; }

bool CanFightEliteValue::Calculate()
{
    return bot->GetGroup() && AI_VALUE2(bool, "group and", "can fight equal") &&
           AI_VALUE2(bool, "group and", "following party") && !AI_VALUE2(bool, "group or", "should sell,can sell");
}

bool CanFightBossValue::Calculate()
{
    return bot->GetGroup() && bot->GetGroup()->GetMembersCount() > 3 &&
           AI_VALUE2(bool, "group and", "can fight equal") && AI_VALUE2(bool, "group and", "following party") &&
           !AI_VALUE2(bool, "group or", "should sell,can sell");
}
