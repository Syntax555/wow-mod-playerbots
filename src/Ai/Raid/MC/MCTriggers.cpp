/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MCTriggers.h"

#include "MCCoreHoundRecoveryValue.h"
#include "MCHelpers.h"
#include "PlayerbotAIConfig.h"
#include "SharedDefines.h"
#include "SpellAuras.h"

using namespace MoltenCoreHelpers;

bool McLivingBombDebuffTrigger::IsActive()
{
    // No check for Baron Geddon, because bots may have the bomb even after Geddon died.
    return bot->HasAura(SPELL_LIVING_BOMB);
}

bool McBaronGeddonInfernoTrigger::IsActive()
{
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "baron geddon"))
        return boss->HasAura(SPELL_INFERNO);
    return false;
}

bool McShazzrahRangedTrigger::IsActive()
{
    // Only fire inside Arcane Explosion range. The move-away action no-ops
    // beyond it, so an unconditional trigger makes every already-safe ranged
    // bot attempt a failing move each tick.
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "shazzrah");
    return boss && bot->GetDistance2d(boss) < ARCANE_EXPLOSION_DISTANCE;
}

bool McInLavaTrigger::IsActive()
{
    LiquidData const& liquid = bot->GetLiquidData();
    return (liquid.Flags & MAP_LIQUID_TYPE_MAGMA) &&
           (liquid.Status & (LIQUID_MAP_WATER_WALK | LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER));
}

bool McGolemaggMagmaSplashTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "golemagg the incinerator");
    if (!boss)
        return false;

    // Below 10% the fight is a burn race (Earthquake pulses, Core Ragers
    // converge): stack management stops mattering, melee return to dps.
    if (boss->GetHealthPct() <= 10.0f)
        return false;

    Aura* splash = bot->GetAura(SPELL_MAGMA_SPLASH);
    if (!splash || splash->GetStackAmount() < MAGMA_SPLASH_BACK_OFF_STACKS)
        return false;

    // Only fire while still inside swing range; once the bot has backed off,
    // the multiplier keeps it from re-engaging until the stack expires.
    return bot->GetDistance2d(boss) < MAGMA_SPLASH_BACK_OFF_DISTANCE;
}

bool McGolemaggMarkBossTrigger::IsActive()
{
    // any tank may mark the boss
    return PlayerbotAI::IsTank(bot) && AI_VALUE2(Unit*, "find target", "golemagg the incinerator");
}

bool McGolemaggIsMainTankTrigger::IsActive()
{
    return PlayerbotAI::IsMainTank(bot) && AI_VALUE2(Unit*, "find target", "golemagg the incinerator");
}

bool McGolemaggIsAssistTankTrigger::IsActive()
{
    return PlayerbotAI::IsAssistTank(bot) && AI_VALUE2(Unit*, "find target", "golemagg the incinerator");
}

bool McGolemaggIsHealerTrigger::IsActive()
{
    return PlayerbotAI::IsHeal(bot) && AI_VALUE2(Unit*, "find target", "golemagg the incinerator");
}

bool McCoreHoundMarkTrigger::IsActive()
{
    return PlayerbotAI::IsMainTank(bot) && AI_VALUE2(Unit*, "find target", "core hound");
}

bool McCoreHoundRecoveryValue::Calculate()
{
    if (!sPlayerbotAIConfig.McCoreHoundRecovery || bot->GetMapId() != 409 || !bot->IsInCombat())
        return _waiting = false;

    Unit* anchor = AI_VALUE2(Unit*, "find target", "core hound");
    if (!anchor)
        return _waiting = false;

    std::list<Creature*> hounds;
    anchor->GetCreatureListWithEntryInGrid(hounds, NPC_CORE_HOUND, CORE_HOUND_REKINDLE_RANGE);
    bool awake = false;
    bool smoldering = false;
    bool expiring = false;
    for (Creature* hound : hounds)
    {
        if (!hound->IsAlive() || !hound->IsInCombat() || !anchor->IsWithinLOSInMap(hound))
            continue;

        if (Aura* aura = hound->GetAura(SPELL_CORE_HOUND_PLAY_DEAD))
        {
            smoldering = true;
            // Keep the normal synchronized kill attempt until its window is almost lost.
            if (aura->GetDuration() <= CORE_HOUND_RECOVERY_WINDOW)
                expiring = true;
        }
        else
            awake = true;
    }

    if (!awake || !smoldering)
        return _waiting = false;

    return _waiting = _waiting || expiring;
}

bool McCoreHoundRecoveryTrigger::IsActive()
{
    return sPlayerbotAIConfig.McCoreHoundRecovery || (bot->GetPet() && bot->GetPet()->HasReactState(REACT_PASSIVE));
}
