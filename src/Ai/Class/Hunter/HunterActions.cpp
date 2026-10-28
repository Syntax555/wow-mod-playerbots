/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HunterActions.h"
#include "Event.h"
#include "GenericSpellActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool CastViperStingAction::isUseful()
{
    return CastAuraSpellAction::isUseful() && AI_VALUE2(uint8, "mana", "self target") < 50 &&
           AI_VALUE2(uint8, "mana", "current target") >= 30;
}

bool CastAspectOfTheHawkAction::isUseful()
{
    Unit* target = GetTarget();
    return target && !botAI->HasSpell("aspect of the dragonhawk");
}

bool CastArcaneShotAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !botAI->HasSpell("explosive shot"))
        return false;

    // Armor Penetration rating check - will not cast Arcane Shot above 435 ArP
    int32 armorPenRating =
        bot->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1) + bot->GetUInt32Value(CR_ARMOR_PENETRATION);
    if (armorPenRating > 435)
        return false;

    return true;
}

bool CastImmolationTrapAction::isUseful()
{
    Unit* target = GetTarget();
    return target && !botAI->HasSpell("explosive trap");
}

Value<Unit*>* CastFreezingTrap::GetTargetValue()
{
    return context->GetValue<Unit*>("cc target", "freezing trap");
}

bool FeedPetAction::Execute(Event /*event*/)
{
    if (sPlayerbotAIConfig.naturalProgression)
    {
        constexpr uint32 SPELL_FEED_PET = 6991;
        constexpr uint32 SPELL_PET_FEEDING = 1539;
        Pet* pet = bot->GetPet();
        if (!pet || pet->getPetType() != HUNTER_PET || !pet->IsAlive() || pet->IsInCombat() || bot->IsInCombat() ||
            pet->GetHappinessState() == HAPPY || pet->HasAura(SPELL_PET_FEEDING) || !bot->HasSpell(SPELL_FEED_PET))
            return false;

        // The learned Feed Pet spell consumes owned food and applies its normal happiness-over-time aura.
        for (Item* food : AI_VALUE2(std::vector<Item*>, "inventory items", "food"))
            if (food && pet->HaveInDiet(food->GetTemplate()) &&
                pet->GetCurrentFoodBenefitLevel(food->GetTemplate()->ItemLevel) > 0 &&
                botAI->CanCastSpell(SPELL_FEED_PET, bot, true, food))
                return botAI->CastSpell(SPELL_FEED_PET, bot, food);

        return false;
    }

    if (Pet* pet = bot->GetPet(); pet && pet->getPetType() == HUNTER_PET &&
        pet->GetHappinessState() != HAPPY)
    {
        pet->SetPower(POWER_HAPPINESS, pet->GetMaxPower(Powers(POWER_HAPPINESS)));
    }

    return true;
}

bool CastAutoShotAction::isUseful()
{
    if (botAI->IsInVehicle() && !botAI->IsInVehicle(false, false, true))
        return false;

    if (AI_VALUE(Unit*, "current target") && bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) &&
        bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL)->m_targets.GetUnitTargetGUID() ==
            AI_VALUE(Unit*, "current target")->GetGUID())
    {
        return false;
    }

    return AI_VALUE(uint32, "active spell") != AI_VALUE2(uint32, "spell id", getName());
}

bool CastDisengageAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    // can cast spell check passed in isUseful()
    bot->SetOrientation(bot->GetAngle(target));
    return CastSpellAction::Execute(event);
}

bool CastDisengageAction::isUseful()
{
    return !botAI->HasStrategy("trap weave", BOT_STATE_COMBAT);
}

Value<Unit*>* CastScareBeastCcAction::GetTargetValue()
{
    return context->GetValue<Unit*>("cc target", "scare beast");
}

bool CastScareBeastCcAction::Execute(Event /*event*/)
{
    return botAI->CastSpell("scare beast", GetTarget());
}

bool CastWingClipAction::isUseful()
{
    return CastSpellAction::isUseful() && !botAI->HasAura(spell, GetTarget());
}

std::vector<NextAction> CastWingClipAction::getPrerequisites()
{
    return {};
}
