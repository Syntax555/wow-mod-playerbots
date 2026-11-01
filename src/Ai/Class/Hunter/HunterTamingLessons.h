/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_HUNTERTAMINGLESSONS_H
#define PLAYERBOTS_HUNTERTAMINGLESSONS_H

#include "NewRpgBaseAction.h"
#include "TravelMgr.h"
#include "Trigger.h"
#include "Value.h"

struct HunterTamingLesson
{
    uint32 Quest;
    uint32 Item;
    uint32 Spell;
    uint32 Creature;
};

class HunterTamingLessonValue : public CalculatedValue<HunterTamingLesson const*>
{
public:
    HunterTamingLessonValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "hunter taming lesson", 5) {}
    HunterTamingLesson const* Calculate() override;
};

class HunterTamingLessonTrigger : public Trigger
{
public:
    HunterTamingLessonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "hunter taming lesson", 5) {}
    bool IsActive() override;
};

class HunterTamingLessonAction : public NewRpgBaseAction
{
public:
    HunterTamingLessonAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "hunter taming lesson") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    bool MoveToLessonLocation(HunterTamingLesson const& lesson, bool questGiver, bool reward);
    bool InteractWithLessonGiver(HunterTamingLesson const& lesson);
    std::vector<WorldPosition> _spawns;
    uint32 _quest = 0;
    uint32 _phase = 0;
    uint32 _map = 0;
    uint32 _lastSpawnSearch = 0;
    size_t _spawnIndex = 0;
    bool _questGiver = false;
    bool _reward = false;
};

#endif
