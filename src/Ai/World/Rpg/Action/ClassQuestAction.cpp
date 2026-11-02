/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ClassQuestAction.h"

#include <cmath>
#include <limits>

#include "GameObject.h"
#include "Map.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SharedValueContext.h"

bool CanRunClassQuest(PlayerbotAI* botAI, uint8 classId)
{
    Player* bot = botAI->GetBot();
    return sPlayerbotAIConfig.naturalProgression && bot && bot->getClass() == classId && bot->GetSession() &&
           bot->GetSession()->IsHeadless() && sRandomPlayerbotMgr.IsRandomBot(bot) && !botAI->GetMaster() &&
           !bot->GetGroup() && bot->IsAlive() && bot->IsInWorld() && !bot->IsBeingTeleported() &&
           !bot->GetMap()->IsDungeon() && !bot->InBattleground() && !bot->InArena() && !bot->IsInCombat() &&
           !bot->IsMounted() && !bot->IsInFlight() && !bot->IsFlying() && !bot->GetTransport() && !bot->GetVehicle() &&
           !bot->HasUnitState(UNIT_STATE_LOST_CONTROL) && !bot->IsNonMeleeSpellCast(true);
}

questRelationGuidps const* ClassQuestLocations(uint32 questId)
{
    auto const& locations = sSharedValueContext.getGlobalValue<questGuidpMap>("quest guidp map")->RefGet();
    auto const it = locations.find(questId);
    return it == locations.end() ? nullptr : &it->second;
}

ClassQuestRouteCosts::ClassQuestRouteCosts(PlayerbotAI* botAI)
    : _bot(botAI->GetBot()), _travel(sPlayerbotAIConfig.continentTravel && sContinentRoutes.Built())
{
    if (_travel)
    {
        _links = sContinentRoutes.UsableLinks(_bot, botAI->continentRoute);
        _search = ContinentRoutes::Explore(_links, sContinentRoutes.Locate(WorldPosition(_bot)),
                                           [](ContinentRoutes::Link const&) { return true; });
    }
}

float ClassQuestRouteCosts::Cost(WorldPosition const& pos) const
{
    if (_travel)
        return ContinentRoutes::RouteTo(_links, _search, sContinentRoutes.Locate(pos)).seconds;
    return pos.GetMapId() == _bot->GetMapId() ? _bot->GetDistance(pos) / ContinentRoutes::RunSpeed
                                              : std::numeric_limits<float>::infinity();
}

WorldPosition NearestClassQuestLocation(uint32 questId, uint32 relation, ClassQuestRouteCosts const& costs)
{
    WorldPosition nearest;
    float distance = std::numeric_limits<float>::max();
    if (auto const* locations = ClassQuestLocations(questId))
        for (auto const& flags : *locations)
            if (flags.first & relation)
                for (auto const& entry : flags.second)
                    for (auto const& pos : entry.second)
                        if (costs.Cost(pos) < distance)
                        {
                            nearest = pos;
                            distance = costs.Cost(pos);
                        }
    return nearest;
}

bool ClassQuestObjectiveNeeded(Player* bot, Quest const* quest, uint32 index)
{
    auto const status = bot->getQuestStatusMap().find(quest->GetQuestId());
    if (status == bot->getQuestStatusMap().end())
        return false;
    if (index < QUEST_OBJECTIVES_COUNT)
        return quest->RequiredNpcOrGo[index] &&
               status->second.CreatureOrGOCount[index] < quest->RequiredNpcOrGoCount[index];
    index -= QUEST_OBJECTIVES_COUNT;
    return index < QUEST_ITEM_OBJECTIVES_COUNT && quest->RequiredItemId[index] &&
           bot->GetItemCount(quest->RequiredItemId[index]) < quest->RequiredItemCount[index];
}

bool ClassQuestAction::InteractWithQuestGiver(Quest const* quest, bool reward)
{
    auto const turnIn = [this, quest](ObjectGuid guid)
    {
        QuestStatus const before = bot->GetQuestStatus(quest->GetQuestId());
        if (!TurnInQuest(quest, guid))
            return false;
        return bot->GetQuestRewardStatus(quest->GetQuestId()) ||
               (quest->IsRepeatable() && before == QUEST_STATUS_COMPLETE &&
                bot->GetQuestStatus(quest->GetQuestId()) == QUEST_STATUS_NONE);
    };
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsInWorld() || creature->IsDuringRemoveFromWorld() || !bot->IsInMap(creature) ||
            !bot->InSamePhase(creature) || !bot->CanSeeOrDetect(creature) || !CanInteractWithQuestGiver(creature) ||
            !(reward ? creature->hasInvolvedQuest(quest->GetQuestId()) : creature->hasQuest(quest->GetQuestId())))
            continue;
        if (!IsWithinInteractionDist(creature) || !bot->IsWithinLOSInMap(creature))
        {
            if (sPlayerbotAIConfig.continentTravel && sContinentRoutes.Built())
                return MoveToQuestPosition(WorldPosition(creature));
            return MoveWorldObjectTo(guid);
        }
        if (reward)
            return turnIn(guid);
        return AcceptQuest(quest, guid) && bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_NONE;
    }
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go || !go->IsInWorld() || !go->isSpawned() || !bot->IsInMap(go) || !bot->InSamePhase(go) ||
            !bot->CanSeeOrDetect(go) || !CanInteractWithQuestGiver(go) ||
            !(reward ? go->hasInvolvedQuest(quest->GetQuestId()) : go->hasQuest(quest->GetQuestId())))
            continue;
        if (!IsWithinInteractionDist(go) || !bot->IsWithinLOSInMap(go))
        {
            if (sPlayerbotAIConfig.continentTravel && sContinentRoutes.Built())
                return MoveToQuestPosition(WorldPosition(go));
            return MoveWorldObjectTo(guid);
        }
        if (reward)
            return turnIn(guid);
        return AcceptQuest(quest, guid) && bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_NONE;
    }
    return false;
}

bool ClassQuestAction::MoveToQuestLocation(Quest const* quest, bool reward)
{
    ClassQuestRouteCosts const costs(botAI);
    WorldPosition const pos = NearestClassQuestLocation(
        quest->GetQuestId(), uint32(reward ? QuestRelationFlag::questTaker : QuestRelationFlag::questGiver), costs);
    return pos != WorldPosition() &&
           (pos.GetMapId() != bot->GetMapId() || bot->GetDistance(pos) > INTERACTION_DISTANCE) &&
           MoveToQuestPosition(pos);
}

bool ClassQuestAction::MoveToQuestPosition(WorldPosition const& pos)
{
    if (!sPlayerbotAIConfig.continentTravel || !sContinentRoutes.Built())
        return pos.GetMapId() == bot->GetMapId() && MoveFarTo(pos);
    if (!MoveToMap(pos))
        return false;
    botAI->rpgInfo.ChangeToTravelRoute();
    return true;
}

bool ClassQuestAction::DoObjectives(Quest const* quest)
{
    auto* state = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data);
    if (!state || state->questId != quest->GetQuestId())
        botAI->rpgInfo.ChangeToDoQuest(quest->GetQuestId(), quest);

    if (_objectiveQuest != quest->GetQuestId() || _objectiveMap != bot->GetMapId() || !_objectiveChecked ||
        GetMSTimeDiffToNow(_objectiveChecked) >= 60 * IN_MILLISECONDS ||
        (_objectivePos != WorldPosition() && !ClassQuestObjectiveNeeded(bot, quest, _objectiveIndex)))
    {
        _objectiveQuest = quest->GetQuestId();
        _objectiveMap = bot->GetMapId();
        _objectiveChecked = getMSTime();
        _objectivePos = WorldPosition();
        float distance = std::numeric_limits<float>::max();
        ClassQuestRouteCosts const costs(botAI);
        if (auto const* locations = ClassQuestLocations(quest->GetQuestId()))
            for (uint32 index = 0; index < QUEST_OBJECTIVES_COUNT + QUEST_ITEM_OBJECTIVES_COUNT; ++index)
            {
                if (!ClassQuestObjectiveNeeded(bot, quest, index))
                    continue;
                uint32 const objective = index < QUEST_OBJECTIVES_COUNT ? index : index - QUEST_OBJECTIVES_COUNT;
                for (auto const& flags : *locations)
                    if (flags.first & (1u << objective))
                        for (auto const& entry : flags.second)
                            for (auto const& pos : entry.second)
                                if (costs.Cost(pos) < distance)
                                {
                                    _objectiveIndex = index;
                                    _objectiveIsGameObject = entry.first < 0;
                                    _objectivePos = pos;
                                    distance = costs.Cost(pos);
                                }
            }
    }
    if (_objectivePos == WorldPosition())
        return false;
    if (_objectivePos.GetMapId() != bot->GetMapId())
        return MoveToQuestPosition(_objectivePos);
    // Native loot/grind reject elevated or obscured targets; walk into caves and up stairs before yielding.
    bool const approach =
        _objectiveIsGameObject || std::abs(bot->GetPositionZ() - _objectivePos.GetPositionZ()) > 3.0f ||
        !bot->IsWithinLOS(_objectivePos.GetPositionX(), _objectivePos.GetPositionY(), _objectivePos.GetPositionZ());
    float const distance = approach ? 2.0f : sPlayerbotAIConfig.SightDistance;
    return bot->GetExactDist(_objectivePos) > distance && MoveToQuestPosition(_objectivePos);
}
