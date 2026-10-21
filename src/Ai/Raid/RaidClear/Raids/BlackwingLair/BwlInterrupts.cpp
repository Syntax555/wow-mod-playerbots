/*
 * Raid clear: Blackwing Lair, interrupting the trash casters. See BwlRaidClear.h.
 *
 * Released under the MIT License.
 */

#include "BwlRaidClear.h"

#include "Common/Interrupts.h"
#include "RaidClearConfig.h"

#include "AllSpellScript.h"
#include "Creature.h"
#include "Map.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"

#include <algorithm>
#include <array>
#include <iterator>

using namespace RaidClear::BlackwingLair;

namespace
{
    // The casts worth an interrupt, the worst first: Fireball Volley hits everyone within 45 yd,
    // Healing Circle puts the pack back up, a Shadow Bolt hits for 3,000, Flamestrike leaves
    // fire under the ranged, a Mage's Fireball is one player's problem. Whether one can be
    // stopped is the server's own spell data and the caster's immunities (Common/Interrupts.h):
    // on this core only the Shadow Bolt and the Fireball can, and the rest are passed over
    // without a kick being spent.
    constexpr std::array<uint32, 5> CASTS = {
        SPELL_FIREBALL_VOLLEY, SPELL_HEALING_CIRCLE, SPELL_BWL_SHADOW_BOLT, SPELL_BWL_FLAMESTRIKE, SPELL_BWL_FIREBALL,
    };

    // Its place in CASTS (0 first); CASTS.size() for anything else.
    size_t RankOf(uint32 spellId)
    {
        return size_t(std::distance(CASTS.begin(), std::find(CASTS.begin(), CASTS.end(), spellId)));
    }

    // The casts worth a stun or an incapacitate where no kick lands: the three that hit or heal a
    // whole pack. One evening's run let all 13 Wyrmkin Fireball Volleys go off, no kick being
    // able to touch them.
    bool WorthACastStopper(uint32 spellId)
    {
        return spellId == SPELL_FIREBALL_VOLLEY || spellId == SPELL_HEALING_CIRCLE || spellId == SPELL_BWL_FLAMESTRIKE;
    }

    bool IsStoppableCast(SpellInfo const* info)
    {
        return info && RankOf(info->Id) < CASTS.size() &&
               (RaidClear::Interruptible(info) || WorthACastStopper(info->Id));
    }

    // What this bot has ready that stops the cast: an interrupt where one gets through, else a
    // stun or an incapacitate on the casts worth one.
    char const* ReadyStop(PlayerbotAI* botAI, Player* bot, Unit* caster, SpellInfo const* cast)
    {
        char const* with = RaidClear::Interruptible(cast) ? RaidClear::ReadyInterrupt(botAI, bot, caster) : nullptr;
        if (!with && WorthACastStopper(cast->Id))
            with = RaidClear::ReadyCastStopper(botAI, bot, caster);
        return with;
    }

    // What this caster is casting that a bot should stop, or nullptr.
    SpellInfo const* CastToStop(Unit const* caster)
    {
        Spell const* spell = caster->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        SpellInfo const* info = spell ? spell->GetSpellInfo() : nullptr;
        return IsStoppableCast(info) ? info : nullptr;
    }

    // The caster fighting the raid that this bot should interrupt now, and with what: the worst
    // cast first, then the nearest. Its own target or not.
    Unit* CasterToInterrupt(PlayerbotAI* botAI, Player* bot, char const*& with)
    {
        Unit* best = nullptr;
        size_t bestRank = 0;
        float bestDist = 0.0f;
        for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsCreature() || !unit->IsAlive())
                continue;
            SpellInfo const* cast = CastToStop(unit);
            if (!cast || RaidClear::InterruptJustSent(unit))
                continue;
            char const* interrupt = ReadyStop(botAI, bot, unit, cast);
            if (!interrupt)
                continue;

            size_t const rank = RankOf(cast->Id);
            float const dist = bot->GetDistance(unit);
            if (!best || rank < bestRank || (rank == bestRank && dist < bestDist))
            {
                best = unit;
                bestRank = rank;
                bestDist = dist;
                with = interrupt;
            }
        }
        return best;
    }
}

bool RcBwlInterruptTrigger::IsActive()
{
    // Not only in combat: a bot held at the camp while the pack is dragged in can still
    // Counterspell a caster that is fighting the raid.
    if (bot->GetMapId() != MAP_ID || !bot->IsAlive())
        return false;
    char const* with = nullptr;
    return CasterToInterrupt(botAI, bot, with) != nullptr;
}

bool RcBwlInterruptAction::Execute(Event /*event*/)
{
    char const* with = nullptr;
    Unit* caster = CasterToInterrupt(botAI, bot, with);
    if (!caster)
        return false;
    // Claimed once it has gone out: a cast that fails leaves the next bot free to try.
    if (!botAI->CastSpell(with, caster))
        return false;
    RaidClear::InterruptSent(caster);
    return true;
}

// A bot doesn't run its engine while it casts or waits out its last action, and these casts
// take two or three seconds: the moment one starts, the bots near it that could stop it and
// aren't casting look at once.
class RaidClearBwlInterruptSpellScript : public AllSpellScript
{
public:
    RaidClearBwlInterruptSpellScript() : AllSpellScript("RaidClearBwlInterruptSpellScript", { ALLSPELLHOOK_ON_PREPARE }) {}

    void OnSpellPrepare(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo) override
    {
        if (!caster || !caster->IsCreature() || caster->GetMapId() != MAP_ID || !IsStoppableCast(spellInfo) ||
            !RaidClear::GetConfig().IsRaidEnabled(MAP_ID))
            return;

        Map::PlayerList const& players = caster->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator it = players.begin(); it != players.end(); ++it)
        {
            Player* player = it->GetSource();
            if (!player || !player->IsAlive() || player->GetExactDist(caster) > INTERRUPT_WAKE_RANGE ||
                player->GetCurrentSpell(CURRENT_GENERIC_SPELL) || player->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                continue;
            PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
            if (botAI && botAI->HasStrategy("rc bwl runners", botAI->GetState()) &&
                ReadyStop(botAI, player, caster, spellInfo))
                botAI->ResetActionDuration();
        }
    }
};

void RaidClear::BlackwingLair::AddInterruptScripts()
{
    new RaidClearBwlInterruptSpellScript();
}
