/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NewPlayerNearbyValue.h"
#include "Playerbots.h"

bool NewPlayerNearbyValue::CanGreet(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot->IsInWorld() && bot->IsAlive() && !bot->IsInCombat() &&
           botAI->GetAiObjectContext()->GetValue<time_t>("last said", "greet")->Get() <= time(nullptr);
}

bool NewPlayerNearbyValue::IsEligiblePlayer(PlayerbotAI* botAI, Player* player)
{
    constexpr float GREETING_DISTANCE = 20.0f;
    Player* bot = botAI->GetBot();
    return IsRealPlayer(player) && player->IsInWorld() && player->IsAlive() && !player->IsInCombat() &&
           bot->IsFriendlyTo(player) && bot->IsWithinDistInMap(player, GREETING_DISTANCE) &&
           bot->CanSeeOrDetect(player) && bot->IsWithinLOSInMap(player);
}

ObjectGuid NewPlayerNearbyValue::Calculate()
{
    if (sPlayerbotAIConfig.greetRealPlayersOnly && !CanGreet(botAI))
        return ObjectGuid::Empty;

    GuidVector players = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get();
    GuidSet& alreadySeenPlayers = botAI->GetAiObjectContext()->GetValue<GuidSet&>("already seen players")->Get();
    for (ObjectGuid const guid : players)
    {
        if (alreadySeenPlayers.find(guid) != alreadySeenPlayers.end())
            continue;

        if (sPlayerbotAIConfig.greetRealPlayersOnly)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!IsEligiblePlayer(botAI, unit ? unit->ToPlayer() : nullptr))
                continue;
        }

        return guid;
    }

    return ObjectGuid::Empty;
}
