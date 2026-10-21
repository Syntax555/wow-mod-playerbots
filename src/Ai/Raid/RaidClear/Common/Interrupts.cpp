/*
 * Raid clear: interrupting an enemy's cast, whoever's target it is. See Interrupts.h.
 *
 * Released under the MIT License.
 */

#include "Interrupts.h"

#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include "SpellAuraDefines.h"
#include "SpellDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <mutex>
#include <utility>

namespace RaidClear
{
    namespace
    {
        struct Interrupt
        {
            char const* spell;  // playerbots name
            float range;        // 0 = melee reach
        };
        // Counterspell, Wind Shear and Silence already go to an enemy healer that isn't the bot's
        // target (playerbots' "on enemy healer" triggers); the melee ones only ever hit the target.
        constexpr std::array<Interrupt, 8> INTERRUPTS = {{
            { "kick", 0.0f }, { "pummel", 0.0f }, { "shield bash", 0.0f }, { "mind freeze", 0.0f },
            { "wind shear", 25.0f }, { "counterspell", 30.0f }, { "silence", 30.0f }, { "silencing shot", 35.0f },
        }};

        // Stuns and incapacitates, which stop whatever the target is casting. Cooldowns from 10 s
        // (Gouge) to a minute, so the raid lists the casts worth one. Kidney Shot isn't here: it
        // needs combo points CanCastSpell doesn't see.
        constexpr std::array<Interrupt, 6> CAST_STOPPERS = {{
            { "gouge", 0.0f }, { "concussion blow", 0.0f }, { "bash", 0.0f },
            { "hammer of justice", 10.0f }, { "scatter shot", 15.0f }, { "repentance", 20.0f },
        }};

        // Whether this interrupt or silence gets through to the target: the spell as a whole (a
        // Counterspell carries its mechanic on the spell) and the effect that does the stopping
        // (Kick, Pummel and Wind Shear carry it on that effect, next to ones that still land).
        bool GetsThrough(Player* bot, Unit* target, SpellInfo const* info)
        {
            if (target->IsImmunedToSpell(info, bot))
                return false;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                SpellEffectInfo const& effect = info->Effects[i];
                bool const stops = effect.Effect == SPELL_EFFECT_INTERRUPT_CAST ||
                                   (effect.Effect == SPELL_EFFECT_APPLY_AURA && effect.ApplyAuraName == SPELL_AURA_MOD_SILENCE);
                if (stops && !target->IsImmunedToSpellEffect(info, i, bot))
                    return true;
            }
            return false;
        }

        // Whether this stun or incapacitate lands on the target: the effect that takes it out of
        // action, for the mechanic it carries (a Wyrmkin is immune to stuns, not to Gouge's
        // knockout or Scatter Shot's disorient).
        bool Stops(Player* bot, Unit* target, SpellInfo const* info)
        {
            if (target->IsImmunedToSpell(info, bot))
                return false;
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                SpellEffectInfo const& effect = info->Effects[i];
                bool const stops = effect.Effect == SPELL_EFFECT_APPLY_AURA &&
                                   (effect.ApplyAuraName == SPELL_AURA_MOD_STUN ||
                                    effect.ApplyAuraName == SPELL_AURA_MOD_CONFUSE);
                if (stops && !target->IsImmunedToSpellEffect(info, i, bot))
                    return true;
            }
            return false;
        }

        // When a bot last sent an interrupt at each caster (getMSTime()), by instance and caster.
        using InterruptKey = std::pair<uint32, ObjectGuid>;
        std::map<InterruptKey, uint32> sInterruptsSent;
        std::mutex sInterruptsLock;

        SpellInfo const* InfoOf(PlayerbotAI* botAI, char const* spell)
        {
            uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
            return spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        }
    }

    bool CanPayFor(PlayerbotAI* botAI, Player* bot, char const* spell)
    {
        SpellInfo const* info = InfoOf(botAI, spell);
        if (!info)
            return false;
        int32 const cost = info->CalcPowerCost(bot, info->GetSchoolMask());
        return cost <= 0 || bot->GetPower(Powers(info->PowerType)) >= uint32(cost);
    }

    char const* ReadyInterrupt(PlayerbotAI* botAI, Player* bot, Unit* target)
    {
        if (PlayerbotAI::IsTank(bot) && target->GetVictim() != bot)
            return nullptr;

        // CanCastSpell lets an out-of-range target through, so reach is checked here.
        char const* ready = nullptr;
        for (Interrupt const& interrupt : INTERRUPTS)
        {
            bool const inReach =
                interrupt.range > 0.0f ? bot->GetDistance(target) <= interrupt.range : bot->IsWithinMeleeRange(target);
            SpellInfo const* info = inReach ? InfoOf(botAI, interrupt.spell) : nullptr;
            if (info && botAI->CanCastSpell(interrupt.spell, target) && CanPayFor(botAI, bot, interrupt.spell) &&
                GetsThrough(bot, target, info))
            {
                ready = interrupt.spell;
                break;
            }
        }
        return ready && bot->IsWithinLOSInMap(target) ? ready : nullptr;
    }

    char const* ReadyCastStopper(PlayerbotAI* botAI, Player* bot, Unit* target)
    {
        if (PlayerbotAI::IsTank(bot) && target->GetVictim() != bot)
            return nullptr;

        char const* ready = nullptr;
        for (Interrupt const& stopper : CAST_STOPPERS)
        {
            bool const inReach =
                stopper.range > 0.0f ? bot->GetDistance(target) <= stopper.range : bot->IsWithinMeleeRange(target);
            SpellInfo const* info = inReach ? InfoOf(botAI, stopper.spell) : nullptr;
            if (info && botAI->CanCastSpell(stopper.spell, target) && CanPayFor(botAI, bot, stopper.spell) &&
                Stops(bot, target, info))
            {
                ready = stopper.spell;
                break;
            }
        }
        return ready && bot->IsWithinLOSInMap(target) ? ready : nullptr;
    }

    char const* ReadyStop(PlayerbotAI* botAI, Player* bot, Unit* caster, SpellInfo const* cast, bool worthACastStopper)
    {
        char const* with = Interruptible(cast) ? ReadyInterrupt(botAI, bot, caster) : nullptr;
        if (!with && worthACastStopper)
            with = ReadyCastStopper(botAI, bot, caster);
        return with;
    }

    bool Interruptible(SpellInfo const* cast)
    {
        // What the core's interrupt effect asks of the cast it stops (Spell::EffectInterruptCast).
        if (!cast || cast->PreventionType != SPELL_PREVENTION_TYPE_SILENCE)
            return false;
        return cast->IsChanneled() ? (cast->ChannelInterruptFlags & CHANNEL_INTERRUPT_FLAG_INTERRUPT) != 0
                                   : (cast->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) != 0;
    }

    bool IsInterrupt(std::string const& name)
    {
        return std::any_of(INTERRUPTS.begin(), INTERRUPTS.end(),
                           [&name](Interrupt const& interrupt) { return name == interrupt.spell; });
    }

    bool InterruptJustSent(Unit const* caster)
    {
        std::lock_guard<std::mutex> lock(sInterruptsLock);
        auto it = sInterruptsSent.find({ caster->GetInstanceId(), caster->GetGUID() });
        return it != sInterruptsSent.end() && getMSTimeDiff(it->second, getMSTime()) < INTERRUPT_SPREAD_MS;
    }

    void InterruptSent(Unit const* caster)
    {
        std::lock_guard<std::mutex> lock(sInterruptsLock);
        uint32 const now = getMSTime();
        // Stamps that have run out (dead casters, other raids) go.
        for (auto it = sInterruptsSent.begin(); it != sInterruptsSent.end();)
            it = getMSTimeDiff(it->second, now) >= INTERRUPT_SPREAD_MS ? sInterruptsSent.erase(it) : std::next(it);
        sInterruptsSent[{ caster->GetInstanceId(), caster->GetGUID() }] = now ? now : 1;
    }
}
