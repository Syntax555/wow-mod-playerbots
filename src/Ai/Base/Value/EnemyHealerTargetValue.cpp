/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EnemyHealerTargetValue.h"
#include "Playerbots.h"
#include "ServerFacade.h"

namespace
{
// A real interrupt or silence. Stuns, charges and incapacitates stay on enemy healers only.
bool IsInterruptOrSilence(SpellInfo const* spellInfo)
{
    for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_INTERRUPT_CAST)
            return true;

        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA &&
            spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_MOD_SILENCE)
            return true;
    }

    return false;
}
}  // namespace

Unit* EnemyHealerTargetValue::Calculate()
{
    std::string const spell = qualifier;

    // Without a healer to stop, a real interrupt also goes to another attacker casting a harmful spell,
    // but only one already in the interrupt's range, so the bot never leaves its own target for it.
    // In BRD the Doomforge Arcanasmiths' Arcane Bolt (151 casts, 2 stopped) was the biggest damage
    // source because only enemy healers and the bot's current target were ever interrupted.
    SpellInfo const* interruptInfo = nullptr;
    if (uint32 interruptId = AI_VALUE2(uint32, "spell id", spell))
        interruptInfo = sSpellMgr->GetSpellInfo(interruptId);
    if (interruptInfo && !IsInterruptOrSilence(interruptInfo))
        interruptInfo = nullptr;

    Unit* harmfulCaster = nullptr;
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit == target)
            continue;

        if (ServerFacade::instance().GetDistance2d(bot, unit) > botAI->GetRange("spell"))
            continue;

        if (!botAI->IsInterruptableSpellCasting(unit, spell))
            continue;

        Spell* generic = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (generic && generic->m_spellInfo->IsPositive())
            return unit;

        Spell* channeled = unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (channeled && channeled->m_spellInfo->IsPositive())
            return unit;

        if (harmfulCaster || !interruptInfo)
            continue;

        float range = interruptInfo->GetMaxRange(false, bot);
        bool inRange = range <= NOMINAL_MELEE_RANGE ? bot->IsWithinMeleeRange(unit)
                                                    : bot->IsWithinCombatRange(unit, range);
        if (inRange)
            harmfulCaster = unit;
    }

    return harmfulCaster;
}
