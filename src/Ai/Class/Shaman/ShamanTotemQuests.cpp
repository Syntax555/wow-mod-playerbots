/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ShamanTotemQuests.h"

#include <limits>
#include <optional>

#include "GameObject.h"
#include "ObjectMgr.h"
#include "OpenItemAction.h"
#include "Playerbots.h"
#include "QuestPackets.h"
#include "SpellMgr.h"
#include "UseItemAction.h"

namespace
{
constexpr uint32 EarthTotem = 5175;
constexpr uint32 FireTotem = 5176;
constexpr uint32 WaterTotem = 5177;
constexpr uint32 AirTotem = 5178;
constexpr uint32 EarthSapta = 6635;
constexpr uint32 FireSapta = 6636;
constexpr uint32 WaterSapta = 6637;
constexpr uint32 WaterBracers = 7812;
constexpr uint32 GlowingEmber = 6655;
constexpr uint32 WaterSpiritQuest = 100;
constexpr uint32 ReturnWaterQuest = 96;
constexpr uint32 WaterSpiritRecovery = 1103;
constexpr uint32 FireproofSatchel = 24336;
constexpr uint32 WaterBreathingPotion = 23871;

ShamanTotemQuest const ShamanTotemQuests[] = {
    {1516, EarthTotem},  {1517, EarthTotem}, {1518, EarthTotem}, {1519, EarthTotem}, {1520, EarthTotem},
    {1521, EarthTotem},  {1522, FireTotem},  {1523, FireTotem},  {2983, FireTotem},  {2984, FireTotem},
    {1524, FireTotem},   {1525, FireTotem},  {1526, FireTotem},  {1527, FireTotem},  {1528, WaterTotem},
    {1529, WaterTotem},  {2985, WaterTotem}, {2986, WaterTotem}, {1530, WaterTotem}, {1535, WaterTotem},
    {1536, WaterTotem},  {1534, WaterTotem}, {220, WaterTotem},  {63, WaterTotem},   {100, WaterTotem},
    {96, WaterTotem},    {1531, AirTotem},   {1532, AirTotem},   {1462, EarthTotem}, {1463, EarthTotem},
    {1464, FireTotem},   {972, WaterTotem},  {1103, WaterTotem}, {9449, EarthTotem}, {9450, EarthTotem},
    {9451, EarthTotem},  {9462, FireTotem},  {9464, FireTotem},  {9465, FireTotem},  {9467, FireTotem},
    {9468, FireTotem},   {9461, FireTotem},  {9555, FireTotem},  {9500, WaterTotem}, {9502, WaterTotem},
    {10490, WaterTotem}, {9501, WaterTotem}, {9503, WaterTotem}, {9504, WaterTotem}, {9508, WaterTotem},
    {9509, WaterTotem},  {9547, AirTotem},   {9551, AirTotem},   {10491, AirTotem},  {9552, AirTotem},
    {9553, AirTotem},    {9554, AirTotem},
};

bool RecoveryQuest(uint32 questId)
{
    return questId == 1462 || questId == 1463 || questId == 1464 || questId == 972 || questId == WaterSpiritRecovery;
}

bool ActiveShamanQuest(Player* bot, uint32 questId)
{
    QuestStatus const status = bot->GetQuestStatus(questId);
    return status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE;
}

ShamanTotemQuest const* EligibleShamanQuest(Player* bot, uint32 questId)
{
    if (sPlayerbotAIConfig.IgnoredQuests.count(questId))
        return nullptr;
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest)
        return nullptr;
    if (!ActiveShamanQuest(bot, questId) && (bot->GetQuestStatus(questId) != QUEST_STATUS_NONE ||
                                             !bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false)))
        return nullptr;
    for (auto const& data : ShamanTotemQuests)
        if (data.Quest == questId)
            return &data;
    return nullptr;
}

bool TotemEarned(Player* bot, uint32 totem)
{
    if (bot->HasItemCount(totem, 1, true))
        return true;
    switch (totem)
    {
        case EarthTotem:
            return bot->GetQuestRewardStatus(1518) || bot->GetQuestRewardStatus(1521) ||
                   bot->GetQuestRewardStatus(9451);
        case FireTotem:
            return bot->GetQuestRewardStatus(1527) || bot->GetQuestRewardStatus(9555);
        case WaterTotem:
            return bot->GetQuestRewardStatus(ReturnWaterQuest) || bot->GetQuestRewardStatus(9509);
        case AirTotem:
            return bot->GetQuestRewardStatus(1531) || bot->GetQuestRewardStatus(1532) ||
                   bot->GetQuestRewardStatus(9554);
    }
    return false;
}

uint32 RequiredSapta(Player* bot, uint32 questId)
{
    QuestStatus const status = bot->GetQuestStatus(questId);
    if (((questId == 1517 || questId == 1520) && ActiveShamanQuest(bot, questId)) ||
        ((questId == 1518 || questId == 1521) && status == QUEST_STATUS_NONE))
        return EarthSapta;
    if (questId == 1526 && status == QUEST_STATUS_INCOMPLETE && !bot->HasItemCount(GlowingEmber))
        return FireSapta;
    if (questId == 63 && status == QUEST_STATUS_INCOMPLETE && !bot->HasItemCount(WaterBracers))
        return WaterSapta;
    return 0;
}

bool SaptaSight(Player* bot, uint32 itemId)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
    return item && item->Spells[0].SpellId > 0 && bot->HasAura(item->Spells[0].SpellId);
}

WorldPosition SpiritLocation(uint32 questId, uint32 creatureEntry, ClassQuestRouteCosts const& costs)
{
    WorldPosition nearest;
    float distance = std::numeric_limits<float>::max();
    if (auto const* locations = ClassQuestLocations(questId))
        for (auto const& flags : *locations)
            for (auto const& entry : flags.second)
                if (entry.first == int32(creatureEntry))
                    for (auto const& pos : entry.second)
                    {
                        float const cost = costs.Cost(pos);
                        if (cost < distance)
                        {
                            nearest = pos;
                            distance = cost;
                        }
                    }
    return nearest;
}
}  // namespace

ShamanTotemQuest const* CurrentShamanTotemQuest(PlayerbotAI* botAI)
{
    if (!CanRunClassQuest(botAI, CLASS_SHAMAN))
        return nullptr;
    Player* bot = botAI->GetBot();
    std::optional<ClassQuestRouteCosts> costs;
    for (uint32 totem : {EarthTotem, FireTotem, WaterTotem, AirTotem})
    {
        if (TotemEarned(bot, totem))
            continue;
        if (totem == WaterTotem && ActiveShamanQuest(bot, WaterSpiritRecovery))
        {
            if (!bot->HasItemCount(WaterSapta))
                if (auto const* recovery = EligibleShamanQuest(bot, 972))
                    return recovery;
            if (auto const* recovery = EligibleShamanQuest(bot, WaterSpiritRecovery))
                return recovery;
        }
        ShamanTotemQuest const* selected = nullptr;
        float nearest = std::numeric_limits<float>::max();
        for (auto const& data : ShamanTotemQuests)
        {
            if (data.Totem != totem || RecoveryQuest(data.Quest) || !EligibleShamanQuest(bot, data.Quest))
                continue;
            if (ActiveShamanQuest(bot, data.Quest))
            {
                selected = &data;
                break;
            }
            if (!costs)
                costs.emplace(botAI);
            // The purified spirit has no static spawn; its preceding brazier is the waiting place.
            WorldPosition const pos =
                data.Quest == ReturnWaterQuest
                    ? NearestClassQuestLocation(63, uint32(QuestRelationFlag::questTaker), *costs)
                    : NearestClassQuestLocation(data.Quest, uint32(QuestRelationFlag::questGiver), *costs);
            if (pos != WorldPosition() && costs->Cost(pos) < nearest)
            {
                selected = &data;
                nearest = costs->Cost(pos);
            }
        }
        if (!selected)
            continue;
        uint32 const sapta = RequiredSapta(bot, selected->Quest);
        if (sapta && !SaptaSight(bot, sapta) && !bot->HasItemCount(sapta))
        {
            uint32 const recoveryId = sapta == EarthSapta
                                          ? ((selected->Quest == 1517 || selected->Quest == 1518) ? 1463 : 1462)
                                          : (sapta == FireSapta ? 1464 : 972);
            if (auto const* recovery = EligibleShamanQuest(bot, recoveryId))
                return recovery;
        }
        return selected;
    }
    return nullptr;
}

uint32 ShamanTotemQuestValue::Calculate()
{
    auto const* quest = CurrentShamanTotemQuest(botAI);
    return quest ? quest->Quest : 0;
}

bool ShamanTotemQuestTrigger::IsActive()
{
    return CanRunClassQuest(botAI, CLASS_SHAMAN) && AI_VALUE(uint32, "shaman totem quest") != 0;
}

bool ShamanTotemQuestAction::isUseful()
{
    return CanRunClassQuest(botAI, CLASS_SHAMAN) && AI_VALUE(uint32, "shaman totem quest") != 0;
}

bool ShamanTotemQuestAction::Execute(Event /*event*/)
{
    _waterScheduler.Update();
    auto const* data = CurrentShamanTotemQuest(botAI);
    if (!data)
        return false;
    Quest const* quest = sObjectMgr->GetQuestTemplate(data->Quest);
    QuestStatus const status = bot->GetQuestStatus(data->Quest);
    if (data->Totem == WaterTotem && bot->IsUnderWater() && !bot->HasAuraType(SPELL_AURA_WATER_BREATHING))
        if (Item* potion = bot->GetItemByEntry(WaterBreathingPotion))
        {
            bot->StopMoving();
            return UseItemAction(botAI).UseItemOnUnit(potion, bot);
        }
    uint32 const sapta = RequiredSapta(bot, data->Quest);
    if (sapta && !SaptaSight(bot, sapta))
        return UseFocusedQuestItem(quest, sapta, sapta == EarthSapta ? 5891 : (sapta == FireSapta ? 5893 : 5894));
    if ((data->Quest == WaterSpiritQuest && status == QUEST_STATUS_COMPLETE) ||
        (data->Quest == ReturnWaterQuest && status == QUEST_STATUS_NONE))
        return WaitForWaterSpirit(quest);
    if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_COMPLETE)
    {
        bool const reward = status == QUEST_STATUS_COMPLETE;
        bool const result = InteractWithQuestGiver(quest, reward) || MoveToQuestLocation(quest, reward);
        if (data->Quest == WaterSpiritRecovery && reward && bot->GetQuestRewardStatus(WaterSpiritRecovery))
            ResetWaterWait();
        return result;
    }
    if (status != QUEST_STATUS_INCOMPLETE)
        return false;
    if (data->Quest == 1535 || data->Quest == 1536 || data->Quest == 1534 || data->Quest == 9504)
        return UseFocusedQuestItem(quest, quest->GetSrcItemId(), 0);
    if (data->Quest == 9467 || data->Quest == 9508)
    {
        auto* state = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data);
        if (!state || state->questId != data->Quest)
            botAI->rpgInfo.ChangeToDoQuest(data->Quest, quest);
        if (data->Quest == 9467 && bot->GetItemByEntry(FireproofSatchel))
            return OpenItemAction(botAI).Execute(Event());
        return data->Quest == 9467 ? UseDraeneiRitual(quest, 23682, 181672, 17206)
                                   : UseDraeneiRitual(quest, 23751, 181699, 17359);
    }
    return DoObjectives(quest);
}

bool ShamanTotemQuestAction::UseDraeneiRitual(Quest const* quest, uint32 itemId, uint32 objectEntry,
                                              uint32 creatureEntry)
{
    for (char const* nearby : {"nearest npcs", "nearest corpses"})
        for (ObjectGuid guid : AI_VALUE(GuidVector, nearby))
        {
            Unit* unit = botAI->GetUnit(guid);
            Creature* creature = unit ? unit->ToCreature() : nullptr;
            if (creature && creature->GetEntry() == creatureEntry && creature->IsInWorld() && bot->IsInMap(creature) &&
                bot->InSamePhase(creature) && bot->CanSeeOrDetect(creature) &&
                (creature->IsAlive() || bot->isAllowedToLoot(creature)))
                return false;
        }
    Item* item = bot->GetItemByEntry(itemId);
    if (!item)
        return false;
    if (_ritualQuest != quest->GetQuestId() || _ritualMap != bot->GetMapId() || !_ritualChecked ||
        GetMSTimeDiffToNow(_ritualChecked) >= MINUTE * IN_MILLISECONDS)
    {
        if (_ritualQuest != quest->GetQuestId() || _ritualMap != bot->GetMapId())
            _ritualUsed = 0;
        _ritualQuest = quest->GetQuestId();
        _ritualMap = bot->GetMapId();
        _ritualChecked = getMSTime();
        _ritualPosition = WorldPosition();
        ClassQuestRouteCosts const costs(botAI);
        WorldPosition origin;
        float nearest = std::numeric_limits<float>::max();
        for (auto const* spawn : origin.getGameObjectsNear(0.0f, objectEntry))
        {
            if (!(spawn->phaseMask & bot->GetPhaseMask()))
                continue;
            WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
            float const cost = costs.Cost(pos);
            if (cost < nearest)
            {
                _ritualPosition = pos;
                nearest = cost;
            }
        }
    }
    if (_ritualPosition == WorldPosition())
        return false;
    // Native goober events summon their opponents after the item cast and a two-second delay.
    if (_ritualUsed && GetMSTimeDiffToNow(_ritualUsed) < _ritualWait)
        return true;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* object = botAI->GetGameObject(guid);
        if (!object || object->GetEntry() != objectEntry || !object->IsInWorld() || !object->isSpawned() ||
            !bot->IsInMap(object) || !bot->InSamePhase(object) || !bot->CanSeeOrDetect(object) ||
            object->GetGoType() != GAMEOBJECT_TYPE_GOOBER ||
            object->GetExactDist(_ritualPosition) > INTERACTION_DISTANCE)
            continue;
        if (!object->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(object))
            return MoveToQuestPosition(_ritualPosition);
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnGameObject(item, guid))
            return false;
        _ritualUsed = getMSTime();
        Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!cast)
            cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        _ritualWait = 5 * IN_MILLISECONDS + (cast ? cast->GetCastTime() : 0);
        if (cast)
            botAI->WaitForSpellCast(cast);
        return true;
    }
    return (_ritualPosition.GetMapId() != bot->GetMapId() ||
            bot->GetExactDist(_ritualPosition) > INTERACTION_DISTANCE) &&
           MoveToQuestPosition(_ritualPosition);
}

bool ShamanTotemQuestAction::UseFocusedQuestItem(Quest const* quest, uint32 itemId, uint32 creatureEntry)
{
    Item* item = bot->GetItemByEntry(itemId);
    if (!item)
        return false;
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(item->GetTemplate()->Spells[0].SpellId);
    if (!spell || !spell->RequiresSpellFocus)
        return false;
    if (_focusQuest != quest->GetQuestId() || _focusItem != itemId || _focusSpell != spell->RequiresSpellFocus ||
        _focusMap != bot->GetMapId() || !_focusChecked || GetMSTimeDiffToNow(_focusChecked) >= MINUTE * IN_MILLISECONDS)
    {
        _focusQuest = quest->GetQuestId();
        _focusItem = itemId;
        _focusSpell = spell->RequiresSpellFocus;
        _focusMap = bot->GetMapId();
        _focusChecked = getMSTime();
        _focusPosition = WorldPosition();
        _focusEntry = 0;
        ClassQuestRouteCosts const costs(botAI);
        WorldPosition const spirit =
            creatureEntry ? SpiritLocation(quest->GetQuestId(), creatureEntry, costs) : WorldPosition();
        if (creatureEntry && spirit == WorldPosition())
            return MoveToQuestLocation(quest, false);
        float nearest = std::numeric_limits<float>::max();
        WorldPosition origin;
        for (auto const& entry : *sObjectMgr->GetGameObjectTemplates())
            if (entry.second.type == GAMEOBJECT_TYPE_SPELL_FOCUS &&
                entry.second.spellFocus.focusId == spell->RequiresSpellFocus)
                for (auto const* spawn : origin.getGameObjectsNear(0.0f, entry.first))
                {
                    if (!(spawn->phaseMask & bot->GetPhaseMask()))
                        continue;
                    WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
                    if (creatureEntry &&
                        (pos.GetMapId() != spirit.GetMapId() ||
                         pos.GetExactDist(spirit) > entry.second.spellFocus.dist + INTERACTION_DISTANCE))
                        continue;
                    float const cost = costs.Cost(pos);
                    if (cost < nearest)
                    {
                        _focusEntry = entry.first;
                        _focusPosition = pos;
                        nearest = cost;
                    }
                }
    }
    if (_focusPosition == WorldPosition())
        return false;
    // Water sampling uses server-only focuses; native casting checks their phase and range.
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* focus = botAI->GetGameObject(guid);
        if (!focus || focus->GetEntry() != _focusEntry || !focus->IsInWorld() || !focus->isSpawned() ||
            !bot->IsInMap(focus) || !bot->InSamePhase(focus) || focus->GetGoType() != GAMEOBJECT_TYPE_SPELL_FOCUS ||
            focus->GetGOInfo()->spellFocus.focusId != spell->RequiresSpellFocus ||
            focus->GetExactDist(_focusPosition) > INTERACTION_DISTANCE)
            continue;
        if (!focus->IsWithinDistInMap(bot, focus->GetGOInfo()->spellFocus.dist))
            return MoveToQuestPosition(_focusPosition);
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnUnit(item, bot))
            return false;
        Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!cast)
            cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (cast)
            botAI->WaitForSpellCast(cast);
        return true;
    }
    return (_focusPosition.GetMapId() != bot->GetMapId() || bot->GetExactDist(_focusPosition) > INTERACTION_DISTANCE) &&
           MoveToQuestPosition(_focusPosition);
}

void ShamanTotemQuestAction::ResetWaterWait()
{
    _waterScheduler.CancelAll();
    _waterWaiting = false;
    _waterRecoveryReady = false;
}

bool ShamanTotemQuestAction::WaitForWaterSpirit(Quest const* quest)
{
    if (InteractWithQuestGiver(quest, quest->GetQuestId() == WaterSpiritQuest))
    {
        ResetWaterWait();
        return true;
    }
    if (_waterBrazier == WorldPosition())
    {
        ClassQuestRouteCosts const costs(botAI);
        _waterBrazier = NearestClassQuestLocation(63, uint32(QuestRelationFlag::questTaker), costs);
    }
    if (_waterBrazier == WorldPosition())
        return false;
    if (_waterBrazier.GetMapId() != bot->GetMapId() || bot->GetExactDist(_waterBrazier) > INTERACTION_DISTANCE)
        return MoveToQuestPosition(_waterBrazier);
    if (!_waterWaiting)
    {
        _waterWaiting = true;
        _waterScheduler.Schedule(std::chrono::seconds(45), [this](TaskContext) { _waterRecoveryReady = true; });
    }
    if (!_waterRecoveryReady)
        return true;
    if (quest->GetQuestId() == WaterSpiritQuest)
        return RestartWaterQuest();
    if (EligibleShamanQuest(bot, WaterSpiritRecovery))
    {
        Quest const* recovery = sObjectMgr->GetQuestTemplate(WaterSpiritRecovery);
        return InteractWithQuestGiver(recovery, bot->GetQuestStatus(WaterSpiritRecovery) == QUEST_STATUS_COMPLETE) ||
               MoveToQuestLocation(recovery, false);
    }
    return false;
}

bool ShamanTotemQuestAction::RestartWaterQuest()
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(WaterSpiritQuest);
    if (!quest || !ActiveShamanQuest(bot, WaterSpiritQuest) || bot->GetQuestRewardStatus(WaterSpiritQuest) ||
        quest->GetSrcItemId())
        return false;
    for (uint32 item : quest->RequiredItemId)
        if (item)
            return false;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (bot->GetQuestSlotQuestId(slot) == WaterSpiritQuest)
        {
            WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST);
            packet << uint8(slot);
            WorldPackets::Quest::QuestLogRemoveQuest request(std::move(packet));
            request.Read();
            bot->GetSession()->HandleQuestLogRemoveQuest(request);
            if (bot->GetQuestStatus(WaterSpiritQuest) != QUEST_STATUS_NONE)
                return false;
            ResetWaterWait();
            return true;
        }
    return false;
}
