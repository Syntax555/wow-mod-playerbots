/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CLASSQUESTACTION_H
#define PLAYERBOTS_CLASSQUESTACTION_H

#include "NewRpgBaseAction.h"
#include "QuestValues.h"

bool CanRunClassQuest(PlayerbotAI* botAI, uint8 classId);
questRelationGuidps const* ClassQuestLocations(uint32 questId);

class ClassQuestRouteCosts
{
public:
    explicit ClassQuestRouteCosts(PlayerbotAI* botAI);
    float Cost(WorldPosition const& pos) const;

private:
    Player* _bot;
    bool _travel;
    std::vector<ContinentRoutes::Link> _links;
    ContinentRoutes::Search _search;
};

WorldPosition NearestClassQuestLocation(uint32 questId, uint32 relation, ClassQuestRouteCosts const& costs);
bool ClassQuestObjectiveNeeded(Player* bot, Quest const* quest, uint32 index);

class ClassQuestAction : public NewRpgBaseAction
{
public:
    ClassQuestAction(PlayerbotAI* botAI, std::string name) : NewRpgBaseAction(botAI, name) {}

protected:
    bool InteractWithQuestGiver(Quest const* quest, bool reward);
    bool MoveToQuestLocation(Quest const* quest, bool reward);
    bool MoveToQuestPosition(WorldPosition const& pos);
    bool DoObjectives(Quest const* quest);

private:
    uint32 _objectiveQuest = 0;
    uint32 _objectiveMap = 0;
    uint32 _objectiveChecked = 0;
    uint32 _objectiveIndex = 0;
    bool _objectiveIsGameObject = false;
    WorldPosition _objectivePos;
};

#endif
