/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TravelAction.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"

bool TravelAction::Execute(Event /*event*/)
{
    TravelTarget* target = AI_VALUE(TravelTarget*, "travel target");

    if (bot->isMoving())
        return false;

    target->setStatus(TRAVEL_STATUS_WORK);

    Unit* newTarget = nullptr;
    std::list<Unit*> targets;
    Acore::AnyUnitInObjectRangeCheck u_check(bot, sPlayerbotAIConfig.SightDistance * 2);
    Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    Cell::VisitObjects(bot, searcher, sPlayerbotAIConfig.SightDistance);

    for (Unit* unit : targets)
    {
        newTarget = unit;
        if (!newTarget || !newTarget->IsInWorld() || newTarget->IsDuringRemoveFromWorld())
            continue;

        if (newTarget->GetMapId() != bot->GetMapId())
            continue;

        if (!newTarget->IsAlive())
            continue;

        if (newTarget->GetEntry() == uint32(target->getDestination()->getEntry()))
            continue;

        if (newTarget->IsInCombat())
            continue;

        if (bot->IsHostileTo(newTarget))
            SET_AI_VALUE(ObjectGuid, "pull target", newTarget->GetGUID());
        else
            SET_AI_VALUE(GuidPosition, "rpg target", GuidPosition(newTarget));

        break;
    }

    return true;
}

bool TravelAction::isUseful()
{
    return false && AI_VALUE(TravelTarget*, "travel target")->isActive() &&
           (!AI_VALUE(GuidPosition, "rpg target") || !AI_VALUE(ObjectGuid, "pull target"));
}
