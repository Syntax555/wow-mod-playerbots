/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyActions.h"
#include "GenericSpellActions.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "OnyRaidClear.h"
#include "Playerbots.h"
#include "PositionAction.h"
#include "RaidClearRegistry.h"

bool RaidOnyxiaMoveToSideAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss)
        return false;

    float angleToBot = boss->GetAngle(bot);
    float bossFacing = boss->GetOrientation();
    float diff = fabs(angleToBot - bossFacing);
    if (diff > M_PI)
        diff = 2 * M_PI - diff;

    float distance = bot->GetDistance(boss);

    // Too close (30 yards) and either in front or behind
    if (distance <= 30.0f && (diff < M_PI / 4 || diff > 3 * M_PI / 4))
    {
        float offsetAngle = bossFacing + M_PI_2;  // 90° to the right
        float offsetDist = 15.0f;

        float sideX = boss->GetPositionX() + offsetDist * cos(offsetAngle);
        float sideY = boss->GetPositionY() + offsetDist * sin(offsetAngle);

        // bot->Yell("Too close to front or tail — moving to side of Onyxia!", LANG_UNIVERSAL);
        return MoveTo(boss->GetMapId(), sideX, sideY, boss->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);
    }

    return false;
}

bool RaidOnyxiaSpreadOutAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");

    if (!boss)
        return false;

    // Trigger may fire on one tick, but the action can execute on a later tick.
    // By that time the cast may have finished, so current spell can be null.
    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell || !currentSpell->m_spellInfo)
        return false;

    // Fireball
    if (currentSpell->m_spellInfo->Id != 18392)
        return false;

    Unit* unitTarget = currentSpell->m_targets.GetUnitTarget();
    Player* target = unitTarget ? unitTarget->ToPlayer() : nullptr;
    if (!target || target != bot)
        return false;

    // bot->Yell("Spreading out — I'm the Fireball target!", LANG_UNIVERSAL);
    return MoveFromGroup(9.0f);  // move 9 yards
}

bool RaidOnyxiaMoveToSafeZoneAction::isUseful()
{
    // Two fixed spots per breath spell, and since the strips moved (core 2a2211c) two of the eight
    // are in the fire. Where raid clear reads the strip from the server's spell data, its way out
    // ("rc ony leave deep breath") takes over.
    return !RaidClear::Runs(botAI, bot->GetMapId()) || !RaidClear::OnyxiasLair::BreathTrailKnown(bot);
}

bool RaidOnyxiaMoveToSafeZoneAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "onyxia");
    if (!boss)
        return false;

    Spell* currentSpell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!currentSpell || !currentSpell->m_spellInfo)
        return false;

    uint32 spellId = currentSpell->m_spellInfo->Id;

    std::vector<SafeZone> safeZones = GetSafeZonesForBreath(spellId);
    if (safeZones.empty())
        return false;

    // Find closest safe zone
    SafeZone* bestZone = nullptr;
    float bestDist = std::numeric_limits<float>::max();

    for (auto& zone : safeZones)
    {
        float dist = bot->GetExactDist2d(zone.pos.GetPositionX(), zone.pos.GetPositionY());
        if (dist < bestDist)
        {
            bestDist = dist;
            bestZone = &zone;
        }
    }

    if (!bestZone)
        return false;

    if (bot->IsWithinDist2d(bestZone->pos.GetPositionX(), bestZone->pos.GetPositionY(), bestZone->radius))
        return false;  // Already safe

    // Stop channeling spell first
    bot->AttackStop();
    bot->CastStop();

    // bot->Yell("Moving to Safe Zone!", LANG_UNIVERSAL);
    return MoveTo(bot->GetMapId(), bestZone->pos.GetPositionX(), bestZone->pos.GetPositionY(), bestZone->pos.GetPositionZ(),
                  false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool RaidOnyxiaKillWhelpsAction::Execute(Event /*event*/)
{
    using namespace RaidClear::OnyxiasLair;

    // A whelp that can't find a way to its victim evades every attack but goes on biting: leave it
    // for one that can be hurt.
    auto const hittable = [](Unit const* unit)
    {
        Creature const* whelp = unit ? unit->ToCreature() : nullptr;
        return whelp && whelp->IsAlive() && (whelp->GetEntry() == NPC_ONYXIAN_WHELP || whelp->GetEntry() == NPC_ONYXIAN_WHELP_40) &&
               !whelp->IsEvadingAttacks();
    };

    // If already attacking a whelp, don't swap targets
    if (hittable(AI_VALUE(Unit*, "current target")))
        return false;

    // The nearest one the bot can attack from here (Attack refuses a target out of sight).
    Unit* nearest = nullptr;
    float nearestDist = 0.0f;
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "possible targets"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsInWorld() || !hittable(unit) || !bot->IsValidAttackTarget(unit))
            continue;

        float const dist = bot->GetDistance(unit);
        if ((nearest && dist >= nearestDist) || !bot->IsWithinLOSInMap(unit))
            continue;
        nearest = unit;
        nearestDist = dist;
    }
    return nearest && Attack(nearest);
}

bool OnyxiaAvoidEggsAction::Execute(Event /*event*/)
{
    Position botPos = Position(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    float x, y;

    // get safe zone slightly away from eggs (Can this be dynamic?)
    if (botPos.GetExactDist2d(-36.0f, -164.0f) <= 5.0f)
    {
        x = -10.0f;
        y = -180.0f;
    }
    else if (botPos.GetExactDist2d(-34.0f, -262.0f) <= 5.0f)
    {
        x = -16.0f;
        y = -250.0f;
    }
    else
    {
        return false;  // Not in danger zone
    }

    // bot->Yell("Too close to eggs — backing off!", LANG_UNIVERSAL);

    return MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}
