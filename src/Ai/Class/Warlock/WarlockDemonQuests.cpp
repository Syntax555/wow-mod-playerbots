/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "WarlockDemonQuests.h"

#include <limits>
#include <optional>

#include "GameObject.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "UseItemAction.h"

namespace
{
// The core owns race/level/prerequisite eligibility and all quest rewards. Items identify only native rituals.
WarlockDemonQuest const WarlockDemonQuests[] = {
    {1470, 688, 0, 0},       {1471, 697, 6284, 5676}, {1472, 712, 0, 0},       {1473, 697, 0, 0},
    {1474, 712, 6286, 5677}, {1476, 712, 0, 0},       {1478, 697, 0, 0},       {1485, 688, 0, 0},
    {1501, 697, 0, 0},       {1504, 697, 7464, 5676}, {1506, 697, 0, 0},       {1507, 712, 0, 0},
    {1508, 712, 0, 0},       {1509, 712, 0, 0},       {1510, 712, 0, 0},       {1511, 712, 0, 0},
    {1512, 712, 0, 0},       {1513, 712, 6626, 5677}, {1515, 712, 0, 0},       {1598, 688, 0, 0},
    {1599, 688, 0, 0},       {1685, 697, 0, 0},       {1688, 697, 0, 0},       {1689, 697, 6928, 5676},
    {1715, 697, 0, 0},       {1716, 712, 0, 0},       {1717, 712, 0, 0},       {1738, 712, 0, 0},
    {1739, 712, 6913, 5677}, {1758, 691, 0, 0},       {1795, 691, 6999, 6268}, {1798, 691, 0, 0},
    {1801, 691, 0, 0},       {1802, 691, 0, 0},       {1803, 691, 0, 0},       {1804, 691, 0, 0},
    {1805, 691, 0, 0},       {2996, 691, 0, 0},       {3001, 691, 0, 0},       {8344, 688, 0, 0},
    {10605, 712, 0, 0},      {10789, 697, 0, 0},      {10790, 697, 0, 0},      {9529, 697, 0, 0},
    {9619, 697, 23732, 5676}, {3631, 5784, 0, 0},     {4487, 5784, 0, 0},      {4488, 5784, 0, 0},
    {4489, 5784, 0, 0},      {4490, 5784, 0, 0},
};

}  // namespace

bool CanRunWarlockDemonQuest(PlayerbotAI* botAI) { return CanRunClassQuest(botAI, CLASS_WARLOCK); }

WarlockDemonQuest const* CurrentWarlockDemonQuest(PlayerbotAI* botAI)
{
    if (!CanRunWarlockDemonQuest(botAI))
        return nullptr;
    Player* bot = botAI->GetBot();
    std::optional<ClassQuestRouteCosts> costs;
    for (uint32 spell : {688u, 697u, 712u, 691u, 5784u})
    {
        if (bot->HasSpell(spell))
            continue;
        WarlockDemonQuest const* available = nullptr;
        float distance = std::numeric_limits<float>::max();
        for (auto const& data : WarlockDemonQuests)
        {
            if (data.Spell != spell || sPlayerbotAIConfig.IgnoredQuests.count(data.Quest) ||
                bot->GetQuestRewardStatus(data.Quest))
                continue;
            Quest const* quest = sObjectMgr->GetQuestTemplate(data.Quest);
            if (!quest)
                continue;
            QuestStatus const status = bot->GetQuestStatus(data.Quest);
            if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE)
                return &data;
            if (status != QUEST_STATUS_NONE || !bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false))
                continue;
            if (!costs)
                costs.emplace(botAI);
            WorldPosition const pos =
                NearestClassQuestLocation(data.Quest, uint32(QuestRelationFlag::questGiver), *costs);
            if (pos != WorldPosition() && costs->Cost(pos) < distance)
            {
                available = &data;
                distance = costs->Cost(pos);
            }
        }
        if (available)
            return available;
    }
    return nullptr;
}

uint32 WarlockDemonQuestValue::Calculate()
{
    auto const* data = CurrentWarlockDemonQuest(botAI);
    return data ? data->Quest : 0;
}

bool WarlockDemonQuestTrigger::IsActive()
{
    return CanRunWarlockDemonQuest(botAI) && AI_VALUE(uint32, "warlock demon quest") != 0;
}

bool WarlockDemonQuestAction::isUseful()
{
    return CanRunWarlockDemonQuest(botAI) && AI_VALUE(uint32, "warlock demon quest") != 0;
}

bool WarlockDemonQuestAction::Execute(Event /*event*/)
{
    auto const* data = CurrentWarlockDemonQuest(botAI);
    if (!data)
        return false;
    Quest const* quest = sObjectMgr->GetQuestTemplate(data->Quest);
    QuestStatus const status = bot->GetQuestStatus(data->Quest);
    if (status == QUEST_STATUS_NONE || status == QUEST_STATUS_COMPLETE)
    {
        bool const reward = status == QUEST_STATUS_COMPLETE;
        return InteractWithQuestGiver(quest, reward) || MoveToQuestLocation(quest, reward);
    }
    if (status != QUEST_STATUS_INCOMPLETE)
        return false;
    if (data->Item)
    {
        auto* state = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data);
        if (!state || state->questId != data->Quest)
            botAI->rpgInfo.ChangeToDoQuest(data->Quest, quest);
        return UseRitual(*data);
    }
    return DoObjectives(quest);
}

bool WarlockDemonQuestAction::UseRitual(WarlockDemonQuest const& data)
{
    if (bot->GetReqKillOrCastCurrentCount(data.Quest, data.Creature))
        return false;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (creature && creature->IsAlive() && creature->IsInWorld() && bot->IsInMap(creature) &&
            bot->InSamePhase(creature) && creature->GetEntry() == data.Creature && creature->ToTempSummon() &&
            creature->ToTempSummon()->GetSummonerGUID() == bot->GetGUID())
            return true;  // Wait here until native SmartAI starts ordinary combat after three seconds.
    }
    Item* item = bot->GetItemByEntry(data.Item);
    if (!item)
        return false;
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(item->GetTemplate()->Spells[0].SpellId);
    if (!spell || !spell->RequiresSpellFocus)
        return false;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* focus = botAI->GetGameObject(guid);
        if (!focus || !focus->IsInWorld() || !focus->isSpawned() || !bot->IsInMap(focus) || !bot->InSamePhase(focus) ||
            !bot->CanSeeOrDetect(focus) || focus->GetGoType() != GAMEOBJECT_TYPE_SPELL_FOCUS ||
            focus->GetGOInfo()->spellFocus.focusId != spell->RequiresSpellFocus)
            continue;
        float const range = focus->GetGOInfo()->spellFocus.dist;
        if (!focus->IsWithinDistInMap(bot, range))
        {
            if (sPlayerbotAIConfig.continentTravel && sContinentRoutes.Built())
                return MoveToQuestPosition(WorldPosition(focus));
            return MoveTo(focus, range * 0.8f);
        }
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnUnit(item, bot))
            return false;
        Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        botAI->WaitForSpellCast(cast ? cast : bot->GetCurrentSpell(CURRENT_GENERIC_SPELL));
        return true;
    }
    if (_focusId != spell->RequiresSpellFocus || _focusMap != bot->GetMapId() || !_focusChecked ||
        GetMSTimeDiffToNow(_focusChecked) >= 60 * IN_MILLISECONDS)
    {
        _focusId = spell->RequiresSpellFocus;
        _focusMap = bot->GetMapId();
        _focusChecked = getMSTime();
        _focusPositions.clear();
        WorldPosition origin(bot);
        for (auto const& entry : *sObjectMgr->GetGameObjectTemplates())
            if (entry.second.type == GAMEOBJECT_TYPE_SPELL_FOCUS && entry.second.spellFocus.focusId == _focusId)
                for (auto const* spawn : origin.getGameObjectsNear(0.0f, entry.first))
                    if (spawn->phaseMask & bot->GetPhaseMask())
                        _focusPositions.emplace_back(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
    }
    WorldPosition nearest;
    float distance = std::numeric_limits<float>::max();
    for (auto const& pos : _focusPositions)
        if (pos.GetMapId() == bot->GetMapId() && bot->GetDistance(pos) < distance)
        {
            nearest = pos;
            distance = bot->GetDistance(pos);
        }
    if (nearest != WorldPosition() && distance > INTERACTION_DISTANCE)
        return MoveToQuestPosition(nearest);
    // Strahad's native quest-accept escort creates the Felhunter focus; follow the living NPC while it runs.
    if (data.Quest == 1795)
        for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
        {
            Unit* unit = botAI->GetUnit(guid);
            Creature* strahad = unit ? unit->ToCreature() : nullptr;
            if (strahad && strahad->GetEntry() == 6251 && strahad->IsAlive() && strahad->IsInWorld() &&
                bot->IsInMap(strahad) && bot->InSamePhase(strahad))
                return bot->GetDistance(strahad) > INTERACTION_DISTANCE
                           ? MoveWorldObjectTo(guid)
                           : !strahad->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        }
    return MoveToQuestLocation(sObjectMgr->GetQuestTemplate(data.Quest), false);
}
