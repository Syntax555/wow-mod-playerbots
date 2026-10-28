/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GreetAction.h"

#include <mutex>
#include <unordered_map>

#include "EmoteAction.h"
#include "Event.h"
#include "NewPlayerNearbyValue.h"
#include "Playerbots.h"

namespace
{
constexpr time_t GREETING_COOLDOWN = 60;

bool ReserveGreeting(ObjectGuid guid)
{
    static std::mutex greetingMutex;
    static std::unordered_map<ObjectGuid, time_t> greetingExpirations;
    static time_t nextCleanup = 0;
    time_t now = time(nullptr);

    std::lock_guard<std::mutex> lock(greetingMutex);
    if (now >= nextCleanup)
    {
        for (auto itr = greetingExpirations.begin(); itr != greetingExpirations.end();)
        {
            if (itr->second <= now)
                itr = greetingExpirations.erase(itr);
            else
                ++itr;
        }
        nextCleanup = now + GREETING_COOLDOWN;
    }

    auto const itr = greetingExpirations.find(guid);
    if (itr != greetingExpirations.end() && itr->second > now)
        return false;

    greetingExpirations[guid] = now + GREETING_COOLDOWN;
    return true;
}
}  // namespace

GreetAction::GreetAction(PlayerbotAI* botAI) : Action(botAI, "greet") {}

bool GreetAction::isUseful()
{
    return !sPlayerbotAIConfig.greetRealPlayersOnly || NewPlayerNearbyValue::CanGreet(botAI);
}

bool GreetAction::Execute(Event /*event*/)
{
    if (!isUseful())
        return false;

    ObjectGuid guid = AI_VALUE(ObjectGuid, "new player nearby");
    if (!guid || !guid.IsPlayer())
        return false;

    Player* player = dynamic_cast<Player*>(botAI->GetUnit(guid));
    if (!player)
        return false;

    if (sPlayerbotAIConfig.greetRealPlayersOnly && !NewPlayerNearbyValue::IsEligiblePlayer(botAI, player))
        return false;

    GuidSet& alreadySeenPlayers = botAI->GetAiObjectContext()->GetValue<GuidSet&>("already seen players")->Get();
    if (sPlayerbotAIConfig.greetRealPlayersOnly && !ReserveGreeting(guid))
    {
        alreadySeenPlayers.insert(guid);
        return false;
    }

    if (!bot->HasInArc(CAST_ANGLE_IN_FRONT, player, sPlayerbotAIConfig.SightDistance))
        bot->SetFacingToObject(player);

    if (sPlayerbotAIConfig.greetRealPlayersOnly)
    {
        WorldPacket data(SMSG_TEXT_EMOTE);
        data << uint32(TEXT_EMOTE_HELLO);
        data << EmoteAction::GetNumberOfEmoteVariants(TEXT_EMOTE_HELLO, bot->getRace(), bot->getGender());
        data << guid;
        bot->GetSession()->HandleTextEmoteOpcode(data);

        botAI->GetAiObjectContext()->GetValue<time_t>("last said", "greet")->Set(time(nullptr) + GREETING_COOLDOWN);
    }
    else
    {
        ObjectGuid oldSel = bot->GetTarget();
        bot->SetTarget(guid);
        botAI->PlayEmote(TEXT_EMOTE_HELLO);
        bot->SetTarget(oldSel);
    }

    alreadySeenPlayers.insert(guid);

    GuidVector nearestPlayers = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest friendly players")->Get();
    for (ObjectGuid const guid : nearestPlayers)
    {
        if (sPlayerbotAIConfig.greetRealPlayersOnly)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!NewPlayerNearbyValue::IsEligiblePlayer(botAI, unit ? unit->ToPlayer() : nullptr))
                continue;
        }

        alreadySeenPlayers.insert(guid);
    }

    return true;
}
