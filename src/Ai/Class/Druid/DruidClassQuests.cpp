/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DruidClassQuests.h"

#include <cmath>
#include <limits>
#include <optional>

#include "AddLootAction.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "LootObjectStack.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "QuestPackets.h"
#include "SharedValueContext.h"
#include "SpellMgr.h"
#include "UseItemAction.h"

namespace
{
constexpr uint32 BearForm = 5487;
constexpr uint32 AquaticForm = 1066;
constexpr uint32 CurePoison = 8946;
constexpr uint32 Lunaclaw = 12138;
constexpr uint32 LunaclawSpirit = 12144;
constexpr uint32 ShrineBauble = 15877;
constexpr uint32 EndurancePendant = 15882;
constexpr uint32 AgilityPendant = 15883;
constexpr uint32 CompletePendant = 15885;
constexpr uint32 CurativeSalve = 15826;
constexpr int32 MaximumQuestGrindLevelDifference = 4;

DruidClassQuest const DruidClassQuests[] = {
    {5921, BearForm},   {5922, BearForm},    {5923, BearForm},   {5924, BearForm},   {5925, BearForm},
    {5926, BearForm},   {5927, BearForm},    {5928, BearForm},   {5929, BearForm},   {5930, BearForm},
    {5931, BearForm},   {5932, BearForm},    {6001, BearForm},   {6002, BearForm},   {26, AquaticForm},
    {27, AquaticForm},  {28, AquaticForm},   {29, AquaticForm},  {30, AquaticForm},  {31, AquaticForm},
    {272, AquaticForm}, {5061, AquaticForm}, {6121, CurePoison}, {6122, CurePoison}, {6123, CurePoison},
    {6124, CurePoison}, {6125, CurePoison},  {6126, CurePoison}, {6127, CurePoison}, {6128, CurePoison},
    {6129, CurePoison}, {6130, CurePoison},
};

bool DruidQuestEarned(Player* bot, uint32 spell)
{
    if (bot->HasSpell(spell))
        return true;
    if (spell == BearForm)
        return bot->GetQuestRewardStatus(6001) || bot->GetQuestRewardStatus(6002);
    if (spell == AquaticForm)
        return bot->GetQuestRewardStatus(31) || bot->GetQuestRewardStatus(5061);
    return bot->GetQuestRewardStatus(6125) || bot->GetQuestRewardStatus(6130);
}

Creature* NearbyDruidCreature(PlayerbotAI* botAI, uint32 entry)
{
    Player* bot = botAI->GetBot();
    Creature* nearest = nullptr;
    float distance = std::numeric_limits<float>::max();
    for (ObjectGuid guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != entry || !creature->IsAlive() || !creature->IsInWorld() ||
            creature->IsDuringRemoveFromWorld() || !bot->IsInMap(creature) || !bot->InSamePhase(creature) ||
            !bot->CanSeeOrDetect(creature))
            continue;
        float const candidate = bot->GetDistance(creature);
        if (candidate < distance)
        {
            nearest = creature;
            distance = candidate;
        }
    }
    return nearest;
}
}  // namespace

DruidClassQuest const* CurrentDruidClassQuest(PlayerbotAI* botAI)
{
    if (!CanRunClassQuest(botAI, CLASS_DRUID))
        return nullptr;
    Player* bot = botAI->GetBot();
    std::optional<ClassQuestRouteCosts> costs;
    for (uint32 spell : {BearForm, AquaticForm, CurePoison})
    {
        if (DruidQuestEarned(bot, spell))
            continue;
        DruidClassQuest const* selected = nullptr;
        float nearest = std::numeric_limits<float>::max();
        for (auto const& data : DruidClassQuests)
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
            float const distance = costs->Cost(pos);
            if (pos != WorldPosition() && distance < nearest)
            {
                selected = &data;
                nearest = distance;
            }
        }
        if (selected)
            return selected;
    }
    return nullptr;
}

uint32 DruidClassQuestValue::Calculate()
{
    auto const* quest = CurrentDruidClassQuest(botAI);
    return quest ? quest->Quest : 0;
}

bool DruidClassQuestTrigger::IsActive()
{
    return CanRunClassQuest(botAI, CLASS_DRUID) && AI_VALUE(uint32, "druid class quest") != 0;
}

bool DruidClassQuestAction::isUseful()
{
    return CanRunClassQuest(botAI, CLASS_DRUID) && AI_VALUE(uint32, "druid class quest") != 0;
}

bool DruidClassQuestAction::Execute(Event /*event*/)
{
    auto const* data = CurrentDruidClassQuest(botAI);
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
    if (data->Quest == 5929 || data->Quest == 5930)
        return TalkToSpirit(quest, 11956);
    if (data->Quest == 6001 || data->Quest == 6002)
        return BodyAndHeart(quest);
    if (data->Quest == 28 || data->Quest == 29)
        return bot->HasItemCount(ShrineBauble) ? UseFocusedItem(quest, ShrineBauble)
                                               : GatherQuestItem(quest, ShrineBauble);
    if (data->Quest == 30 || data->Quest == 272)
    {
        if (bot->HasItemCount(CompletePendant))
            return false;
        if (!bot->HasItemCount(EndurancePendant))
            return GatherQuestItem(quest, EndurancePendant);
        if (!bot->HasItemCount(AgilityPendant))
            return GatherQuestItem(quest, AgilityPendant);
        return UseFocusedItem(quest, AgilityPendant);
    }
    if (data->Quest == 6122 || data->Quest == 6127)
        return UseFocusedItem(quest, quest->GetSrcItemId());
    if (data->Quest == 6124 || data->Quest == 6129)
        return CureSick(quest);
    if (data->Quest == 6123 || data->Quest == 6128)
    {
        if (AcquireEarthroot(quest))
            return true;
        uint32 const material = data->Quest == 6123 ? 15851 : 15852;
        return bot->HasQuestForItem(material) && GatherQuestItem(quest, material);
    }
    return DoObjectives(quest);
}

bool DruidClassQuestAction::TalkToSpirit(Quest const* quest, uint32 entry)
{
    Creature* spirit = NearbyDruidCreature(botAI, entry);
    if (!spirit)
    {
        if (_spiritEntry != entry || _spiritMap != bot->GetMapId() || !_spiritChecked ||
            GetMSTimeDiffToNow(_spiritChecked) >= MINUTE * IN_MILLISECONDS)
        {
            _spiritEntry = entry;
            _spiritMap = bot->GetMapId();
            _spiritChecked = getMSTime();
            _spiritPosition = WorldPosition();
            ClassQuestRouteCosts const costs(botAI);
            WorldPosition origin;
            float distance = std::numeric_limits<float>::max();
            for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, entry))
            {
                if (!(spawn->phaseMask & bot->GetPhaseMask()))
                    continue;
                WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
                float const cost = costs.Cost(pos);
                if (cost < distance)
                {
                    _spiritPosition = pos;
                    distance = cost;
                }
            }
        }
        return _spiritPosition != WorldPosition() && MoveToQuestPosition(_spiritPosition);
    }
    if (!spirit->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(spirit))
        return MoveToQuestPosition(WorldPosition(spirit));
    if (!bot->GetNPCIfCanInteractWith(spirit->GetGUID(), UNIT_NPC_FLAG_GOSSIP) || !bot->PlayerTalkClass)
        return false;
    bot->StopMoving();
    GossipMenu& menu = bot->PlayerTalkClass->GetGossipMenu();
    uint32 const current = menu.GetMenuId();
    bool const allowed = entry == LunaclawSpirit
                             ? current == 3862
                             : (current == 3882 || current == 3881 || current == 3883 || current == 3884);
    if (menu.GetSenderGUID() != spirit->GetGUID() || !allowed)
    {
        WorldPacket packet(CMSG_GOSSIP_HELLO);
        packet << spirit->GetGUID();
        bot->GetSession()->HandleGossipHelloOpcode(packet);
        return true;
    }
    uint32 const next =
        current == 3882 ? 3881 : (current == 3881 ? 3883 : (current == 3883 ? 3884 : (current == 3884 ? 3885 : 3863)));
    for (auto const& option : menu.GetMenuItems())
    {
        GossipMenuItemData const* details = menu.GetItemData(option.first);
        if (option.second.IsCoded || option.second.BoxMoney || option.second.OptionType != GOSSIP_OPTION_GOSSIP ||
            !details || details->GossipActionMenuId != next)
            continue;
        WorldPacket packet(CMSG_GOSSIP_SELECT_OPTION);
        packet << spirit->GetGUID() << menu.GetMenuId() << option.first;
        bot->GetSession()->HandleGossipSelectOptionOpcode(packet);
        return bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_FAILED;
    }
    return false;
}

WorldPosition DruidClassQuestAction::FocusPosition(uint32 itemId, uint32 summonEntry)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
    SpellInfo const* spell = item ? sSpellMgr->GetSpellInfo(item->Spells[0].SpellId) : nullptr;
    if (!spell || !spell->RequiresSpellFocus)
        return WorldPosition();
    if (_focusItem == itemId && _focusId == spell->RequiresSpellFocus && _focusSummon == summonEntry &&
        _focusMap == bot->GetMapId() && _focusChecked && GetMSTimeDiffToNow(_focusChecked) < MINUTE * IN_MILLISECONDS)
        return _focusPosition;
    _focusItem = itemId;
    _focusId = spell->RequiresSpellFocus;
    _focusSummon = summonEntry;
    _focusMap = bot->GetMapId();
    _focusChecked = getMSTime();
    _focusPosition = WorldPosition();
    _focusEntry = 0;
    _summonPosition = WorldPosition();
    std::optional<ScriptInfo> summon;
    if (summonEntry)
    {
        for (auto const& effect : spell->Effects)
            if (effect.Effect == SPELL_EFFECT_SEND_EVENT)
                if (auto const events = sEventScripts.find(effect.MiscValue); events != sEventScripts.end())
                    for (auto const& event : events->second)
                        if (event.second.command == SCRIPT_COMMAND_TEMP_SUMMON_CREATURE &&
                            event.second.TempSummonCreature.CreatureEntry == summonEntry)
                            summon = event.second;
        if (!summon)
            return WorldPosition();
    }
    ClassQuestRouteCosts const costs(botAI);
    WorldPosition origin;
    float nearest = std::numeric_limits<float>::max();
    for (auto const& entry : *sObjectMgr->GetGameObjectTemplates())
        if (entry.second.type == GAMEOBJECT_TYPE_SPELL_FOCUS && entry.second.spellFocus.focusId == _focusId)
            for (GameObjectData const* spawn : origin.getGameObjectsNear(0.0f, entry.first))
            {
                if (!(spawn->phaseMask & bot->GetPhaseMask()))
                    continue;
                WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
                WorldPosition target;
                if (summon)
                {
                    target = WorldPosition(spawn->mapid, summon->TempSummonCreature.PosX,
                                           summon->TempSummonCreature.PosY, summon->TempSummonCreature.PosZ);
                    // The shared focus ID also exists at the other faction's ritual site.
                    if (pos.GetExactDist(target) > 100.0f)
                        continue;
                }
                float const cost = costs.Cost(pos);
                if (cost < nearest)
                {
                    _focusPosition = pos;
                    _focusEntry = entry.first;
                    _summonPosition = target;
                    nearest = cost;
                }
            }
    return _focusPosition;
}

bool DruidClassQuestAction::UseFocusedItem(Quest const* /*quest*/, uint32 itemId, uint32 summonEntry)
{
    Item* item = bot->GetItemByEntry(itemId);
    if (!item)
        return false;
    WorldPosition const pos = FocusPosition(itemId, summonEntry);
    if (pos == WorldPosition())
        return false;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* focus = botAI->GetGameObject(guid);
        if (!focus || focus->GetEntry() != _focusEntry || !focus->IsInWorld() || !focus->isSpawned() ||
            !bot->IsInMap(focus) || !bot->InSamePhase(focus) || focus->GetGoType() != GAMEOBJECT_TYPE_SPELL_FOCUS ||
            focus->GetGOInfo()->spellFocus.focusId != _focusId || focus->GetExactDist(pos) > INTERACTION_DISTANCE)
            continue;
        if (!focus->IsWithinDistInMap(bot, focus->GetGOInfo()->spellFocus.dist))
            return MoveToQuestPosition(pos);
        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnUnit(item, bot))
            return false;
        Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!cast)
            cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (cast)
            botAI->WaitForSpellCast(cast);
        if (summonEntry)
            _bearMissingSince = getMSTime();
        return true;
    }
    return (pos.GetMapId() != bot->GetMapId() || bot->GetExactDist(pos) > INTERACTION_DISTANCE) &&
           MoveToQuestPosition(pos);
}

bool DruidClassQuestAction::GatherQuestItem(Quest const* quest, uint32 itemId)
{
    if (!bot->HasQuestForItem(itemId))
        return false;
    if (AI_VALUE(LootObjectStack*, "available loot")->IsLootPending() || bot->GetLootGUID())
        return true;
    if (_lootItem != itemId || _lootMap != bot->GetMapId() || !_lootChecked ||
        GetMSTimeDiffToNow(_lootChecked) >= MINUTE * IN_MILLISECONDS)
    {
        if (_lootItem != itemId)
            _lootUnavailable.clear();
        _lootItem = itemId;
        _lootMap = bot->GetMapId();
        _lootChecked = getMSTime();
        _lootPosition = WorldPosition();
        _lootEntry = 0;
        ClassQuestRouteCosts const costs(botAI);
        WorldPosition origin;
        float nearest = std::numeric_limits<float>::max();
        auto consider = [&](auto const* spawn, uint32 entry, bool creature)
        {
            auto const blocked = _lootUnavailable.find(spawn->spawnId);
            if (!(spawn->phaseMask & bot->GetPhaseMask()) ||
                (blocked != _lootUnavailable.end() && GetMSTimeDiffToNow(blocked->second) < MINUTE * IN_MILLISECONDS))
                return;
            WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
            float const cost = costs.Cost(pos);
            if (cost < nearest)
            {
                _lootEntry = entry;
                _lootIsCreature = creature;
                _lootSpawn = spawn->spawnId;
                _lootPosition = pos;
                nearest = cost;
            }
        };
        for (int32 entry : GAI_VALUE2(std::vector<int32>, "item drop list", itemId))
            if (entry > 0)
            {
                CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(uint32(entry));
                if (!info || int32(info->minlevel) - int32(bot->GetLevel()) > MaximumQuestGrindLevelDifference)
                    continue;
                for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, uint32(entry)))
                    consider(spawn, uint32(entry), true);
            }
            else if (entry < 0 && !sPlayerbotAIConfig.DisallowedGameObjects.contains(uint32(-entry)))
                for (GameObjectData const* spawn : origin.getGameObjectsNear(0.0f, uint32(-entry)))
                    consider(spawn, uint32(-entry), false);
    }
    if (_lootPosition == WorldPosition())
        return false;
    if (_lootIsCreature)
    {
        auto* state = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data);
        if (!state || state->questId != quest->GetQuestId())
            botAI->rpgInfo.ChangeToDoQuest(quest->GetQuestId(), quest);
        if (_lootPosition.GetMapId() != bot->GetMapId())
            return MoveToQuestPosition(_lootPosition);
        bool const approach =
            std::abs(bot->GetPositionZ() - _lootPosition.GetPositionZ()) > 3.0f ||
            !bot->IsWithinLOS(_lootPosition.GetPositionX(), _lootPosition.GetPositionY(), _lootPosition.GetPositionZ());
        float const range = approach ? INTERACTION_DISTANCE : sPlayerbotAIConfig.SightDistance;
        return bot->GetExactDist(_lootPosition) > range && MoveToQuestPosition(_lootPosition);
    }
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* object = botAI->GetGameObject(guid);
        if (!object || object->GetEntry() != _lootEntry || object->GetSpawnId() != _lootSpawn || !object->IsInWorld() ||
            !object->isSpawned() || !bot->IsInMap(object) || !bot->InSamePhase(object) ||
            !bot->CanSeeOrDetect(object) || object->GetExactDist(_lootPosition) > INTERACTION_DISTANCE)
            continue;
        if (!object->IsAtInteractDistance(bot) || !bot->IsWithinLOSInMap(object))
            return MoveToQuestPosition(_lootPosition);
        AddLootAction(botAI).Execute(Event("druid quest", guid));
        return false;  // Ordinary loot, movement and opening actions must be allowed to run.
    }
    if (_lootPosition.GetMapId() != bot->GetMapId() || bot->GetExactDist(_lootPosition) > INTERACTION_DISTANCE)
        return MoveToQuestPosition(_lootPosition);
    _lootUnavailable[_lootSpawn] = getMSTime();
    _lootChecked = 0;
    return false;
}

bool DruidClassQuestAction::CureSick(Quest const* quest)
{
    Item* salve = bot->GetItemByEntry(CurativeSalve);
    if (!salve)
        return false;
    uint32 const entry = quest->GetQuestId() == 6124 ? 12298 : 12296;
    if (Creature* sick = NearbyDruidCreature(botAI, entry))
    {
        if (!sick->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(sick))
            return MoveToQuestPosition(WorldPosition(sick));
        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnUnit(salve, sick))
            return false;
        _sickUnavailable[sick->GetSpawnId()] = getMSTime();
        _sickChecked = 0;
        if (Spell* cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            botAI->WaitForSpellCast(cast);
        return true;
    }
    if (_sickEntry != entry || _sickMap != bot->GetMapId() || !_sickChecked ||
        GetMSTimeDiffToNow(_sickChecked) >= MINUTE * IN_MILLISECONDS)
    {
        _sickEntry = entry;
        _sickMap = bot->GetMapId();
        _sickChecked = getMSTime();
        _sickPosition = WorldPosition();
        ClassQuestRouteCosts const costs(botAI);
        WorldPosition origin;
        float nearest = std::numeric_limits<float>::max();
        for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, entry))
        {
            auto const blocked = _sickUnavailable.find(spawn->spawnId);
            if (!(spawn->phaseMask & bot->GetPhaseMask()) ||
                (blocked != _sickUnavailable.end() && GetMSTimeDiffToNow(blocked->second) < MINUTE * IN_MILLISECONDS))
                continue;
            WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
            float const cost = costs.Cost(pos);
            if (cost < nearest)
            {
                _sickPosition = pos;
                _sickSpawn = spawn->spawnId;
                nearest = cost;
            }
        }
    }
    if (_sickPosition == WorldPosition())
        return false;
    if (_sickPosition.GetMapId() != bot->GetMapId() || bot->GetExactDist(_sickPosition) > INTERACTION_DISTANCE)
        return MoveToQuestPosition(_sickPosition);
    _sickUnavailable[_sickSpawn] = getMSTime();
    _sickChecked = 0;
    return false;
}

bool DruidClassQuestAction::BodyAndHeart(Quest const* quest)
{
    if (_bearQuest != quest->GetQuestId())
    {
        _bearQuest = quest->GetQuestId();
        _bearMissingSince = 0;
    }
    if (NearbyDruidCreature(botAI, LunaclawSpirit))
        return TalkToSpirit(quest, LunaclawSpirit);
    if (Creature* lunaclaw = NearbyDruidCreature(botAI, Lunaclaw))
    {
        _bearMissingSince = 0;
        return bot->GetDistance(lunaclaw) > INTERACTION_DISTANCE ? MoveToQuestPosition(WorldPosition(lunaclaw)) : false;
    }
    uint32 const itemId = quest->GetSrcItemId();
    if (bot->GetItemByEntry(itemId))
        return UseFocusedItem(quest, itemId, Lunaclaw);
    if (bot->HasItemCount(itemId, 1, true))
        return false;
    WorldPosition const focus = FocusPosition(itemId, Lunaclaw);
    if (focus == WorldPosition())
        return false;
    WorldPosition const waiting = _summonPosition == WorldPosition() ? focus : _summonPosition;
    if (waiting.GetMapId() != bot->GetMapId() || bot->GetExactDist(waiting) > INTERACTION_DISTANCE)
        return MoveToQuestPosition(waiting);
    if (!_bearMissingSince)
        _bearMissingSince = getMSTime();
    if (GetMSTimeDiffToNow(_bearMissingSince) < 45 * IN_MILLISECONDS)
        return true;
    return RestartLunaclawQuest(quest);
}

bool DruidClassQuestAction::RestartLunaclawQuest(Quest const* quest)
{
    uint32 const id = quest->GetQuestId();
    if ((id != 6001 && id != 6002) || bot->GetQuestStatus(id) != QUEST_STATUS_INCOMPLETE ||
        bot->GetQuestRewardStatus(id) || bot->HasItemCount(quest->GetSrcItemId(), 1, true))
        return false;
    for (uint32 index = 0; index < QUEST_OBJECTIVES_COUNT; ++index)
        if (quest->RequiredNpcOrGo[index] || quest->ItemDrop[index])
            return false;
    for (uint32 item : quest->RequiredItemId)
        if (item)
            return false;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (bot->GetQuestSlotQuestId(slot) == id)
        {
            WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST);
            packet << uint8(slot);
            WorldPackets::Quest::QuestLogRemoveQuest request(std::move(packet));
            request.Read();
            bot->GetSession()->HandleQuestLogRemoveQuest(request);
            if (bot->GetQuestStatus(id) != QUEST_STATUS_NONE)
                return false;
            _bearMissingSince = 0;
            return true;
        }
    return false;
}

bool DruidClassQuestAction::AcquireEarthroot(Quest const* quest)
{
    constexpr uint32 itemId = 2449;
    uint32 required = 0;
    for (uint32 index = 0; index < QUEST_ITEM_OBJECTIVES_COUNT; ++index)
        if (quest->RequiredItemId[index] == itemId)
            required = quest->RequiredItemCount[index];
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!required || bot->GetItemCount(itemId, false) >= required || !proto || !proto->BuyCount ||
        (proto->BuyPrice && !bot->GetMoney()))
        return false;
    auto offerSlot = [](VendorItemData const* items) -> std::optional<uint32>
    {
        if (items)
            for (uint32 slot = 0; slot < items->GetItemCount(); ++slot)
                if (VendorItem const* offer = items->GetItem(slot))
                    if (offer->item == itemId && !offer->ExtendedCost)
                        return slot;
        return std::nullopt;
    };
    auto unavailable = [this]()
    {
        _rootUnavailable[_rootSpawn] = getMSTime();
        _rootChecked = 0;
        return false;
    };
    if (_rootMap != bot->GetMapId() || !_rootChecked || GetMSTimeDiffToNow(_rootChecked) >= MINUTE * IN_MILLISECONDS)
    {
        _rootMap = bot->GetMapId();
        _rootChecked = getMSTime();
        _rootEntry = 0;
        _rootPosition = WorldPosition();
        ClassQuestRouteCosts const costs(botAI);
        WorldPosition origin;
        float nearest = std::numeric_limits<float>::max();
        for (uint32 entry : {844u, 1302u, 1313u, 3490u, 3548u, 4083u, 4878u, 8305u, 14846u, 16224u, 16641u, 16705u})
        {
            CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry);
            FactionTemplateEntry const* faction = info ? sFactionTemplateStore.LookupEntry(info->faction) : nullptr;
            if (!faction || bot->GetFactionReactionTo(bot->GetFactionTemplateEntry(), faction) < REP_NEUTRAL ||
                !offerSlot(sObjectMgr->GetNpcVendorItemList(entry)))
                continue;
            for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, entry))
            {
                auto const blocked = _rootUnavailable.find(spawn->spawnId);
                if (!(spawn->phaseMask & bot->GetPhaseMask()) ||
                    (blocked != _rootUnavailable.end() &&
                     GetMSTimeDiffToNow(blocked->second) < 10 * MINUTE * IN_MILLISECONDS))
                    continue;
                WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
                float const cost = costs.Cost(pos);
                if (cost < nearest)
                {
                    nearest = cost;
                    _rootEntry = entry;
                    _rootSpawn = spawn->spawnId;
                    _rootPosition = pos;
                }
            }
        }
    }
    if (!_rootEntry || _rootPosition == WorldPosition())
        return false;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* vendor = unit ? unit->ToCreature() : nullptr;
        if (!vendor || vendor->GetEntry() != _rootEntry || vendor->GetSpawnId() != _rootSpawn || !vendor->IsAlive() ||
            !vendor->IsInWorld() || !bot->IsInMap(vendor) || !bot->InSamePhase(vendor) ||
            !bot->CanSeeOrDetect(vendor) || !vendor->HasNpcFlag(UNIT_NPC_FLAG_VENDOR) || vendor->IsHostileTo(bot))
            continue;
        if (!vendor->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(vendor))
            return MoveToQuestPosition(WorldPosition(vendor)) || unavailable();
        if (!bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_VENDOR))
            return unavailable();
        bot->StopMoving();
        bot->GetSession()->SendListInventory(guid);
        uint32 const vendorContext = bot->GetSession()->GetCurrentVendor();
        VendorItemData const* items =
            vendorContext ? sObjectMgr->GetNpcVendorItemList(vendorContext) : vendor->GetVendorItems();
        auto const slot = offerSlot(items);
        if (!slot)
            return unavailable();
        VendorItem const* offer = items->GetItem(*slot);
        uint32 const price = uint32(std::floor(proto->BuyPrice * bot->GetReputationPriceDiscount(vendor)));
        if ((offer->maxcount && vendor->GetVendorItemCurrentCount(offer) < proto->BuyCount) ||
            !bot->HasEnoughMoney(price))
            return unavailable();
        uint32 const before = bot->GetItemCount(itemId, false);
        bot->BuyItemFromVendorSlot(guid, *slot, itemId, 1, NULL_BAG, NULL_SLOT);
        return bot->GetItemCount(itemId, false) > before || unavailable();
    }
    if (_rootPosition.GetMapId() != bot->GetMapId() || bot->GetExactDist(_rootPosition) > INTERACTION_DISTANCE)
        return MoveToQuestPosition(_rootPosition) || unavailable();
    return unavailable();
}
