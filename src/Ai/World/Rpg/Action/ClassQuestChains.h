/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CLASSQUESTCHAINS_H
#define PLAYERBOTS_CLASSQUESTCHAINS_H

#include <map>

#include "ClassQuestAction.h"
#include "Trigger.h"
#include "Value.h"

// Item and Target name a step's native interaction: Target > 0 is a creature, Target < 0 a gameobject.
// Without an item, the bot stays beside the creature or uses the gameobject.
struct ClassQuestStep
{
    uint8 Class;
    uint32 Quest;
    uint32 Spell;
    uint32 Item;
    int32 Target;
};

ClassQuestStep const* CurrentClassQuestStep(PlayerbotAI* botAI);

class ClassQuestChainValue : public CalculatedValue<uint32>
{
public:
    ClassQuestChainValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "class quest chain", 5) {}
    uint32 Calculate() override;
};

class ClassQuestChainTrigger : public Trigger
{
public:
    ClassQuestChainTrigger(PlayerbotAI* botAI) : Trigger(botAI, "class quest chain") {}
    bool IsActive() override;
};

class ClassQuestChainAction : public ClassQuestAction
{
public:
    ClassQuestChainAction(PlayerbotAI* botAI) : ClassQuestAction(botAI, "class quest chain") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    bool UseOnCreature(Quest const* quest, ClassQuestStep const& step);
    bool UseObject(Quest const* quest, ClassQuestStep const& step);
    bool StayWith(Quest const* quest, ClassQuestStep const& step);
    bool Wait(Quest const* quest, WorldPosition const& pos);
    bool Restart(Quest const* quest, uint32 itemId);
    bool Abandon(uint32 questId);
    WorldPosition SpawnPosition(int32 entry);

    uint32 _waitQuest = 0;
    uint32 _waitSince = 0;
    uint32 _waitLast = 0;
    uint32 _usedQuest = 0;
    uint32 _usedAt = 0;
    WorldPosition _usedPosition;
    int32 _spawnEntry = 0;
    uint32 _spawnMap = 0;
    uint32 _spawnChecked = 0;
    WorldPosition _spawnPosition;
    std::map<ObjectGuid::LowType, uint32> _usedTargets;
};

#endif
