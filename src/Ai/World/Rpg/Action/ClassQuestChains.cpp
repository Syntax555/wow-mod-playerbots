/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ClassQuestChains.h"

#include <limits>
#include <optional>

#include "AttackAction.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "QuestPackets.h"
#include "UseItemAction.h"

namespace
{
constexpr uint32 TheAffray = 1719;
constexpr float WaitDistance = 3.0f;
constexpr uint32 StalledEventMs = 5 * MINUTE * IN_MILLISECONDS;

// Steps of one lesson stay together. The core owns race, level, prerequisite and era eligibility and all rewards.
ClassQuestStep const ClassQuestSteps[] = {
    // Warrior Defensive Stance: Barrens, Tirisfal, Stormwind, Ironforge, Darnassus and Azuremyst routes.
    {CLASS_WARRIOR, 1505, 71, 0, 0},
    {CLASS_WARRIOR, 1498, 71, 0, 0},
    {CLASS_WARRIOR, 1818, 71, 0, 0},
    {CLASS_WARRIOR, 1819, 71, 0, -104593},
    {CLASS_WARRIOR, 1638, 71, 0, 0},
    {CLASS_WARRIOR, 1639, 71, 0, 0},
    {CLASS_WARRIOR, 1640, 71, 0, 6090},
    {CLASS_WARRIOR, 1665, 71, 0, 0},
    {CLASS_WARRIOR, 1679, 71, 0, 0},
    {CLASS_WARRIOR, 1678, 71, 0, 0},
    {CLASS_WARRIOR, 1684, 71, 0, 0},
    {CLASS_WARRIOR, 1683, 71, 0, 0},
    {CLASS_WARRIOR, 9582, 71, 23925, -181849},
    // Warrior Berserker Stance and Intercept.
    {CLASS_WARRIOR, 1718, 2458, 0, 0},
    {CLASS_WARRIOR, TheAffray, 2458, 0, 0},
    // Paladin Redemption: human, dwarf, draenei and blood elf chains.
    {CLASS_PALADIN, 1643, 7328, 0, 0},
    {CLASS_PALADIN, 1644, 7328, 0, 0},
    {CLASS_PALADIN, 1780, 7328, 0, 0},
    {CLASS_PALADIN, 1781, 7328, 0, 0},
    {CLASS_PALADIN, 1786, 7328, 6866, 6172},
    {CLASS_PALADIN, 1787, 7328, 0, 0},
    {CLASS_PALADIN, 1788, 7328, 0, 0},
    {CLASS_PALADIN, 1647, 7328, 0, 0},
    {CLASS_PALADIN, 1648, 7328, 0, 0},
    {CLASS_PALADIN, 1778, 7328, 0, 0},
    {CLASS_PALADIN, 1779, 7328, 0, 0},
    {CLASS_PALADIN, 1783, 7328, 6866, 6177},
    {CLASS_PALADIN, 1784, 7328, 0, 0},
    {CLASS_PALADIN, 1785, 7328, 0, 0},
    {CLASS_PALADIN, 10366, 7328, 0, 0},
    {CLASS_PALADIN, 9598, 7328, 0, 0},
    {CLASS_PALADIN, 9600, 7328, 6866, 17542},
    {CLASS_PALADIN, 9677, 7328, 0, 0},
    {CLASS_PALADIN, 9678, 7328, 0, -181956},
    {CLASS_PALADIN, 9681, 7328, 0, 0},
    {CLASS_PALADIN, 9684, 7328, 24157, 17845},
    {CLASS_PALADIN, 9685, 7328, 24184, 17768},
    // Paladin Sense Undead: Daphne Stilwell's defense, or the blood elf Second Trial.
    {CLASS_PALADIN, 1793, 5502, 0, 0},
    {CLASS_PALADIN, 1794, 5502, 0, 0},
    {CLASS_PALADIN, 1649, 5502, 0, 0},
    {CLASS_PALADIN, 1650, 5502, 0, 0},
    {CLASS_PALADIN, 1651, 5502, 0, 6182},
    {CLASS_PALADIN, 1652, 5502, 0, 0},
    {CLASS_PALADIN, 9690, 5502, 0, 0},
    {CLASS_PALADIN, 9686, 5502, 0, -182052},
    {CLASS_PALADIN, 9691, 5502, 0, 0},
    // Paladin Warhorse and Thalassian Warhorse.
    {CLASS_PALADIN, 4485, 13819, 0, 0},
    {CLASS_PALADIN, 4486, 13819, 0, 0},
    {CLASS_PALADIN, 1661, 13819, 0, 0},
    {CLASS_PALADIN, 9712, 34769, 0, 0},
    // Priest racial lessons restored by Individual Progression.
    {CLASS_PRIEST, 5634, 948081, 0, 0},
    {CLASS_PRIEST, 5635, 948081, 0, 0},
    {CLASS_PRIEST, 5636, 948081, 0, 0},
    {CLASS_PRIEST, 5637, 948081, 0, 0},
    {CLASS_PRIEST, 5638, 948081, 0, 0},
    {CLASS_PRIEST, 5639, 948081, 0, 0},
    {CLASS_PRIEST, 5627, 949100, 0, 0},
    {CLASS_PRIEST, 5628, 949100, 0, 0},
    {CLASS_PRIEST, 5629, 949100, 0, 0},
    {CLASS_PRIEST, 5630, 949100, 0, 0},
    {CLASS_PRIEST, 5631, 949100, 0, 0},
    {CLASS_PRIEST, 5632, 949100, 0, 0},
    {CLASS_PRIEST, 5633, 949100, 0, 0},
    {CLASS_PRIEST, 5658, 949112, 0, 0},
    {CLASS_PRIEST, 5660, 949112, 0, 0},
    {CLASS_PRIEST, 5661, 949112, 0, 0},
    {CLASS_PRIEST, 5662, 949112, 0, 0},
    {CLASS_PRIEST, 5663, 949112, 0, 0},
    {CLASS_PRIEST, 5652, 949124, 0, 0},
    {CLASS_PRIEST, 5654, 949124, 0, 0},
    {CLASS_PRIEST, 5655, 949124, 0, 0},
    {CLASS_PRIEST, 5656, 949124, 0, 0},
    {CLASS_PRIEST, 5657, 949124, 0, 0},
    {CLASS_PRIEST, 5641, 948089, 0, 0},
    {CLASS_PRIEST, 5645, 948089, 0, 0},
    {CLASS_PRIEST, 5647, 948089, 0, 0},
    {CLASS_PRIEST, 5672, 949107, 0, 0},
    {CLASS_PRIEST, 5673, 949107, 0, 0},
    {CLASS_PRIEST, 5674, 949107, 0, 0},
    {CLASS_PRIEST, 5675, 949107, 0, 0},
    {CLASS_PRIEST, 5642, 949130, 0, 0},
    {CLASS_PRIEST, 5643, 949130, 0, 0},
    {CLASS_PRIEST, 5680, 949130, 0, 0},
    {CLASS_PRIEST, 5676, 949142, 0, 0},
    {CLASS_PRIEST, 5677, 949142, 0, 0},
    {CLASS_PRIEST, 5644, 949152, 0, 0},
    {CLASS_PRIEST, 5646, 949152, 0, 0},
    {CLASS_PRIEST, 5679, 949152, 0, 0},
};

// Twiggy Flathead's native script starts the Affray for a warrior within ten yards of him and on this grate.
WorldPosition const AffrayGrate(1, -1681.0f, -4327.0f, 2.79f);

// Reuse the ordinary attack and object-use paths with their native eligibility checks.
struct ClassQuestAttack : AttackAction
{
    using AttackAction::Attack;
    using AttackAction::AttackAction;
};

struct ClassQuestUse : UseItemAction
{
    using UseItemAction::UseGameObject;
    using UseItemAction::UseItemAction;
};

bool ClassQuestEarned(Player* bot, uint32 spell)
{
    if (bot->HasSpell(spell))
        return true;
    // Era-specific records can replace the learned spell; the rewarded lesson quest still ends the chain.
    for (auto const& step : ClassQuestSteps)
        if (step.Spell == spell && bot->GetQuestRewardStatus(step.Quest))
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(step.Quest);
                quest && (quest->GetRewSpell() || quest->GetRewSpellCast()))
                return true;
    return false;
}

Item* QuestStartItem(PlayerbotAI* botAI, uint32 questId)
{
    for (Item* item :
         botAI->GetAiObjectContext()->GetValue<std::vector<Item*>>("inventory items", "start quest")->Get())
        if (item->GetTemplate()->StartQuest == questId)
            return item;
    return nullptr;
}

bool Usable(Player* bot, WorldObject* object)
{
    return object->IsInWorld() && bot->IsInMap(object) && bot->InSamePhase(object) && bot->CanSeeOrDetect(object);
}

bool CanRunClassQuestChain(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return bot && CanRunClassQuest(botAI, bot->getClass());
}
}  // namespace

ClassQuestStep const* CurrentClassQuestStep(PlayerbotAI* botAI)
{
    if (!CanRunClassQuestChain(botAI))
        return nullptr;
    Player* bot = botAI->GetBot();
    std::optional<ClassQuestRouteCosts> costs;
    ClassQuestStep const* selected = nullptr;
    float nearest = std::numeric_limits<float>::max();
    uint32 lesson = 0;
    bool earned = false;
    for (auto const& step : ClassQuestSteps)
    {
        if (step.Class != bot->getClass())
            continue;
        if (step.Spell != lesson)
        {
            if (selected)
                return selected;
            lesson = step.Spell;
            earned = ClassQuestEarned(bot, lesson);
        }
        if (earned || sPlayerbotAIConfig.IgnoredQuests.count(step.Quest))
            continue;
        Quest const* quest = sObjectMgr->GetQuestTemplate(step.Quest);
        if (!quest)
            continue;
        QuestStatus const status = bot->GetQuestStatus(step.Quest);
        if (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_FAILED)
            return &step;
        if (status != QUEST_STATUS_NONE || !bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false))
            continue;
        if (QuestStartItem(botAI, step.Quest))
            return &step;
        if (!costs)
            costs.emplace(botAI);
        WorldPosition const pos = NearestClassQuestLocation(step.Quest, uint32(QuestRelationFlag::questGiver), *costs);
        if (pos != WorldPosition() && costs->Cost(pos) < nearest)
        {
            selected = &step;
            nearest = costs->Cost(pos);
        }
    }
    return selected;
}

uint32 ClassQuestChainValue::Calculate()
{
    auto const* step = CurrentClassQuestStep(botAI);
    return step ? step->Quest : 0;
}

bool ClassQuestChainTrigger::IsActive()
{
    return CanRunClassQuestChain(botAI) && AI_VALUE(uint32, "class quest chain") != 0;
}

bool ClassQuestChainAction::isUseful()
{
    return CanRunClassQuestChain(botAI) && AI_VALUE(uint32, "class quest chain") != 0;
}

bool ClassQuestChainAction::Execute(Event /*event*/)
{
    auto const* step = CurrentClassQuestStep(botAI);
    if (!step)
        return false;
    Quest const* quest = sObjectMgr->GetQuestTemplate(step->Quest);
    QuestStatus const status = bot->GetQuestStatus(step->Quest);
    // A failed trial or escort is abandoned as a player would, so its giver offers it again.
    if (status == QUEST_STATUS_FAILED)
        return Abandon(step->Quest);
    if (status == QUEST_STATUS_NONE)
    {
        if (Item* item = QuestStartItem(botAI, step->Quest))
            return AcceptQuest(quest, item->GetGUID()) && bot->GetQuestStatus(step->Quest) != QUEST_STATUS_NONE;
        return InteractWithQuestGiver(quest, false) || MoveToQuestLocation(quest, false);
    }
    if (status == QUEST_STATUS_COMPLETE)
        return InteractWithQuestGiver(quest, true) || (step->Item && step->Target > 0 && UseOnCreature(quest, *step)) ||
               MoveToQuestLocation(quest, true);
    if (status != QUEST_STATUS_INCOMPLETE)
        return false;
    if (step->Quest == TheAffray)
        return Wait(quest, AffrayGrate);
    if (step->Target < 0)
        return UseObject(quest, *step);
    if (step->Target > 0)
        return step->Item ? UseOnCreature(quest, *step) : StayWith(quest, *step);
    return DoObjectives(quest);
}

bool ClassQuestChainAction::UseOnCreature(Quest const* quest, ClassQuestStep const& step)
{
    Item* item = bot->GetItemByEntry(step.Item);
    bool const revive = bot->GetQuestStatus(quest->GetQuestId()) == QUEST_STATUS_COMPLETE;
    if (!item)
        return !revive && Restart(quest, step.Item);
    Creature* target = nullptr;
    float distance = std::numeric_limits<float>::max();
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != uint32(step.Target) || !creature->IsAlive() ||
            creature->IsDuringRemoveFromWorld() || !Usable(bot, creature))
            continue;
        // Henze and Narm Faulk accept the turn-in once the owned Symbol of Life has revived them.
        if (revive &&
            (!creature->hasInvolvedQuest(quest->GetQuestId()) || creature->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER)))
            continue;
        auto const used = _usedTargets.find(creature->GetSpawnId());
        if (used != _usedTargets.end() && GetMSTimeDiffToNow(used->second) < MINUTE * IN_MILLISECONDS)
            continue;
        if (bot->GetDistance(creature) < distance)
        {
            target = creature;
            distance = bot->GetDistance(creature);
        }
    }
    if (!target)
    {
        if (revive)
            return false;
        WorldPosition const pos = SpawnPosition(step.Target);
        return pos != WorldPosition() &&
               (pos.GetMapId() != bot->GetMapId() || bot->GetExactDist(pos) > INTERACTION_DISTANCE) &&
               MoveToQuestPosition(pos);
    }
    if (!target->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(target))
        return MoveToQuestPosition(WorldPosition(target));
    bot->StopMoving();
    if (!UseItemAction(botAI).UseItemOnUnit(item, target))
        return false;
    _usedTargets[target->GetSpawnId()] = getMSTime();
    Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!cast)
        cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (cast)
        botAI->WaitForSpellCast(cast);
    return true;
}

bool ClassQuestChainAction::UseObject(Quest const* quest, ClassQuestStep const& step)
{
    Item* item = step.Item ? bot->GetItemByEntry(step.Item) : nullptr;
    if (step.Item && !item)
        return Restart(quest, step.Item);
    // A released or summoned opponent is fought through ordinary combat; the object is not used again.
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsAlive() || !Usable(bot, creature) || !bot->IsValidAttackTarget(creature))
            continue;
        for (uint32 index = 0; index < QUEST_OBJECTIVES_COUNT; ++index)
            if (quest->RequiredNpcOrGo[index] == int32(creature->GetEntry()) &&
                ClassQuestObjectiveNeeded(bot, quest, index))
                return bot->IsWithinLOSInMap(creature) ? ClassQuestAttack(botAI, "class quest attack").Attack(creature)
                                                       : MoveToQuestPosition(WorldPosition(creature));
    }
    if (_usedQuest == quest->GetQuestId() && _usedAt && GetMSTimeDiffToNow(_usedAt) < MINUTE * IN_MILLISECONDS)
        return Wait(quest, _usedPosition);
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* object = botAI->GetGameObject(guid);
        if (!object || object->GetEntry() != uint32(-step.Target) || !object->isSpawned() || !Usable(bot, object))
            continue;
        if (!object->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(object))
            return MoveToQuestPosition(WorldPosition(object));
        bot->StopMoving();
        ClassQuestUse use(botAI);
        if (!(item ? use.UseItemOnGameObject(item, guid) : use.UseGameObject(guid)))
            return false;
        _usedQuest = quest->GetQuestId();
        _usedAt = getMSTime();
        _usedPosition = WorldPosition(object);
        if (Spell* cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            botAI->WaitForSpellCast(cast);
        return true;
    }
    return Wait(quest, SpawnPosition(step.Target));
}

bool ClassQuestChainAction::StayWith(Quest const* quest, ClassQuestStep const& step)
{
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != uint32(step.Target) || !creature->IsAlive() || !Usable(bot, creature))
            continue;
        // Daphne Stilwell's raiders attack her, not the escorting paladin.
        for (Unit* attacker : creature->getAttackers())
            if (attacker && Usable(bot, attacker) && ClassQuestAttack(botAI, "class quest attack").Attack(attacker))
                return true;
        return Wait(quest, WorldPosition(creature));
    }
    return Wait(quest, SpawnPosition(step.Target));
}

bool ClassQuestChainAction::Wait(Quest const* quest, WorldPosition const& pos)
{
    if (pos == WorldPosition())
        return false;
    if (pos.GetMapId() != bot->GetMapId() || bot->GetExactDist(pos) > WaitDistance)
        return MoveToQuestPosition(pos);
    // Combat pauses this action, so a gap between waits restarts the stall clock.
    if (_waitQuest != quest->GetQuestId() || !_waitLast || GetMSTimeDiffToNow(_waitLast) > 10 * IN_MILLISECONDS)
    {
        _waitQuest = quest->GetQuestId();
        _waitSince = getMSTime();
    }
    _waitLast = getMSTime();
    if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT) ||
        GetMSTimeDiffToNow(_waitSince) < StalledEventMs)
        return true;
    // A busy or reset native event never started; accepting the quest again restarts it.
    _waitLast = 0;
    return Abandon(quest->GetQuestId());
}

bool ClassQuestChainAction::Restart(Quest const* quest, uint32 itemId)
{
    // Only the step's own source item is restored by accepting the quest again.
    if (quest->GetSrcItemId() != itemId || bot->HasItemCount(itemId, 1, true))
        return false;
    return Abandon(quest->GetQuestId());
}

bool ClassQuestChainAction::Abandon(uint32 questId)
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        if (bot->GetQuestSlotQuestId(slot) == questId)
        {
            WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST);
            packet << uint8(slot);
            WorldPackets::Quest::QuestLogRemoveQuest request(std::move(packet));
            request.Read();
            bot->GetSession()->HandleQuestLogRemoveQuest(request);
            return bot->GetQuestStatus(questId) == QUEST_STATUS_NONE;
        }
    return false;
}

WorldPosition ClassQuestChainAction::SpawnPosition(int32 entry)
{
    if (_spawnEntry == entry && _spawnMap == bot->GetMapId() && _spawnChecked &&
        GetMSTimeDiffToNow(_spawnChecked) < MINUTE * IN_MILLISECONDS)
        return _spawnPosition;
    _spawnEntry = entry;
    _spawnMap = bot->GetMapId();
    _spawnChecked = getMSTime();
    _spawnPosition = WorldPosition();
    ClassQuestRouteCosts const costs(botAI);
    WorldPosition origin;
    float nearest = std::numeric_limits<float>::max();
    auto consider = [&](auto const* spawn)
    {
        if (!(spawn->phaseMask & bot->GetPhaseMask()))
            return;
        WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
        if (costs.Cost(pos) < nearest)
        {
            _spawnPosition = pos;
            nearest = costs.Cost(pos);
        }
    };
    if (entry > 0)
        for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, uint32(entry)))
            consider(spawn);
    else
        for (GameObjectData const* spawn : origin.getGameObjectsNear(0.0f, uint32(-entry)))
            consider(spawn);
    return _spawnPosition;
}
