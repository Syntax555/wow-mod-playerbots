/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DRUIDCLASSQUESTS_H
#define PLAYERBOTS_DRUIDCLASSQUESTS_H

#include <map>

#include "ClassQuestAction.h"
#include "Trigger.h"
#include "Value.h"

struct DruidClassQuest
{
    uint32 Quest;
    uint32 Spell;
};

DruidClassQuest const* CurrentDruidClassQuest(PlayerbotAI* botAI);

class DruidClassQuestValue : public CalculatedValue<uint32>
{
public:
    DruidClassQuestValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "druid class quest", 5) {}
    uint32 Calculate() override;
};

class DruidClassQuestTrigger : public Trigger
{
public:
    DruidClassQuestTrigger(PlayerbotAI* botAI) : Trigger(botAI, "druid class quest") {}
    bool IsActive() override;
};

class DruidClassQuestAction : public ClassQuestAction
{
public:
    DruidClassQuestAction(PlayerbotAI* botAI) : ClassQuestAction(botAI, "druid class quest") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    bool TalkToSpirit(Quest const* quest, uint32 entry);
    WorldPosition FocusPosition(uint32 itemId, uint32 summonEntry = 0);
    bool UseFocusedItem(Quest const* quest, uint32 itemId, uint32 summonEntry = 0);
    bool GatherQuestItem(Quest const* quest, uint32 itemId);
    bool CureSick(Quest const* quest);
    bool AcquireEarthroot(Quest const* quest);
    bool BodyAndHeart(Quest const* quest);
    bool RestartLunaclawQuest(Quest const* quest);

    uint32 _focusItem = 0;
    uint32 _focusId = 0;
    uint32 _focusSummon = 0;
    uint32 _focusMap = 0;
    uint32 _focusChecked = 0;
    uint32 _focusEntry = 0;
    WorldPosition _focusPosition;
    WorldPosition _summonPosition;
    uint32 _bearQuest = 0;
    uint32 _bearMissingSince = 0;
    uint32 _spiritEntry = 0;
    uint32 _spiritMap = 0;
    uint32 _spiritChecked = 0;
    WorldPosition _spiritPosition;
    uint32 _lootItem = 0;
    uint32 _lootMap = 0;
    uint32 _lootChecked = 0;
    uint32 _lootEntry = 0;
    bool _lootIsCreature = false;
    ObjectGuid::LowType _lootSpawn = 0;
    WorldPosition _lootPosition;
    std::map<ObjectGuid::LowType, uint32> _lootUnavailable;
    uint32 _sickEntry = 0;
    uint32 _sickMap = 0;
    uint32 _sickChecked = 0;
    ObjectGuid::LowType _sickSpawn = 0;
    WorldPosition _sickPosition;
    std::map<ObjectGuid::LowType, uint32> _sickUnavailable;
    uint32 _rootEntry = 0;
    uint32 _rootMap = 0;
    uint32 _rootChecked = 0;
    ObjectGuid::LowType _rootSpawn = 0;
    WorldPosition _rootPosition;
    std::map<ObjectGuid::LowType, uint32> _rootUnavailable;
};

#endif
