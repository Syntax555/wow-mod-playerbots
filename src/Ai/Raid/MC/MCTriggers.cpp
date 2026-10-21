/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MCTriggers.h"

#include <algorithm>
#include <unordered_map>

#include "MCCoreHoundRecoveryValue.h"
#include "MCHelpers.h"
#include "Group.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidClearRegistry.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "Timer.h"

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

    // Raid clear's "rc mc shazzrah ranged" replaces this one while it runs: it finds him after his
    // Gate wipes his threat list, takes the healers out too, and looks all round for a way out.
    if (RaidClear::Runs(botAI, bot->GetMapId()))
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

bool MoltenCoreHelpers::GolemaggBacksOff(PlayerbotAI* botAI, Player* bot)
{
    Aura* splash = bot->GetAura(SPELL_MAGMA_SPLASH);
    uint32 const stacks = splash ? splash->GetStackAmount() : 0;
    if (stacks >= MAGMA_SPLASH_BACK_OFF_STACKS)
        return true;
    if (stacks < MAGMA_SPLASH_ROTATE_STACKS || !RaidClear::Runs(botAI, bot->GetMapId()))
        return false;

    // Twenty stacks are more than the healers keep up with on every melee at once (the logs showed
    // 20 to 26 on all of them). So the half of the group's melee bots with the most stacks stays
    // out while the rest hit him: one that comes back with its stacks still up is next in line
    // once the others have more. A group walk, so the answer is kept for a moment: the trigger and
    // the multiplier ask it every tick. One map thread updates a bot at a time.
    Group* group = bot->GetGroup();
    if (!group)
        return true;
    constexpr uint32 ROTATION_FRESH_MS = 500;
    struct Rotation
    {
        uint32 ms = 0;
        uint32 stacks = 0;
        bool out = false;
    };
    thread_local std::unordered_map<ObjectGuid, Rotation> tRotation;
    uint32 const now = getMSTime();
    Rotation& rotation = tRotation[bot->GetGUID()];
    if (rotation.ms && rotation.stacks == stacks && getMSTimeDiff(rotation.ms, now) < ROTATION_FRESH_MS)
        return rotation.out;
    uint32 melee = 0;
    uint32 ahead = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot) || !GET_PLAYERBOT_AI(member) ||
            PlayerbotAI::IsTank(member) || !PlayerbotAI::IsMelee(member) || member->GetDistance(bot) > 60.0f)
            continue;
        ++melee;
        if (member == bot)
            continue;
        Aura* theirs = member->GetAura(SPELL_MAGMA_SPLASH);
        uint32 const their = theirs ? theirs->GetStackAmount() : 0;
        if (their >= MAGMA_SPLASH_ROTATE_STACKS &&
            (their > stacks || (their == stacks && member->GetGUID() < bot->GetGUID())))
            ++ahead;
    }
    rotation.ms = now ? now : 1;
    rotation.stacks = stacks;
    rotation.out = ahead < std::max(1u, melee / 2);
    return rotation.out;
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

    if (!GolemaggBacksOff(botAI, bot))
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
    // Raid clear's hound skull ("rc mc mark hound") sees every hound in the fight, not only the ones
    // on the main tank's threat list, and replaces this one while it runs.
    if (RaidClear::Runs(botAI, bot->GetMapId()))
        return false;
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
