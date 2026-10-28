/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MaintenanceValues.h"
#include "NewRpgTriggers.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

bool NewRpgStatusTrigger::IsActive() { return status == botAI->rpgInfo.GetStatus(); }

bool NewRpgNaturalMaintenanceTrigger::IsActive()
{
    return sPlayerbotAIConfig.naturalProgression &&
           !AI_VALUE(NaturalMaintenanceTask, "natural maintenance").target.IsEmpty();
}
