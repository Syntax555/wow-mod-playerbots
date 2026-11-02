/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SHAMANTOTEMQUESTS_H
#define PLAYERBOTS_SHAMANTOTEMQUESTS_H

#include "ClassQuestAction.h"
#include "TaskScheduler.h"
#include "Trigger.h"
#include "Value.h"

struct ShamanTotemQuest
{
    uint32 Quest;
    uint32 Totem;
};

ShamanTotemQuest const* CurrentShamanTotemQuest(PlayerbotAI* botAI);

class ShamanTotemQuestValue : public CalculatedValue<uint32>
{
public:
    ShamanTotemQuestValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "shaman totem quest", 5) {}
    uint32 Calculate() override;
};

class ShamanTotemQuestTrigger : public Trigger
{
public:
    ShamanTotemQuestTrigger(PlayerbotAI* botAI) : Trigger(botAI, "shaman totem quest") {}
    bool IsActive() override;
};

class ShamanTotemQuestAction : public ClassQuestAction
{
public:
    ShamanTotemQuestAction(PlayerbotAI* botAI) : ClassQuestAction(botAI, "shaman totem quest") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    bool UseFocusedQuestItem(Quest const* quest, uint32 itemId, uint32 creatureEntry);
    bool UseDraeneiRitual(Quest const* quest, uint32 itemId, uint32 objectEntry, uint32 creatureEntry);
    bool WaitForWaterSpirit(Quest const* quest);
    bool RestartWaterQuest();
    void ResetWaterWait();

    uint32 _focusQuest = 0;
    uint32 _focusItem = 0;
    uint32 _focusSpell = 0;
    uint32 _focusMap = 0;
    uint32 _focusChecked = 0;
    uint32 _focusEntry = 0;
    WorldPosition _focusPosition;
    uint32 _ritualQuest = 0;
    uint32 _ritualMap = 0;
    uint32 _ritualChecked = 0;
    uint32 _ritualUsed = 0;
    uint32 _ritualWait = 0;
    WorldPosition _ritualPosition;
    WorldPosition _waterBrazier;
    TaskScheduler _waterScheduler;
    bool _waterWaiting = false;
    bool _waterRecoveryReady = false;
};

#endif
