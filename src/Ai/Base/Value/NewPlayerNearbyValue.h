/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NEWPLAYERNEARBYVALUE_H
#define PLAYERBOTS_NEWPLAYERNEARBYVALUE_H

#include "ObjectGuid.h"
#include "PlayerbotAIConfig.h"
#include "Value.h"

class Player;
class PlayerbotAI;

class NewPlayerNearbyValue : public CalculatedValue<ObjectGuid>
{
public:
    NewPlayerNearbyValue(PlayerbotAI* botAI)
        : CalculatedValue<ObjectGuid>(botAI, "new player nearby", sPlayerbotAIConfig.greetRealPlayersOnly ? 10 : 1)
    {
    }

    ObjectGuid Calculate() override;
    static bool CanGreet(PlayerbotAI* botAI);
    static bool IsEligiblePlayer(PlayerbotAI* botAI, Player* player);
};

class AlreadySeenPlayersValue : public ManualSetValue<GuidSet&>
{
public:
    AlreadySeenPlayersValue(PlayerbotAI* botAI) : ManualSetValue<GuidSet&>(botAI, data, "already seen players") {}

    GuidSet data;
};

#endif
