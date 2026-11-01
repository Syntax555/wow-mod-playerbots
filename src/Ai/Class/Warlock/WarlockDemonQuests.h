/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_WARLOCKDEMONQUESTS_H
#define PLAYERBOTS_WARLOCKDEMONQUESTS_H

#include "NewRpgBaseAction.h"
#include "Trigger.h"
#include "Value.h"

struct WarlockDemonQuest
{
    uint32 Quest;
    uint32 Spell;
    uint32 Item;
    uint32 Creature;
};

bool CanRunWarlockDemonQuest(PlayerbotAI* botAI);
WarlockDemonQuest const* CurrentWarlockDemonQuest(PlayerbotAI* botAI);

class WarlockDemonQuestValue : public CalculatedValue<uint32>
{
public:
    WarlockDemonQuestValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "warlock demon quest", 5) {}
    uint32 Calculate() override;
};

class WarlockDemonQuestTrigger : public Trigger
{
public:
    WarlockDemonQuestTrigger(PlayerbotAI* botAI) : Trigger(botAI, "warlock demon quest") {}
    bool IsActive() override;
};

class WarlockDemonQuestAction : public NewRpgBaseAction
{
public:
    WarlockDemonQuestAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "warlock demon quest") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    bool MoveToQuestLocation(Quest const* quest, bool reward);
    bool MoveToQuestPosition(WorldPosition const& pos);
    bool InteractWithQuestGiver(Quest const* quest, bool reward);
    bool DoObjectives(Quest const* quest);
    bool UseRitual(WarlockDemonQuest const& data);

    uint32 _objectiveQuest = 0;
    uint32 _objectiveMap = 0;
    uint32 _objectiveChecked = 0;
    uint32 _objectiveIndex = 0;
    bool _objectiveIsGameObject = false;
    WorldPosition _objectivePos;
    uint32 _focusId = 0;
    uint32 _focusMap = 0;
    uint32 _focusChecked = 0;
    std::vector<WorldPosition> _focusPositions;
};

#endif
