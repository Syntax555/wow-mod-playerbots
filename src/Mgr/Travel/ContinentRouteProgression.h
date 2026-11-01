/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_CONTINENTROUTEPROGRESSION_H
#define PLAYERBOTS_CONTINENTROUTEPROGRESSION_H

#include "Define.h"

class Player;

namespace ContinentRouteProgression
{
// The account's expansion and Individual Progression's map gate for an arrival point, without
// the database reads map threads must not make. Kept apart because the module's header and
// PlayerbotAI.h declare conflicting names.
bool CanArrive(Player* bot, uint32 mapId, float x, float y, float z, uint32 zoneId);
}  // namespace ContinentRouteProgression

#endif
