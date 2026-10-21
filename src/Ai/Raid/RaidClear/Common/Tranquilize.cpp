/*
 * Raid clear: Tranquilizing Shot on the frenzies that matter. See Tranquilize.h.
 *
 * Released under the MIT License.
 */

#include "Tranquilize.h"

#include "Common/BossLookup.h"
#include "Raids/BlackwingLair/BwlRaidClear.h"
#include "Raids/MoltenCore/McRaidClear.h"

#include "Creature.h"
#include "Group.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"

#include <list>

using namespace RaidClear::Tranq;

namespace
{
    std::vector<Enrage> const& EnragesFor(uint32 mapId)
    {
        static std::vector<Enrage> const none;
        switch (mapId)
        {
            case RaidClear::MoltenCore::MAP_ID:    return RaidClear::MoltenCore::TranqEnrages();
            case RaidClear::BlackwingLair::MAP_ID: return RaidClear::BlackwingLair::TranqEnrages();
            default:                               return none;
        }
    }

    // An enraged mob in Tranquilizing Shot range that this hunter should take: the nearest of the
    // group's hunter bots that can shoot it now, so they don't all fire at the same one.
    Creature* EnragedToTranq(PlayerbotAI* botAI, Player* bot)
    {
        std::vector<Enrage> const& enrages = EnragesFor(bot->GetMapId());
        if (enrages.empty())
            return nullptr;

        std::list<Creature*> enraged;
        for (Enrage const& enrage : enrages)
        {
            if (enrage.boss)
            {
                if (Creature* boss = RaidClear::FightingCreature(bot, { enrage.entry }))
                    if (boss->HasAura(enrage.aura))
                        enraged.push_back(boss);
                continue;
            }
            std::list<Creature*> trash;
            bot->GetCreatureListWithEntryInGrid(trash, enrage.entry, TRANQUILIZING_SHOT_RANGE);
            for (Creature* mob : trash)
                if (mob->IsAlive() && mob->HasAura(enrage.aura))
                    enraged.push_back(mob);
        }

        for (Creature* mob : enraged)
        {
            // CanCastSpell lets an out-of-range target through.
            if (!bot->IsWithinCombatRange(mob, TRANQUILIZING_SHOT_RANGE) ||
                !botAI->CanCastSpell("tranquilizing shot", mob))
                continue;

            float const mine = bot->GetExactDist(mob);
            bool closer = false;
            if (Group* group = bot->GetGroup())
                for (GroupReference* ref = group->GetFirstMember(); ref && !closer; ref = ref->next())
                    if (Player* member = ref->GetSource())
                    {
                        if (member == bot || !member->IsAlive() || !member->IsClass(CLASS_HUNTER) ||
                            !member->IsInMap(bot) || member->GetExactDist(mob) >= mine ||
                            !member->IsWithinCombatRange(mob, TRANQUILIZING_SHOT_RANGE))
                            continue;
                        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
                        closer = memberAI && memberAI->CanCastSpell("tranquilizing shot", mob);
                    }
            if (!closer)
                return mob;
        }
        return nullptr;
    }
}

bool RcTranqTrigger::IsActive()
{
    return bot->IsClass(CLASS_HUNTER) && bot->IsInCombat() && EnragedToTranq(botAI, bot);
}

bool RcTranqAction::Execute(Event /*event*/)
{
    Creature* mob = EnragedToTranq(botAI, bot);
    return mob && botAI->CastSpell("tranquilizing shot", mob);
}
