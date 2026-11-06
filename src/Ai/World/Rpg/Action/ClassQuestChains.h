/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_CLASSQUESTCHAINS_H
#define PLAYERBOTS_CLASSQUESTCHAINS_H

#include <array>
#include <map>

#include "ClassQuestAction.h"
#include "Trigger.h"
#include "Value.h"

// The one native interaction a step needs beyond accepting, ordinary objectives and the turn-in.
enum class ClassQuestUse : uint8
{
    None,
    Creature,  // Own Item on creature Entry; a dormant quest taker is revived the same way.
    Object,    // Gameobject Entry, opened with the own Item when one is given.
    Stay,      // Beside creature Entry, defending it from its attackers.
    Focus,     // Own Item at its native spell focus; a ritual then fights the Entry it summons.
    Cast,      // Own Casts, in order, on creature Entry.
    Area,      // Native area trigger Entry.
};

struct ClassQuestStep
{
    uint8 Class;
    uint32 Quest;
    uint32 Spell;  // The lesson its chain teaches; 0 for class quests that teach none.
    ClassQuestUse Use = ClassQuestUse::None;
    uint32 Item = 0;
    uint32 Entry = 0;
    std::array<uint32, 2> Casts{};
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
    Creature* QuestTarget(Quest const* quest, ClassQuestStep const& step, bool revive);
    bool UseOnCreature(Quest const* quest, ClassQuestStep const& step);
    bool UseObject(Quest const* quest, ClassQuestStep const& step);
    bool StayWith(Quest const* quest, ClassQuestStep const& step);
    bool UseAtFocus(Quest const* quest, ClassQuestStep const& step);
    bool CastOn(Quest const* quest, ClassQuestStep const& step);
    bool ReachArea(Quest const* quest, ClassQuestStep const& step);
    bool Wait(Quest const* quest, WorldPosition const& pos);
    bool Restart(Quest const* quest, uint32 itemId);
    bool Abandon(uint32 questId);
    WorldPosition SpawnPosition(uint32 entry, bool object);
    WorldPosition FocusPosition(uint32 focusId);

    uint32 _waitQuest = 0;
    uint32 _waitSince = 0;
    uint32 _waitLast = 0;
    uint32 _usedQuest = 0;
    uint32 _usedAt = 0;
    WorldPosition _usedPosition;
    uint32 _spawnEntry = 0;
    bool _spawnObject = false;
    uint32 _spawnMap = 0;
    uint32 _spawnChecked = 0;
    WorldPosition _spawnPosition;
    uint32 _focusId = 0;
    uint32 _focusMap = 0;
    uint32 _focusChecked = 0;
    WorldPosition _focusPosition;
    ObjectGuid::LowType _castTarget = 0;
    uint8 _castIndex = 0;
    std::map<ObjectGuid::LowType, uint32> _usedTargets;
};

#endif
