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
#include "NewRpgHelpers.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "QuestPackets.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "UseItemAction.h"

namespace
{
using Use = ClassQuestUse;
constexpr uint32 TheAffray = 1719;
constexpr float WaitDistance = 3.0f;
constexpr uint32 StalledEventMs = 5 * MINUTE * IN_MILLISECONDS;

// Lesson steps stay together and come first. The core owns race, level, prerequisite and era eligibility and all
// rewards. The other class quests are those an ordinary solo character can finish with native interactions on the
// open continents; group, dungeon, raid, crafting and repeatable class quests remain player content.
ClassQuestStep const ClassQuestSteps[] = {
    // Warrior Defensive Stance: Barrens, Tirisfal, Stormwind, Ironforge, Darnassus and Azuremyst routes.
    {CLASS_WARRIOR, 1505, 71},
    {CLASS_WARRIOR, 1498, 71},
    {CLASS_WARRIOR, 1818, 71},
    {CLASS_WARRIOR, 1819, 71, Use::Object, 0, 104593},
    {CLASS_WARRIOR, 1638, 71},
    {CLASS_WARRIOR, 1639, 71},
    {CLASS_WARRIOR, 1640, 71, Use::Stay, 0, 6090},
    {CLASS_WARRIOR, 1665, 71},
    {CLASS_WARRIOR, 1679, 71},
    {CLASS_WARRIOR, 1678, 71},
    {CLASS_WARRIOR, 1684, 71},
    {CLASS_WARRIOR, 1683, 71},
    {CLASS_WARRIOR, 9582, 71, Use::Object, 23925, 181849},
    // Warrior Berserker Stance and Intercept.
    {CLASS_WARRIOR, 1718, 2458},
    {CLASS_WARRIOR, TheAffray, 2458},
    // Warrior class quests without a lesson, nearest native giver first.
    {CLASS_WARRIOR, 2383, 0},
    {CLASS_WARRIOR, 3065, 0},
    {CLASS_WARRIOR, 3091, 0},
    {CLASS_WARRIOR, 3095, 0},
    {CLASS_WARRIOR, 3100, 0},
    {CLASS_WARRIOR, 3106, 0},
    {CLASS_WARRIOR, 3112, 0},
    {CLASS_WARRIOR, 3116, 0},
    {CLASS_WARRIOR, 9289, 0},
    {CLASS_WARRIOR, 1502, 0},
    {CLASS_WARRIOR, 1503, 0},
    {CLASS_WARRIOR, 1666, 0},
    {CLASS_WARRIOR, 1667, 0},
    {CLASS_WARRIOR, 1680, 0},
    {CLASS_WARRIOR, 1681, 0},
    {CLASS_WARRIOR, 1682, 0},
    {CLASS_WARRIOR, 1686, 0},
    {CLASS_WARRIOR, 1692, 0},
    {CLASS_WARRIOR, 1693, 0},
    {CLASS_WARRIOR, 1820, 0},
    {CLASS_WARRIOR, 1821, 0},
    {CLASS_WARRIOR, 1822, 0},
    {CLASS_WARRIOR, 10350, 0},
    {CLASS_WARRIOR, 1698, 0},
    {CLASS_WARRIOR, 1699, 0, Use::Area, 0, 482},
    {CLASS_WARRIOR, 1702, 0},
    {CLASS_WARRIOR, 1823, 0},
    {CLASS_WARRIOR, 1824, 0},
    {CLASS_WARRIOR, 10371, 0},
    // Paladin Redemption: human, dwarf, draenei and blood elf chains.
    {CLASS_PALADIN, 2997, 7328},
    {CLASS_PALADIN, 2998, 7328},
    {CLASS_PALADIN, 2999, 7328},
    {CLASS_PALADIN, 3000, 7328},
    {CLASS_PALADIN, 3681, 7328},
    {CLASS_PALADIN, 1643, 7328},
    {CLASS_PALADIN, 1644, 7328},
    {CLASS_PALADIN, 1780, 7328},
    {CLASS_PALADIN, 1781, 7328},
    {CLASS_PALADIN, 1786, 7328, Use::Creature, 6866, 6172},
    {CLASS_PALADIN, 1787, 7328},
    {CLASS_PALADIN, 1788, 7328},
    {CLASS_PALADIN, 1647, 7328},
    {CLASS_PALADIN, 1648, 7328},
    {CLASS_PALADIN, 1778, 7328},
    {CLASS_PALADIN, 1779, 7328},
    {CLASS_PALADIN, 1783, 7328, Use::Creature, 6866, 6177},
    {CLASS_PALADIN, 1784, 7328},
    {CLASS_PALADIN, 1785, 7328},
    {CLASS_PALADIN, 10366, 7328},
    {CLASS_PALADIN, 9598, 7328},
    {CLASS_PALADIN, 9600, 7328, Use::Creature, 6866, 17542},
    {CLASS_PALADIN, 9677, 7328},
    {CLASS_PALADIN, 9678, 7328, Use::Object, 0, 181956},
    {CLASS_PALADIN, 9681, 7328},
    {CLASS_PALADIN, 9684, 7328, Use::Creature, 24157, 17845},
    {CLASS_PALADIN, 9685, 7328, Use::Creature, 24184, 17768},
    // Paladin Sense Undead: Daphne Stilwell's defense, or the blood elf Second Trial.
    {CLASS_PALADIN, 1793, 5502},
    {CLASS_PALADIN, 1794, 5502},
    {CLASS_PALADIN, 1649, 5502},
    {CLASS_PALADIN, 1650, 5502},
    {CLASS_PALADIN, 1651, 5502, Use::Stay, 0, 6182},
    {CLASS_PALADIN, 1652, 5502},
    {CLASS_PALADIN, 9690, 5502},
    {CLASS_PALADIN, 9686, 5502, Use::Object, 0, 182052},
    {CLASS_PALADIN, 9691, 5502},
    // Paladin Warhorse and Thalassian Warhorse.
    {CLASS_PALADIN, 4485, 13819},
    {CLASS_PALADIN, 4486, 13819},
    {CLASS_PALADIN, 1661, 13819},
    {CLASS_PALADIN, 9712, 34769},
    // Paladin class quests without a lesson, nearest native giver first.
    {CLASS_PALADIN, 3101, 0},
    {CLASS_PALADIN, 3107, 0},
    {CLASS_PALADIN, 9287, 0},
    {CLASS_PALADIN, 9676, 0},
    {CLASS_PALADIN, 10069, 0},
    // Hunter class quests without a lesson, nearest native giver first.
    {CLASS_HUNTER, 3082, 0},
    {CLASS_HUNTER, 3087, 0},
    {CLASS_HUNTER, 3092, 0},
    {CLASS_HUNTER, 3108, 0},
    {CLASS_HUNTER, 3117, 0},
    {CLASS_HUNTER, 9288, 0},
    {CLASS_HUNTER, 9393, 0},
    {CLASS_HUNTER, 10070, 0},
    {CLASS_HUNTER, 6065, 0},
    {CLASS_HUNTER, 6066, 0},
    {CLASS_HUNTER, 6067, 0},
    {CLASS_HUNTER, 6068, 0},
    {CLASS_HUNTER, 6069, 0},
    {CLASS_HUNTER, 6070, 0},
    {CLASS_HUNTER, 6071, 0},
    {CLASS_HUNTER, 6072, 0},
    {CLASS_HUNTER, 6073, 0},
    {CLASS_HUNTER, 6074, 0},
    {CLASS_HUNTER, 6075, 0},
    {CLASS_HUNTER, 6076, 0},
    {CLASS_HUNTER, 6721, 0},
    {CLASS_HUNTER, 6722, 0},
    {CLASS_HUNTER, 9617, 0},
    {CLASS_HUNTER, 9757, 0},
    {CLASS_HUNTER, 7633, 0},
    // Rogue class quests without a lesson, nearest native giver first.
    {CLASS_ROGUE, 3083, 0},
    {CLASS_ROGUE, 3088, 0},
    {CLASS_ROGUE, 3096, 0},
    {CLASS_ROGUE, 3102, 0},
    {CLASS_ROGUE, 3109, 0},
    {CLASS_ROGUE, 3113, 0},
    {CLASS_ROGUE, 3118, 0},
    {CLASS_ROGUE, 9392, 0},
    {CLASS_ROGUE, 10071, 0},
    {CLASS_ROGUE, 2205, 0},
    {CLASS_ROGUE, 2206, 0},
    {CLASS_ROGUE, 2218, 0},
    {CLASS_ROGUE, 2238, 0},
    {CLASS_ROGUE, 2239, 0},
    {CLASS_ROGUE, 2241, 0},
    {CLASS_ROGUE, 1998, 0},
    {CLASS_ROGUE, 1999, 0},
    {CLASS_ROGUE, 2259, 0},
    {CLASS_ROGUE, 2260, 0},
    {CLASS_ROGUE, 2281, 0},
    {CLASS_ROGUE, 2282, 0},
    {CLASS_ROGUE, 2298, 0},
    {CLASS_ROGUE, 2299, 0},
    {CLASS_ROGUE, 2300, 0},
    {CLASS_ROGUE, 2358, 0},
    {CLASS_ROGUE, 9491, 0},
    {CLASS_ROGUE, 10372, 0},
    {CLASS_ROGUE, 10548, 0},
    {CLASS_ROGUE, 2359, 0},
    {CLASS_ROGUE, 2360, 0},
    // Priest racial lessons restored by Individual Progression.
    {CLASS_PRIEST, 5634, 948081},
    {CLASS_PRIEST, 5635, 948081},
    {CLASS_PRIEST, 5636, 948081},
    {CLASS_PRIEST, 5637, 948081},
    {CLASS_PRIEST, 5638, 948081},
    {CLASS_PRIEST, 5639, 948081},
    {CLASS_PRIEST, 5627, 949100},
    {CLASS_PRIEST, 5628, 949100},
    {CLASS_PRIEST, 5629, 949100},
    {CLASS_PRIEST, 5630, 949100},
    {CLASS_PRIEST, 5631, 949100},
    {CLASS_PRIEST, 5632, 949100},
    {CLASS_PRIEST, 5633, 949100},
    {CLASS_PRIEST, 5658, 949112},
    {CLASS_PRIEST, 5660, 949112},
    {CLASS_PRIEST, 5661, 949112},
    {CLASS_PRIEST, 5662, 949112},
    {CLASS_PRIEST, 5663, 949112},
    {CLASS_PRIEST, 5652, 949124},
    {CLASS_PRIEST, 5654, 949124},
    {CLASS_PRIEST, 5655, 949124},
    {CLASS_PRIEST, 5656, 949124},
    {CLASS_PRIEST, 5657, 949124},
    {CLASS_PRIEST, 5641, 948089},
    {CLASS_PRIEST, 5645, 948089},
    {CLASS_PRIEST, 5647, 948089},
    {CLASS_PRIEST, 5672, 949107},
    {CLASS_PRIEST, 5673, 949107},
    {CLASS_PRIEST, 5674, 949107},
    {CLASS_PRIEST, 5675, 949107},
    {CLASS_PRIEST, 5642, 949130},
    {CLASS_PRIEST, 5643, 949130},
    {CLASS_PRIEST, 5680, 949130},
    {CLASS_PRIEST, 5676, 949142},
    {CLASS_PRIEST, 5677, 949142},
    {CLASS_PRIEST, 5644, 949152},
    {CLASS_PRIEST, 5646, 949152},
    {CLASS_PRIEST, 5679, 949152},
    // Priest class quests without a lesson, nearest native giver first.
    {CLASS_PRIEST, 3085, 0},
    {CLASS_PRIEST, 3097, 0},
    {CLASS_PRIEST, 3103, 0},
    {CLASS_PRIEST, 3110, 0},
    {CLASS_PRIEST, 3119, 0},
    {CLASS_PRIEST, 8564, 0},
    {CLASS_PRIEST, 9291, 0},
    {CLASS_PRIEST, 10072, 0},
    {CLASS_PRIEST, 5621, 0, Use::Cast, 0, 12429, {2052, 1243}},
    {CLASS_PRIEST, 5622, 0},
    {CLASS_PRIEST, 5623, 0},
    {CLASS_PRIEST, 5624, 0, Use::Cast, 0, 12423, {2052, 1243}},
    {CLASS_PRIEST, 5625, 0, Use::Cast, 0, 12427, {2052, 1243}},
    {CLASS_PRIEST, 5626, 0},
    {CLASS_PRIEST, 5648, 0, Use::Cast, 0, 12430, {2052, 1243}},
    {CLASS_PRIEST, 5649, 0},
    {CLASS_PRIEST, 5650, 0, Use::Cast, 0, 12428, {2052, 1243}},
    {CLASS_PRIEST, 5651, 0},
    {CLASS_PRIEST, 9489, 0, Use::Cast, 0, 15938, {1243}},
    {CLASS_PRIEST, 9586, 0, Use::Cast, 0, 17551, {2050}},
    {CLASS_PRIEST, 7621, 0},
    // Shaman class quests without a lesson, nearest native giver first.
    {CLASS_SHAMAN, 3084, 0},
    {CLASS_SHAMAN, 3089, 0},
    {CLASS_SHAMAN, 3093, 0},
    {CLASS_SHAMAN, 9421, 0},
    {CLASS_SHAMAN, 7669, 0},
    // Mage class quests without a lesson, nearest native giver first.
    {CLASS_MAGE, 3086, 0},
    {CLASS_MAGE, 3098, 0},
    {CLASS_MAGE, 3104, 0},
    {CLASS_MAGE, 3114, 0},
    {CLASS_MAGE, 8328, 0},
    {CLASS_MAGE, 9290, 0},
    {CLASS_MAGE, 10068, 0},
    {CLASS_MAGE, 1860, 0},
    {CLASS_MAGE, 1861, 0, Use::Focus, 7207, 0},
    {CLASS_MAGE, 1879, 0},
    {CLASS_MAGE, 1880, 0},
    {CLASS_MAGE, 1881, 0},
    {CLASS_MAGE, 1882, 0},
    {CLASS_MAGE, 1883, 0},
    {CLASS_MAGE, 1884, 0, Use::Object, 0, 102986},
    {CLASS_MAGE, 9402, 0},
    {CLASS_MAGE, 9403, 0, Use::Focus, 23566, 0},
    {CLASS_MAGE, 9404, 0},
    {CLASS_MAGE, 9487, 0},
    {CLASS_MAGE, 9488, 0},
    {CLASS_MAGE, 1938, 0},
    {CLASS_MAGE, 1939, 0},
    {CLASS_MAGE, 1940, 0},
    {CLASS_MAGE, 1942, 0},
    {CLASS_MAGE, 1943, 0},
    {CLASS_MAGE, 1944, 0, Use::Focus, 7269, 0},
    {CLASS_MAGE, 1945, 0},
    {CLASS_MAGE, 1946, 0},
    {CLASS_MAGE, 8250, 0},
    // Warlock Imp: the restored Vanilla chains of each starting zone.
    {CLASS_WARLOCK, 1470, 688},
    {CLASS_WARLOCK, 1485, 688},
    {CLASS_WARLOCK, 1598, 688},
    {CLASS_WARLOCK, 1599, 688},
    {CLASS_WARLOCK, 8344, 688},
    // Warlock Voidwalker; the ritual items summon the Voidwalker at their native focus.
    {CLASS_WARLOCK, 1471, 697, Use::Focus, 6284, 5676},
    {CLASS_WARLOCK, 1473, 697},
    {CLASS_WARLOCK, 1478, 697},
    {CLASS_WARLOCK, 1501, 697},
    {CLASS_WARLOCK, 1504, 697, Use::Focus, 7464, 5676},
    {CLASS_WARLOCK, 1506, 697},
    {CLASS_WARLOCK, 1685, 697},
    {CLASS_WARLOCK, 1688, 697},
    {CLASS_WARLOCK, 1689, 697, Use::Focus, 6928, 5676},
    {CLASS_WARLOCK, 1715, 697},
    {CLASS_WARLOCK, 10789, 697},
    {CLASS_WARLOCK, 10790, 697},
    {CLASS_WARLOCK, 9529, 697},
    {CLASS_WARLOCK, 9619, 697, Use::Focus, 23732, 5676},
    // Warlock Succubus.
    {CLASS_WARLOCK, 1472, 712},
    {CLASS_WARLOCK, 1474, 712, Use::Focus, 6286, 5677},
    {CLASS_WARLOCK, 1476, 712},
    {CLASS_WARLOCK, 1507, 712},
    {CLASS_WARLOCK, 1508, 712},
    {CLASS_WARLOCK, 1509, 712},
    {CLASS_WARLOCK, 1510, 712},
    {CLASS_WARLOCK, 1511, 712},
    {CLASS_WARLOCK, 1512, 712},
    {CLASS_WARLOCK, 1513, 712, Use::Focus, 6626, 5677},
    {CLASS_WARLOCK, 1515, 712},
    {CLASS_WARLOCK, 1716, 712},
    {CLASS_WARLOCK, 1717, 712},
    {CLASS_WARLOCK, 1738, 712},
    {CLASS_WARLOCK, 1739, 712, Use::Focus, 6913, 5677},
    {CLASS_WARLOCK, 10605, 712},
    // Warlock Felhunter; Strahad Farsan's escort creates the focus of The Summoning.
    {CLASS_WARLOCK, 1758, 691},
    {CLASS_WARLOCK, 1795, 691, Use::Focus, 6999, 6268},
    {CLASS_WARLOCK, 1798, 691},
    {CLASS_WARLOCK, 1801, 691},
    {CLASS_WARLOCK, 1802, 691},
    {CLASS_WARLOCK, 1803, 691},
    {CLASS_WARLOCK, 1804, 691},
    {CLASS_WARLOCK, 1805, 691},
    {CLASS_WARLOCK, 2996, 691},
    {CLASS_WARLOCK, 3001, 691},
    // Warlock Felsteed, restored by Individual Progression.
    {CLASS_WARLOCK, 3631, 5784},
    {CLASS_WARLOCK, 4487, 5784},
    {CLASS_WARLOCK, 4488, 5784},
    {CLASS_WARLOCK, 4489, 5784},
    {CLASS_WARLOCK, 4490, 5784},
    // Warlock class quests without a lesson, nearest native giver first.
    {CLASS_WARLOCK, 1499, 0},
    {CLASS_WARLOCK, 3090, 0},
    {CLASS_WARLOCK, 3099, 0},
    {CLASS_WARLOCK, 3105, 0},
    {CLASS_WARLOCK, 3115, 0},
    {CLASS_WARLOCK, 8563, 0},
    {CLASS_WARLOCK, 10073, 0},
    {CLASS_WARLOCK, 10788, 0},
    {CLASS_WARLOCK, 397, 0},
    // Druid class quests without a lesson, nearest native giver first.
    {CLASS_DRUID, 3094, 0},
    {CLASS_DRUID, 3120, 0},
    {CLASS_DRUID, 9052, 0},
    {CLASS_DRUID, 9063, 0},
};

// Twiggy Flathead's native script starts the Affray for a warrior within ten yards of him and on this grate.
WorldPosition const AffrayGrate(1, -1681.0f, -4327.0f, 2.79f);

// Reuse the ordinary attack and object-use paths with their native eligibility checks.
struct ClassQuestAttack : AttackAction
{
    using AttackAction::Attack;
    using AttackAction::AttackAction;
};

struct ClassQuestUseItem : UseItemAction
{
    using UseItemAction::UseGameObject;
    using UseItemAction::UseItemAction;
};

bool ClassQuestEarned(Player* bot, uint32 spell)
{
    if (!spell)
        return false;
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

void WaitForCast(PlayerbotAI* botAI, Player* bot)
{
    Spell* cast = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!cast)
        cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (cast)
        botAI->WaitForSpellCast(cast);
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
    uint32 lesson = std::numeric_limits<uint32>::max();
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
        // Lessons are always worth earning; other class quests follow the ordinary quest level rules.
        if (!step.Spell &&
            (!NewRpgHelpers::IsQuestWorthDoing(bot, quest) || !NewRpgHelpers::IsQuestCapableDoing(bot, quest)))
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
    // A failed trial, escort or timed quest is abandoned as a player would, so its giver offers it again.
    if (status == QUEST_STATUS_FAILED)
        return Abandon(step->Quest);
    if (status == QUEST_STATUS_NONE)
    {
        if (Item* item = QuestStartItem(botAI, step->Quest))
            return AcceptQuest(quest, item->GetGUID()) && bot->GetQuestStatus(step->Quest) != QUEST_STATUS_NONE;
        return InteractWithQuestGiver(quest, false) || MoveToQuestLocation(quest, false);
    }
    if (status == QUEST_STATUS_COMPLETE)
        return InteractWithQuestGiver(quest, true) || (step->Use == Use::Creature && UseOnCreature(quest, *step)) ||
               MoveToQuestLocation(quest, true);
    if (status != QUEST_STATUS_INCOMPLETE)
        return false;
    switch (step->Use)
    {
        case Use::Creature:
            return UseOnCreature(quest, *step);
        case Use::Object:
            return UseObject(quest, *step);
        case Use::Stay:
            return StayWith(quest, *step);
        case Use::Focus:
            return UseAtFocus(quest, *step);
        case Use::Cast:
            return CastOn(quest, *step);
        case Use::Area:
            return ReachArea(quest, *step);
        case Use::None:
            break;
    }
    return step->Quest == TheAffray ? Wait(quest, AffrayGrate) : DoObjectives(quest);
}

Creature* ClassQuestChainAction::QuestTarget(Quest const* quest, ClassQuestStep const& step, bool revive)
{
    Creature* target = nullptr;
    float distance = std::numeric_limits<float>::max();
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != step.Entry || !creature->IsAlive() ||
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
    return target;
}

bool ClassQuestChainAction::UseOnCreature(Quest const* quest, ClassQuestStep const& step)
{
    Item* item = bot->GetItemByEntry(step.Item);
    bool const revive = bot->GetQuestStatus(quest->GetQuestId()) == QUEST_STATUS_COMPLETE;
    if (!item)
        return !revive && Restart(quest, step.Item);
    Creature* target = QuestTarget(quest, step, revive);
    if (!target)
    {
        if (revive)
            return false;
        WorldPosition const pos = SpawnPosition(step.Entry, false);
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
    WaitForCast(botAI, bot);
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
        if (!object || object->GetEntry() != step.Entry || !object->isSpawned() || !Usable(bot, object))
            continue;
        if (!object->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(object))
            return MoveToQuestPosition(WorldPosition(object));
        bot->StopMoving();
        ClassQuestUseItem use(botAI);
        if (!(item ? use.UseItemOnGameObject(item, guid) : use.UseGameObject(guid)))
            return false;
        _usedQuest = quest->GetQuestId();
        _usedAt = getMSTime();
        _usedPosition = WorldPosition(object);
        if (Spell* cast = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            botAI->WaitForSpellCast(cast);
        return true;
    }
    return Wait(quest, SpawnPosition(step.Entry, true));
}

bool ClassQuestChainAction::StayWith(Quest const* quest, ClassQuestStep const& step)
{
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || creature->GetEntry() != step.Entry || !creature->IsAlive() || !Usable(bot, creature))
            continue;
        // Daphne Stilwell's raiders attack her, not the escorting paladin.
        for (Unit* attacker : creature->getAttackers())
            if (attacker && Usable(bot, attacker) && ClassQuestAttack(botAI, "class quest attack").Attack(attacker))
                return true;
        return Wait(quest, WorldPosition(creature));
    }
    return Wait(quest, SpawnPosition(step.Entry, false));
}

bool ClassQuestChainAction::UseAtFocus(Quest const* quest, ClassQuestStep const& step)
{
    if (step.Entry)
    {
        // Stay on the ritual instead of grinding elsewhere; the summoned Entry is fought as its quest target.
        auto* state = std::get_if<NewRpgInfo::DoQuest>(&botAI->rpgInfo.data);
        if (!state || state->questId != quest->GetQuestId())
            botAI->rpgInfo.ChangeToDoQuest(quest->GetQuestId(), quest);
        if (bot->GetReqKillOrCastCurrentCount(quest->GetQuestId(), step.Entry))
            return false;
        // Native AI turns the own summon hostile after a short delay; the ritual is not repeated meanwhile.
        for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
        {
            Unit* unit = botAI->GetUnit(guid);
            Creature* creature = unit ? unit->ToCreature() : nullptr;
            if (creature && creature->GetEntry() == step.Entry && creature->IsAlive() && Usable(bot, creature) &&
                creature->ToTempSummon() && creature->ToTempSummon()->GetSummonerGUID() == bot->GetGUID())
                return true;
        }
    }
    Item* item = bot->GetItemByEntry(step.Item);
    if (!item)
        return Restart(quest, step.Item);
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(item->GetTemplate()->Spells[0].SpellId);
    if (!spell || !spell->RequiresSpellFocus)
        return false;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest game objects no los"))
    {
        GameObject* focus = botAI->GetGameObject(guid);
        if (!focus || !focus->isSpawned() || !Usable(bot, focus) || focus->GetGoType() != GAMEOBJECT_TYPE_SPELL_FOCUS ||
            focus->GetGOInfo()->spellFocus.focusId != spell->RequiresSpellFocus)
            continue;
        if (!focus->IsWithinDistInMap(bot, focus->GetGOInfo()->spellFocus.dist))
            return MoveToQuestPosition(WorldPosition(focus));
        bot->StopMoving();
        if (!UseItemAction(botAI).UseItemOnUnit(item, bot))
            return false;
        WaitForCast(botAI, bot);
        return true;
    }
    WorldPosition const pos = FocusPosition(spell->RequiresSpellFocus);
    if (pos != WorldPosition() && (pos.GetMapId() != bot->GetMapId() || bot->GetExactDist(pos) > INTERACTION_DISTANCE))
        return MoveToQuestPosition(pos);
    if (!step.Entry)
        return false;
    // Strahad Farsan's native escort creates the Felhunter focus; stay with the quest giver while it runs.
    for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* giver = unit ? unit->ToCreature() : nullptr;
        if (giver && giver->hasQuest(quest->GetQuestId()) && giver->IsAlive() && Usable(bot, giver))
            return !giver->HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER) && Wait(quest, WorldPosition(giver));
    }
    return MoveToQuestLocation(quest, false);
}

bool ClassQuestChainAction::CastOn(Quest const* quest, ClassQuestStep const& step)
{
    Creature* target = nullptr;
    // Finish the native spell sequence on the creature already started.
    if (_castIndex)
        for (ObjectGuid guid : AI_VALUE(GuidVector, "nearest npcs"))
        {
            Unit* unit = botAI->GetUnit(guid);
            Creature* creature = unit ? unit->ToCreature() : nullptr;
            if (creature && creature->GetSpawnId() == _castTarget && creature->GetEntry() == step.Entry &&
                creature->IsAlive() && Usable(bot, creature))
                target = creature;
        }
    if (!target)
    {
        _castIndex = 0;
        target = QuestTarget(quest, step, false);
    }
    if (!target)
    {
        WorldPosition const pos = SpawnPosition(step.Entry, false);
        return pos != WorldPosition() &&
               (pos.GetMapId() != bot->GetMapId() || bot->GetExactDist(pos) > INTERACTION_DISTANCE) &&
               MoveToQuestPosition(pos);
    }
    uint32 const spell = step.Casts[_castIndex];
    if (!spell || !bot->HasSpell(spell))
        return false;
    if (!target->IsWithinDistInMap(bot, INTERACTION_DISTANCE) || !bot->IsWithinLOSInMap(target))
        return MoveToQuestPosition(WorldPosition(target));
    bot->StopMoving();
    if (!botAI->CastSpell(spell, target))
        return false;
    _castTarget = target->GetSpawnId();
    if (++_castIndex == step.Casts.size() || !step.Casts[_castIndex])
    {
        _usedTargets[_castTarget] = getMSTime();
        _castIndex = 0;
    }
    WaitForCast(botAI, bot);
    return true;
}

bool ClassQuestChainAction::ReachArea(Quest const* quest, ClassQuestStep const& step)
{
    AreaTrigger const* trigger = sObjectMgr->GetAreaTrigger(step.Entry);
    if (!trigger || sObjectMgr->GetQuestForAreaTrigger(step.Entry) != quest->GetQuestId())
        return false;
    if (bot->GetMapId() != trigger->map || !bot->IsInAreaTriggerRadius(trigger))
        return MoveToQuestPosition(WorldPosition(trigger->map, trigger->x, trigger->y, trigger->z));
    // Players report the trigger they enter; the native handler validates the position and quest.
    WorldPacket packet(CMSG_AREATRIGGER);
    packet << trigger->entry;
    packet.rpos(0);
    bot->GetSession()->HandleAreaTriggerOpcode(packet);
    return bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_INCOMPLETE;
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

WorldPosition ClassQuestChainAction::SpawnPosition(uint32 entry, bool object)
{
    if (_spawnEntry == entry && _spawnObject == object && _spawnMap == bot->GetMapId() && _spawnChecked &&
        GetMSTimeDiffToNow(_spawnChecked) < MINUTE * IN_MILLISECONDS)
        return _spawnPosition;
    _spawnEntry = entry;
    _spawnObject = object;
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
    if (object)
        for (GameObjectData const* spawn : origin.getGameObjectsNear(0.0f, entry))
            consider(spawn);
    else
        for (CreatureData const* spawn : origin.getCreaturesNear(0.0f, entry))
            consider(spawn);
    return _spawnPosition;
}

WorldPosition ClassQuestChainAction::FocusPosition(uint32 focusId)
{
    if (_focusId == focusId && _focusMap == bot->GetMapId() && _focusChecked &&
        GetMSTimeDiffToNow(_focusChecked) < MINUTE * IN_MILLISECONDS)
        return _focusPosition;
    _focusId = focusId;
    _focusMap = bot->GetMapId();
    _focusChecked = getMSTime();
    _focusPosition = WorldPosition();
    ClassQuestRouteCosts const costs(botAI);
    WorldPosition origin;
    float nearest = std::numeric_limits<float>::max();
    for (auto const& entry : *sObjectMgr->GetGameObjectTemplates())
        if (entry.second.type == GAMEOBJECT_TYPE_SPELL_FOCUS && entry.second.spellFocus.focusId == focusId)
            for (GameObjectData const* spawn : origin.getGameObjectsNear(0.0f, entry.first))
            {
                if (!(spawn->phaseMask & bot->GetPhaseMask()))
                    continue;
                WorldPosition const pos(spawn->mapid, spawn->posX, spawn->posY, spawn->posZ);
                if (costs.Cost(pos) < nearest)
                {
                    _focusPosition = pos;
                    nearest = costs.Cost(pos);
                }
            }
    return _focusPosition;
}
