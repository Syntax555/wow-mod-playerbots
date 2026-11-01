/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HunterTamingLessons.h"

#include <algorithm>
#include <array>
#include <set>

#include "HunterActions.h"
#include "Item.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "UseItemAction.h"

namespace
{
constexpr uint32 SPELL_TAME_BEAST = 1515;
constexpr uint32 SPELL_FEED_PET = 6991;
constexpr uint32 SPELL_REVIVE_PET = 982;
constexpr uint32 SPAWN_SEARCH_INTERVAL = 60000;

// Native class quests, their source items and the rods' required creature targets.
constexpr std::array<HunterTamingLesson, 24> kTamingLessons = {{
    {6061, 15914, 19688, 2956},
    {6087, 15915, 19689, 2959},
    {6088, 15916, 19692, 2970},
    {6062, 15917, 19694, 3099},
    {6083, 15919, 19696, 3107},
    {6082, 15920, 19697, 3126},
    {6064, 15911, 19674, 1126},
    {6084, 15913, 19687, 1201},
    {6085, 15908, 19548, 1196},
    {6063, 15921, 19693, 1998},
    {6101, 15922, 19699, 2043},
    {6102, 15923, 19700, 1996},
    {9484, 23697, 30099, 15650},
    {9486, 23702, 30102, 15652},
    {9485, 23703, 30105, 16353},
    {9591, 23896, 30646, 17217},
    {9592, 23897, 30653, 17374},
    {9593, 23898, 30654, 17203},
    {6081, 0, 0, 0},
    {6086, 0, 0, 0},
    {6089, 0, 0, 0},
    {6103, 0, 0, 0},
    {9673, 0, 0, 0},
    {9675, 0, 0, 0},
}};

bool CanTakeTamingLesson(PlayerbotAI* botAI)
{
    return TameBeastAction::CanAcquirePet(botAI) &&
           !(botAI->GetBot()->HasSpell(SPELL_TAME_BEAST) && botAI->GetBot()->HasSpell(SPELL_FEED_PET) &&
             botAI->GetBot()->HasSpell(SPELL_REVIVE_PET));
}

HunterTamingLesson const* CurrentTamingLesson(PlayerbotAI* botAI)
{
    if (!CanTakeTamingLesson(botAI))
        return nullptr;

    Player* bot = botAI->GetBot();
    for (auto const& lesson : kTamingLessons)
        if (!sPlayerbotAIConfig.IgnoredQuests.contains(lesson.Quest) &&
            (bot->GetQuestStatus(lesson.Quest) == QUEST_STATUS_INCOMPLETE ||
             bot->GetQuestStatus(lesson.Quest) == QUEST_STATUS_COMPLETE))
            return &lesson;
    for (auto const& lesson : kTamingLessons)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(lesson.Quest);
        if (quest && !sPlayerbotAIConfig.IgnoredQuests.contains(lesson.Quest) && bot->CanTakeQuest(quest, false) &&
            bot->CanAddQuest(quest, false))
            return &lesson;
    }
    return nullptr;
}
}  // namespace

HunterTamingLesson const* HunterTamingLessonValue::Calculate() { return CurrentTamingLesson(botAI); }

bool HunterTamingLessonTrigger::IsActive()
{
    return AI_VALUE(HunterTamingLesson const*, "hunter taming lesson") != nullptr;
}

bool HunterTamingLessonAction::isUseful()
{
    return AI_VALUE(HunterTamingLesson const*, "hunter taming lesson") != nullptr;
}

bool HunterTamingLessonAction::MoveToLessonLocation(HunterTamingLesson const& lesson, bool questGiver, bool reward)
{
    uint32 const now = getMSTime();
    if (_quest != lesson.Quest || _questGiver != questGiver || _reward != reward || _map != bot->GetMapId() ||
        _phase != bot->GetPhaseMask() || GetMSTimeDiffToNow(_lastSpawnSearch) >= SPAWN_SEARCH_INTERVAL)
    {
        _quest = lesson.Quest;
        _questGiver = questGiver;
        _reward = reward;
        _map = bot->GetMapId();
        _phase = bot->GetPhaseMask();
        _lastSpawnSearch = now;
        _spawnIndex = 0;
        _spawns.clear();
        WorldPosition origin(bot);
        std::set<uint32> entries;
        if (questGiver)
        {
            QuestRelations const* relations =
                reward ? sObjectMgr->GetCreatureQuestInvolvedRelationMap() : sObjectMgr->GetCreatureQuestRelationMap();
            for (auto const& [entry, quest] : *relations)
                if (quest == lesson.Quest)
                    entries.insert(entry);
        }
        else
            entries.insert(lesson.Creature);
        for (uint32 entry : entries)
            for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, entry))
                if (spawn->phaseMask & bot->GetPhaseMask())
                    _spawns.emplace_back(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
        std::sort(_spawns.begin(), _spawns.end(), [&origin](WorldPosition const& a, WorldPosition const& b)
                  { return origin.sqDistance(a) < origin.sqDistance(b); });
    }

    while (_spawnIndex < _spawns.size())
    {
        WorldPosition const& destination = _spawns[_spawnIndex];
        // A loaded area without the required live beast may have a dead or phased spawn.
        if (bot->GetDistance(destination) < sPlayerbotAIConfig.SightDistance)
        {
            ++_spawnIndex;
            continue;
        }
        return MoveFarTo(destination);
    }
    return false;
}

bool HunterTamingLessonAction::InteractWithLessonGiver(HunterTamingLesson const& lesson)
{
    Quest const* quest = sObjectMgr->GetQuestTemplate(lesson.Quest);
    if (!quest)
        return false;
    bool const reward = bot->GetQuestStatus(lesson.Quest) == QUEST_STATUS_COMPLETE;
    if (!reward && (!bot->CanTakeQuest(quest, false) || !bot->CanAddQuest(quest, false)))
        return false;

    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsInWorld() || creature->IsDuringRemoveFromWorld() || !creature->IsAlive() ||
            !bot->IsInMap(creature) || !bot->InSamePhase(creature) || !bot->CanSeeOrDetect(creature) ||
            !bot->IsWithinLOSInMap(creature) ||
            !(reward ? creature->hasInvolvedQuest(lesson.Quest) : creature->hasQuest(lesson.Quest)))
            continue;
        if (!bot->IsWithinDistInMap(creature, INTERACTION_DISTANCE))
        {
            PathGenerator path(bot);
            if (!path.CalculatePath(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(),
                                    false) ||
                path.GetPathType() != PATHFIND_NORMAL)
                continue;
            return MoveTo(creature, INTERACTION_DISTANCE * 0.8f);
        }
        if (!bot->CanInteractWithQuestGiver(creature))
            continue;

        bot->StopMoving();
        if (reward)
        {
            if (!bot->CanRewardQuest(quest, false))
                return false;
            TurnInQuest(quest, creature->GetGUID());
            return bot->GetQuestRewardStatus(lesson.Quest);
        }
        AcceptQuest(quest, creature->GetGUID());
        return bot->GetQuestStatus(lesson.Quest) != QUEST_STATUS_NONE;
    }
    return MoveToLessonLocation(lesson, true, reward);
}

bool HunterTamingLessonAction::Execute(Event /*event*/)
{
    HunterTamingLesson const* lesson = CurrentTamingLesson(botAI);
    if (!lesson)
        return false;
    if (bot->GetQuestStatus(lesson->Quest) == QUEST_STATUS_NONE ||
        bot->GetQuestStatus(lesson->Quest) == QUEST_STATUS_COMPLETE)
        return InteractWithLessonGiver(*lesson);

    Item* rod = bot->GetItemByEntry(lesson->Item);
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(lesson->Spell);
    if (!rod || rod->GetTemplate()->Spells[0].SpellId != int32(lesson->Spell) || !spell || !spell->IsQuestTame())
        return false;

    Creature* target = nullptr;
    float distance = sPlayerbotAIConfig.SightDistance;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != lesson->Creature || !creature->IsInWorld() ||
            creature->IsDuringRemoveFromWorld() || !creature->IsAlive() || !bot->IsInMap(creature) ||
            !bot->InSamePhase(creature) || creature->IsPet() || creature->IsSummon() || creature->GetOwnerGUID() ||
            creature->GetCharmerGUID() || creature->IsInCombat() || creature->hasLootRecipient() ||
            !bot->IsValidAttackTarget(creature) || !bot->CanSeeOrDetect(creature) || !bot->IsWithinLOSInMap(creature))
            continue;
        float const candidateDistance = bot->GetDistance(creature);
        if (candidateDistance < distance)
        {
            distance = candidateDistance;
            target = creature;
        }
    }
    if (!target)
        return MoveToLessonLocation(*lesson, false, false);

    float const range = spell->GetMaxRange(false, bot);
    if (!bot->IsWithinDistInMap(target, range))
    {
        PathGenerator path(bot);
        if (!path.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false) ||
            path.GetPathType() != PATHFIND_NORMAL)
            return false;
        return MoveTo(target, range * 0.8f);
    }

    bot->StopMoving();
    if (!UseItemAction(botAI).UseItemOnUnit(rod, target))
        return false;
    botAI->WaitForSpellCast(bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL));
    return true;
}
