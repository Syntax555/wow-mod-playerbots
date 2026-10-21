/*
 * Raid clear: Blackwing Lair, crowd control on the trash. See BwlRaidClear.h.
 *
 * Released under the MIT License.
 */

#include "BwlRaidClear.h"

#include "Creature.h"
#include "Group.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

#include <algorithm>
#include <array>
#include <list>

using namespace RaidClear::BlackwingLair;

namespace
{
    // Banish is good for 30 s; a fresh one goes out before the old one ends, so the Felguard
    // never gets a swing in between.
    constexpr int32 BANISH_RENEW_MS = 3000;
    constexpr float BANISH_REACH = 30.0f;

    // The mobs worth a Polymorph here: the humanoids that hit or heal hard and live long. Not the
    // Blackwing Technicians, which die in seconds to the raid's area damage: one evening's run
    // put 85 of 187 Polymorphs on them.
    constexpr std::array<uint32, 3> POLYMORPH_WORTH = {
        NPC_BLACKWING_MAGE, NPC_BLACKWING_LEGIONNAIRE, NPC_BLACKWING_TASKMASTER,
    };

    bool IsBanish(SpellInfo const* info)
    {
        return info && (info->GetAllEffectsMechanicMask() & (1 << MECHANIC_BANISH));
    }

    Aura* BanishOn(Unit* unit)
    {
        for (auto const& [id, application] : unit->GetAppliedAuras())
            if (IsBanish(application->GetBase()->GetSpellInfo()))
                return application->GetBase();
        return nullptr;
    }

    bool IsSkull(Player* bot, Unit* unit)
    {
        Group* group = bot->GetGroup();
        return group && group->GetTargetIcon(RtiTargetValue::skullIndex) == unit->GetGUID();
    }

    // Another warlock is casting Banish at it right now.
    bool BanishOnTheWay(Player* bot, Unit* unit)
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsInMap(bot))
                continue;
            Spell const* spell = member->GetCurrentSpell(CURRENT_GENERIC_SPELL);
            if (spell && IsBanish(spell->GetSpellInfo()) &&
                spell->m_targets.GetUnitTargetGUID() == unit->GetGUID())
                return true;
        }
        return false;
    }

    // The Enraged Felguard this warlock should banish now: its own banish that is about to end,
    // else the nearest Felguard in the fight nobody holds, isn't skull and no warlock is already
    // banishing. A warlock holds one banish at a time, so one that is holding a Felguard well
    // enough leaves the rest to the others.
    Unit* FelguardToBanish(PlayerbotAI* botAI, Player* bot)
    {
        if (bot->getClass() != CLASS_WARLOCK || bot->GetMapId() != MAP_ID || !bot->IsInCombat())
            return nullptr;

        std::list<Creature*> felguards;
        bot->GetCreatureListWithEntryInGrid(felguards, NPC_ENRAGED_FELGUARD, BANISH_REACH);

        Unit* best = nullptr;
        float bestDist = 0.0f;
        for (Creature* felguard : felguards)
        {
            if (!felguard->IsAlive() || !felguard->IsInCombat() || IsSkull(bot, felguard))
                continue;
            if (Aura const* banish = BanishOn(felguard))
            {
                if (banish->GetCasterGUID() != bot->GetGUID())
                    continue;
                if (banish->GetDuration() > BANISH_RENEW_MS)
                    return nullptr;
                // Its own and about to end: nothing comes before that.
                return botAI->CanCastSpell("banish", felguard) && bot->IsWithinLOSInMap(felguard) ? felguard
                                                                                                    : nullptr;
            }
            if (BanishOnTheWay(bot, felguard) || !botAI->CanCastSpell("banish", felguard) ||
                !bot->IsWithinLOSInMap(felguard))
                continue;
            float const dist = bot->GetDistance(felguard);
            if (!best || dist < bestDist)
            {
                best = felguard;
                bestDist = dist;
            }
        }
        return best;
    }
}

bool RcBwlBanishFelguardTrigger::IsActive()
{
    return FelguardToBanish(botAI, bot) != nullptr;
}

bool RcBwlBanishFelguardAction::Execute(Event /*event*/)
{
    Unit* felguard = FelguardToBanish(botAI, bot);
    return felguard && botAI->CastSpell("banish", felguard);
}

float RcBwlPolymorphMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != MAP_ID || action->getName() != "polymorph")
        return 1.0f;
    Unit* target = action->GetTarget();
    if (!target || !target->IsCreature())
        return 1.0f;
    return std::find(POLYMORPH_WORTH.begin(), POLYMORPH_WORTH.end(), target->GetEntry()) != POLYMORPH_WORTH.end()
               ? 1.0f
               : 0.0f;
}
