/*
 * Raid clear: fire on the ground and the fronts of mobs that cleave or breathe. See Hazards.h.
 *
 * Released under the MIT License.
 */

#include "Hazards.h"

#include "Raids/BlackwingLair/BwlRaidClear.h"
#include "Raids/MoltenCore/McRaidClear.h"

#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <unordered_map>

using namespace RaidClear::Hazards;

namespace
{
    struct Fire
    {
        Position pos;
        float radius;
    };

    struct Front
    {
        Position pos;
        float facing;
        float reach;
        float halfAngle;
    };

    struct Found
    {
        uint32 ms = 0;
        std::vector<Fire> fires;
        std::vector<Front> fronts;
    };

    Table const* TableFor(uint32 mapId)
    {
        switch (mapId)
        {
            case RaidClear::MoltenCore::MAP_ID:    return &RaidClear::MoltenCore::HazardTable();
            case RaidClear::BlackwingLair::MAP_ID: return &RaidClear::BlackwingLair::HazardTable();
            default:                               return nullptr;
        }
    }

    // How far a spell's widest effect reaches round its caster (or where it lands).
    float RadiusOf(SpellInfo const* info, Unit* caster)
    {
        float radius = 0.0f;
        for (SpellEffectInfo const& effect : info->Effects)
            if (effect.HasRadius())
                radius = std::max(radius, effect.CalcRadius(caster));
        return radius;
    }

    // The reach and half the angle of a spell's cone, as the core works it out
    // (Spell::SelectImplicitConeTargets): the spell_cone table, else the cone the target type
    // names, else 60 degrees. False for a spell without a cone.
    bool ConeOf(SpellInfo const* info, Unit* caster, float& radius, float& halfAngle)
    {
        radius = 0.0f;
        float degrees = 0.0f;
        for (SpellEffectInfo const& effect : info->Effects)
            for (SpellImplicitTargetInfo const& target : { effect.TargetA, effect.TargetB })
            {
                if (target.GetSelectionCategory() != TARGET_SELECT_CATEGORY_CONE)
                    continue;
                radius = std::max(radius, effect.CalcRadius(caster));
                float cone = 60.0f;
                switch (target.GetTarget())
                {
                    case TARGET_UNIT_CONE_ENEMY_24:  cone = 24.0f; break;
                    case TARGET_UNIT_CONE_ENEMY_54:  cone = 54.0f; break;
                    case TARGET_UNIT_CONE_ENEMY_104: cone = 104.0f; break;
                    default: break;
                }
                degrees = std::max(degrees, cone);
            }
        if (radius <= 0.0f)
            return false;
        if (SpellCone const* cone = sSpellMgr->GetSpellCone(info->Id))
            degrees = float(cone->cone_degrees);
        halfAngle = degrees * float(M_PI) / 360.0f;
        return true;
    }

    // Where a mob that cleaves or breathes at whoever it is on is facing: at its victim (it turns
    // to it), or the way it stands when the two are on top of each other.
    float FacingOf(Unit const* unit, Unit const* victim)
    {
        return unit->GetExactDist2d(victim) > 1.0f ? unit->GetAngle(victim) : unit->GetOrientation();
    }

    // Everything this bot keeps out of, from the mobs fighting the raid, per bot for
    // HAZARD_FRESH_MS: the trigger asks every tick. One map thread updates a bot at a time.
    Found const& HazardsFor(PlayerbotAI* botAI, Player* bot)
    {
        thread_local std::unordered_map<ObjectGuid, Found> tFound;
        uint32 const now = getMSTime();
        Found& found = tFound[bot->GetGUID()];
        if (found.ms && getMSTimeDiff(found.ms, now) < HAZARD_FRESH_MS)
            return found;

        found.fires.clear();
        found.fronts.clear();
        found.ms = now ? now : 1;
        Table const* table = TableFor(bot->GetMapId());
        if (!table)
            return found;

        // Tanks stand in front of what they hold, and take its cleave.
        bool const keepsOutOfFronts = !PlayerbotAI::IsTank(bot);
        for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsCreature() || !unit->IsInMap(bot))
                continue;
            uint32 const entry = unit->GetEntry();
            // The caster's fire while it burns: a Rain of Fire for its channel, a Flamestrike for
            // eight seconds.
            for (Burner const& burner : table->burners)
                if (burner.entry == entry)
                    if (DynamicObject* fire = unit->GetDynObject(burner.spell))
                        found.fires.push_back({ fire->GetPosition(), fire->GetRadius() });

            if (!keepsOutOfFronts || !unit->IsAlive())
                continue;
            Unit* victim = unit->GetVictim();
            if (!victim || victim == bot)
                continue;
            for (Cleaver const& cleaver : table->cleavers)
                if (cleaver.entry == entry)
                    found.fronts.push_back({ unit->GetPosition(), FacingOf(unit, victim),
                                             unit->GetCombatReach() + cleaver.reach, cleaver.halfAngle });
            for (Breather const& breather : table->breathers)
            {
                if (breather.entry != entry)
                    continue;
                SpellInfo const* info = sSpellMgr->GetSpellInfo(breather.spell);
                float radius = 0.0f;
                float halfAngle = 0.0f;
                // The core counts a bot as in it as far out as the bot's own size.
                if (info && ConeOf(info, unit, radius, halfAngle))
                    found.fronts.push_back({ unit->GetPosition(), FacingOf(unit, victim),
                                             radius + bot->GetObjectSize() + CONE_MARGIN,
                                             halfAngle + CONE_ANGLE_MARGIN });
            }
        }

        // Fire traps: as far as the trap goes off or the fire it casts reaches.
        if (!table->traps.empty())
        {
            std::list<GameObject*> traps;
            bot->GetGameObjectListWithEntryInGrid(traps, table->traps, TRAP_SEARCH);
            for (GameObject* trap : traps)
            {
                if (!trap->isSpawned() || trap->GetGoType() != GAMEOBJECT_TYPE_TRAP)
                    continue;
                GameObjectTemplate const* goInfo = trap->GetGOInfo();
                float radius = float(goInfo->trap.diameter) / 2.0f;
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(goInfo->trap.spellId))
                    radius = std::max(radius, RadiusOf(info, nullptr));
                if (radius > 0.0f)
                    found.fires.push_back({ trap->GetPosition(), radius });
            }
        }
        return found;
    }

    // Yards from a spot to the nearest way out of everything above; negative inside.
    float HazardGap(Found const& found, float x, float y)
    {
        float gap = 1000.0f;
        for (Fire const& fire : found.fires)
            gap = std::min(gap, fire.pos.GetExactDist2d(x, y) - fire.radius - GROUND_FIRE_MARGIN);
        for (Front const& front : found.fronts)
        {
            float const r = front.pos.GetExactDist2d(x, y);
            if (r >= front.reach)
            {
                gap = std::min(gap, r - front.reach);
                continue;
            }
            float off = Position::NormalizeOrientation(
                std::atan2(y - front.pos.GetPositionY(), x - front.pos.GetPositionX()) - front.facing);
            if (off > float(M_PI))
                off = 2.0f * float(M_PI) - off;
            // Out over its side, or back past its reach.
            float const side = off >= front.halfAngle
                                   ? r * std::sin(std::min(off - front.halfAngle, float(M_PI) / 2.0f))
                                   : -std::min(r * std::sin(front.halfAngle - off), front.reach - r);
            gap = std::min(gap, side);
        }
        return gap;
    }

    bool Empty(Found const& found)
    {
        return found.fires.empty() && found.fronts.empty();
    }
}

bool RcHazardTrigger::IsActive()
{
    // Not only in combat: a Flamestrike lands on whoever is about, a camp included.
    if (!bot->IsAlive() || !botAI->CanMove())
        return false;
    Found const& found = HazardsFor(botAI, bot);
    return !Empty(found) && HazardGap(found, bot->GetPositionX(), bot->GetPositionY()) < 0.0f;
}

bool RcHazardAction::Execute(Event /*event*/)
{
    // A copy: the walk asks for paths, and the cache is the bot's own anyway.
    Found const found = HazardsFor(botAI, bot);
    if (Empty(found))
        return false;
    auto const gapAt = [&found](float x, float y, float /*z*/) { return HazardGap(found, x, y); };
    if (gapAt(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()) >= 0.0f)
        return false;
    return _walk.Go(gapAt, 0.0f, HAZARD_SPOT, 1.0f);
}
