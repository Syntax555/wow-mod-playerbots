/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AiFactory.h"
#include "BattlegroundMgr.h"
#include "DKAiObjectContext.h"
#include "DruidAiObjectContext.h"
#include "Engine.h"
#include "EraTalentBridge.h"
#include "Group.h"
#include "HunterAiObjectContext.h"
#include "Item.h"
#include "MageAiObjectContext.h"
#include "PaladinAiObjectContext.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotRepository.h"
#include "Playerbots.h"
#include "PriestAiObjectContext.h"
#include "ReactionEngine.h"
#include "RogueAiObjectContext.h"
#include "ShamanAiObjectContext.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WarlockAiObjectContext.h"
#include "WarriorAiObjectContext.h"
#include <algorithm>

namespace
{
constexpr uint32 SPELL_FROSTFIRE_BOLT = 44614;
constexpr uint32 SPELL_ICE_SHARDS = 15047;
constexpr uint32 SPELL_WHIRLWIND = 1680;
constexpr uint32 SPELL_CAT_FORM = 768;
constexpr uint32 SPELL_DRUID_THICK_HIDE = 16931;
}

AiObjectContext* AiFactory::createAiObjectContext(Player* player, PlayerbotAI* botAI)
{
    switch (player->getClass())
    {
        case CLASS_PRIEST:
            return new PriestAiObjectContext(botAI);
        case CLASS_MAGE:
            return new MageAiObjectContext(botAI);
        case CLASS_WARLOCK:
            return new WarlockAiObjectContext(botAI);
        case CLASS_WARRIOR:
            return new WarriorAiObjectContext(botAI);
        case CLASS_SHAMAN:
            return new ShamanAiObjectContext(botAI);
        case CLASS_PALADIN:
            return new PaladinAiObjectContext(botAI);
        case CLASS_DRUID:
            return new DruidAiObjectContext(botAI);
        case CLASS_HUNTER:
            return new HunterAiObjectContext(botAI);
        case CLASS_ROGUE:
            return new RogueAiObjectContext(botAI);
        case CLASS_DEATH_KNIGHT:
            return new DKAiObjectContext(botAI);
    }

    return new AiObjectContext(botAI);
}

std::vector<std::string> AiFactory::GetCombatProfiles(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR: return {"auto", "arms", "fury", "prot"};
        case CLASS_PALADIN: return {"auto", "holy", "prot", "ret"};
        case CLASS_HUNTER: return {"auto", "bm", "mm", "surv"};
        case CLASS_ROGUE: return {"auto", "assassin", "combat", "subtlety"};
        case CLASS_PRIEST: return {"auto", "disc", "holy", "shadow"};
        case CLASS_DEATH_KNIGHT: return {"auto", "blood", "frost", "unholy"};
        case CLASS_SHAMAN: return {"auto", "ele", "enh", "resto"};
        case CLASS_MAGE: return {"auto", "arcane", "fire", "frost"};
        case CLASS_WARLOCK: return {"auto", "affli", "demo", "destro"};
        case CLASS_DRUID: return {"auto", "balance", "cat", "bear", "resto"};
        default: return {"auto"};
    }
}

std::string AiFactory::GetCombatProfile(Player* player)
{
    PlayerbotAI* ai = player ? GET_PLAYERBOT_AI(player) : nullptr;
    if (!ai || !ai->GetAiObjectContext())
        return "auto";
    std::string mode = ai->GetAiObjectContext()->GetValue<std::string>("combat profile")->Get();
    auto profiles = GetCombatProfiles(player->getClass());
    return std::find(profiles.begin(), profiles.end(), mode) != profiles.end() ? mode : "auto";
}

bool AiFactory::SetCombatProfile(Player* player, std::string const& mode)
{
    PlayerbotAI* ai = player ? GET_PLAYERBOT_AI(player) : nullptr;
    if (!ai || !ai->GetAiObjectContext() || !player->IsInWorld() || !player->IsAlive() ||
        player->IsInCombat() || player->IsBeingTeleported())
        return false;
    auto profiles = GetCombatProfiles(player->getClass());
    if (std::find(profiles.begin(), profiles.end(), mode) == profiles.end())
        return false;
    auto value = ai->GetAiObjectContext()->GetValue<std::string>("combat profile");
    bool changed = value->Get() != mode;
    value->Set(mode);
    ai->InvalidateCombatProfile();
    ai->RefreshCombatProfile(true);
    if (changed)
        PlayerbotRepository::instance().Save(ai);
    return true;
}

bool AiFactory::IsFeralTank(Player* player)
{
    std::string mode = GetCombatProfile(player);
    if (mode == "cat")
        return false;
    if (mode == "bear")
        return true;
    int seeded = EraTalentBots_FeralSpec(player);
    if (seeded >= 0)
        return seeded == 1;
    return player->GetShapeshiftForm() == FORM_BEAR || player->GetShapeshiftForm() == FORM_DIREBEAR ||
           player->HasAura(EraKnown(player, SPELL_DRUID_THICK_HIDE));
}

std::string AiFactory::GetEffectiveCombatProfile(Player* player)
{
    if (!player)
        return "";
    std::string mode = GetCombatProfile(player);
    if (mode != "auto")
        return mode;
    uint8 tab = GetPlayerSpecTab(player);
    if (player->getClass() == CLASS_DRUID)
        return tab == DRUID_TAB_BALANCE ? "balance" : tab == DRUID_TAB_RESTORATION ? "resto" :
               IsFeralTank(player) ? "bear" : "cat";
    if (player->getClass() == CLASS_WARRIOR && tab == WARRIOR_TAB_FURY && !player->HasSpell(SPELL_WHIRLWIND))
        return "arms";
    if (player->getClass() == CLASS_MAGE && tab == MAGE_TAB_FIRE &&
        player->HasSpell(SPELL_FROSTFIRE_BOLT) && player->HasAura(SPELL_ICE_SHARDS))
        return "frostfire";
    auto profiles = GetCombatProfiles(player->getClass());
    return tab + 1u < profiles.size() ? profiles[tab + 1] : "";
}

uint8 AiFactory::GetPlayerSpecTab(Player* bot)
{
    std::string mode = GetCombatProfile(bot);
    if (mode != "auto")
    {
        if (bot->getClass() == CLASS_DRUID)
            return mode == "balance" ? DRUID_TAB_BALANCE : mode == "resto" ? DRUID_TAB_RESTORATION : DRUID_TAB_FERAL;
        auto profiles = GetCombatProfiles(bot->getClass());
        return uint8(std::find(profiles.begin(), profiles.end(), mode) - profiles.begin() - 1);
    }
    std::map<uint8, uint32> tabs = GetPlayerSpecTabs(bot);

    if (bot->GetLevel() >= 10 && ((tabs[0] + tabs[1] + tabs[2]) > 0))
    {
        int8 tab = -1;
        uint32 max = 0;
        for (uint32 i = 0; i < uint32(3); i++)
        {
            if (tab == -1 || max < tabs[i])
            {
                tab = i;
                max = tabs[i];
            }
        }
        return tab;
    }
    else
    {
        uint8 tab = 0;

        switch (bot->getClass())
        {
            case CLASS_MAGE:
                tab = MAGE_TAB_FROST;
                break;
            case CLASS_PALADIN:
                tab = PALADIN_TAB_RETRIBUTION;
                break;
            case CLASS_PRIEST:
                tab = PRIEST_TAB_HOLY;
                break;
            case CLASS_WARLOCK:
                tab = WARLOCK_TAB_DEMONOLOGY;
                break;
        }

        return tab;
    }
}

std::map<uint8, uint32> AiFactory::GetPlayerSpecTabs(Player* bot)
{
    std::map<uint8, uint32> tabs = {{0, 0}, {0, 0}, {0, 0}};

    // Custom era talents are stored separately from the native talent map.
    uint32 eraTabs[3] = {0, 0, 0};
    if (EraTalentBots_SpecTabs(bot, eraTabs))
    {
        tabs[0] = eraTabs[0];
        tabs[1] = eraTabs[1];
        tabs[2] = eraTabs[2];
        return tabs;
    }

    PlayerTalentMap const& talentMap = bot->GetTalentMap();
    for (PlayerTalentMap::const_iterator i = talentMap.begin(); i != talentMap.end(); ++i)
    {
        uint32 spellId = i->first;
        if ((bot->GetActiveSpecMask() & i->second->specMask) == 0)
        {
            continue;
        }
        TalentSpellPos const* talentPos = GetTalentSpellPos(spellId);
        if (!talentPos)
            continue;
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(talentPos->talent_id);
        if (!talentInfo)
            continue;

        uint32 const* talentTabIds = GetTalentTabPages(bot->getClass());

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        int rank = spellInfo ? spellInfo->GetRank() : 1;
        if (talentInfo->TalentTab == talentTabIds[0])
            tabs[0] += rank;
        if (talentInfo->TalentTab == talentTabIds[1])
            tabs[1] += rank;
        if (talentInfo->TalentTab == talentTabIds[2])
            tabs[2] += rank;
    }
    return tabs;
}

BotRoles AiFactory::GetPlayerRoles(Player* player)
{
    if (PlayerbotAI::IsTank(player, true))
        return BOT_ROLE_TANK;
    if (PlayerbotAI::IsHeal(player, true))
        return BOT_ROLE_HEALER;
    return BOT_ROLE_DPS;
}

std::string AiFactory::GetPlayerSpecName(Player* player)
{
    std::string specName;
    int tab = GetPlayerSpecTab(player);
    switch (player->getClass())
    {
        case CLASS_PRIEST:
            if (tab == PRIEST_TAB_SHADOW)
                specName = "shadow";
            else if (tab == PRIEST_TAB_HOLY)
                specName = "holy";
            else
                specName = "disc";
            break;
        case CLASS_SHAMAN:
            if (tab == SHAMAN_TAB_RESTORATION)
                specName = "resto";
            else if (tab == SHAMAN_TAB_ENHANCEMENT)
                specName = "enhance";
            else
                specName = "elem";
            break;
        case CLASS_WARRIOR:
            if (tab == WARRIOR_TAB_PROTECTION)
                specName = "prot";
            else if (tab == WARRIOR_TAB_FURY)
                specName = "fury";
            else
                specName = "arms";
            break;
        case CLASS_PALADIN:
            if (tab == PALADIN_TAB_HOLY)
                specName = "holy";
            else if (tab == PALADIN_TAB_PROTECTION)
                specName = "prot";
            else if (tab == PALADIN_TAB_RETRIBUTION)
                specName = "retrib";
            break;
        case CLASS_DRUID:
            if (tab == DRUID_TAB_BALANCE)
                specName = "balance";
            else if (tab == DRUID_TAB_FERAL)
                specName = "feraldps";
            else if (tab == DRUID_TAB_RESTORATION)
                specName = "resto";
            break;
        case CLASS_ROGUE:
            if (tab == ROGUE_TAB_ASSASSINATION)
                specName = "assas";
            else if (tab == ROGUE_TAB_COMBAT)
                specName = "combat";
            else if (tab == ROGUE_TAB_SUBTLETY)
                specName = "subtle";
            break;
        case CLASS_HUNTER:
            if (tab == HUNTER_TAB_BEAST_MASTERY)
                specName = "beast";
            else if (tab == HUNTER_TAB_MARKSMANSHIP)
                specName = "marks";
            else if (tab == HUNTER_TAB_SURVIVAL)
                specName = "surv";
            break;
        case CLASS_DEATH_KNIGHT:
            if (tab == DEATH_KNIGHT_TAB_BLOOD)
                specName = "blooddps";
            else if (tab == DEATH_KNIGHT_TAB_FROST)
                specName = "frostdps";
            else if (tab == DEATH_KNIGHT_TAB_UNHOLY)
                specName = "unholydps";
            break;
        case CLASS_MAGE:
            if (tab == MAGE_TAB_ARCANE)
                specName = "arcane";
            else if (tab == MAGE_TAB_FIRE)
                specName = "fire";
            else if (tab == MAGE_TAB_FROST)
                specName = "frost";
            break;
        case CLASS_WARLOCK:
            if (tab == WARLOCK_TAB_AFFLICTION)
                specName = "afflic";
            else if (tab == WARLOCK_TAB_DEMONOLOGY)
                specName = "demo";
            else if (tab == WARLOCK_TAB_DESTRUCTION)
                specName = "destro";
            break;
        default:
            break;
    }

    return specName;
}

void AiFactory::AddClassCombatStrategies(Player* player, PlayerbotAI* const /*facade*/, Engine* engine)
{
    uint8 tab = GetPlayerSpecTab(player);

    switch (player->getClass())
    {
        case CLASS_PRIEST:
            if (tab == PRIEST_TAB_SHADOW)
                engine->addStrategiesNoInit("shadow", "shadow debuff", "shadow aoe", nullptr);
            else if (tab == PRIEST_TAB_DISCIPLINE)
                engine->addStrategy("disc", false);
            else // if (tab == PRIEST_TAB_HOLY)
                engine->addStrategy("holy heal", false);
            engine->addStrategiesNoInit("dps assist", "cure", nullptr);
            break;

        case CLASS_MAGE:
            if (tab == MAGE_TAB_ARCANE)
                engine->addStrategiesNoInit("arcane", "bdps", nullptr);
            else if (tab == MAGE_TAB_FIRE)
            {
                if (GetEffectiveCombatProfile(player) == "frostfire")
                    engine->addStrategiesNoInit("frostfire", "bdps", nullptr);
                else
                    engine->addStrategiesNoInit("fire", "bdps", nullptr);
            }
            else // if (tab == MAGE_TAB_FROST)
                engine->addStrategiesNoInit("frost", "bmana", nullptr);
            engine->addStrategiesNoInit("dps assist", "cure", "cc", "aoe", nullptr);
            break;

        case CLASS_WARRIOR:
            if (tab == WARRIOR_TAB_PROTECTION)
                engine->addStrategiesNoInit("prot", "tank assist", "pull", "pull back", "aoe", nullptr);
            else if (GetEffectiveCombatProfile(player) == "arms")
                engine->addStrategiesNoInit("arms", "aoe", "dps assist", nullptr);
            else // if (tab == WARRIOR_TAB_FURY)
                engine->addStrategiesNoInit("fury", "aoe", "dps assist", nullptr);
            break;

        case CLASS_SHAMAN:
            if (tab == SHAMAN_TAB_ELEMENTAL)
                engine->addStrategiesNoInit("ele", "stoneskin", "wrath", "mana spring", "wrath of air", nullptr);
            else if (tab == SHAMAN_TAB_RESTORATION)
                engine->addStrategiesNoInit("resto", "stoneskin", "flametongue", "mana spring", "wrath of air", nullptr);
            else // if (tab == SHAMAN_TAB_ENHANCEMENT)
                engine->addStrategiesNoInit("enh", "strength of earth", "magma", "healing stream", "windfury", nullptr);
            engine->addStrategiesNoInit("dps assist", "cure", "aoe", nullptr);
            break;

        case CLASS_PALADIN:
            if (tab == PALADIN_TAB_PROTECTION)
                engine->addStrategiesNoInit("prot", "tank assist", "pull", "pull back", "bthreat", "barmor", "cure", nullptr);
            else if (tab == PALADIN_TAB_HOLY)
                engine->addStrategiesNoInit("holy", "dps assist", "cure", "bcast", nullptr);
            else // if (tab == PALADIN_TAB_RETRIBUTION)
                engine->addStrategiesNoInit("ret", "dps assist", "cure", "baoe", nullptr);
            break;

        case CLASS_DRUID:
            if (tab == DRUID_TAB_BALANCE)
            {
                engine->addStrategiesNoInit("balance", "cure", "aoe", "cc", "dps assist", nullptr);
            }
            else if (tab == DRUID_TAB_RESTORATION)
                engine->addStrategiesNoInit("resto", "cure", "dps assist", "tranquility", nullptr);
            else
            {
                if (!IsFeralTank(player) && player->HasSpell(SPELL_CAT_FORM))
                    engine->addStrategiesNoInit("cat", "aoe", "cc", "dps assist", "feral charge", nullptr);
                else
                    engine->addStrategiesNoInit("bear", "tank assist", "pull", "pull back", "feral charge", nullptr);
            }
            break;

        case CLASS_HUNTER:
            if (tab == HUNTER_TAB_BEAST_MASTERY)
                engine->addStrategy("bm", false);
            else if (tab == HUNTER_TAB_MARKSMANSHIP)
                engine->addStrategy("mm", false);
            else // if (tab == HUNTER_TAB_SURVIVAL)
                engine->addStrategy("surv", false);
            engine->addStrategiesNoInit("cc", "dps assist", "aoe", "bdps", nullptr);
            break;

        case CLASS_ROGUE:
            if (tab == ROGUE_TAB_COMBAT)
                engine->addStrategiesNoInit("combat", nullptr);
            else // if (tab == ROGUE_TAB_ASSASSINATION || tab == ROGUE_TAB_SUBTLETY)
                engine->addStrategiesNoInit("assassin", nullptr);
            engine->addStrategiesNoInit("dps assist", "aoe", nullptr);
            break;

        case CLASS_WARLOCK:
            if (tab == WARLOCK_TAB_AFFLICTION)
                engine->addStrategiesNoInit("affli", "curse of agony", nullptr);
            else if (tab == WARLOCK_TAB_DEMONOLOGY)
                engine->addStrategiesNoInit("demo", "curse of agony", "meta melee", nullptr);
            else // if (tab == WARLOCK_TAB_DESTRUCTION)
                engine->addStrategiesNoInit("destro", "curse of elements", nullptr);
            engine->addStrategiesNoInit("cc", "dps assist", "aoe", nullptr);
            break;

        case CLASS_DEATH_KNIGHT:
            if (tab == DEATH_KNIGHT_TAB_BLOOD)
                engine->addStrategiesNoInit("blood", "tank assist", "pull", "pull back", nullptr);
            else if (tab == DEATH_KNIGHT_TAB_FROST)
                engine->addStrategiesNoInit("frost", "frost aoe", "dps assist", nullptr);
            else // if (tab == DEATH_KNIGHT_TAB_UNHOLY)
                engine->addStrategiesNoInit("unholy", "unholy aoe", "dps assist", nullptr);
            break;
    }

    if (PlayerbotAI::IsTank(player, true))
        engine->addStrategy("tank face", false);

    if (PlayerbotAI::IsMelee(player, true) && PlayerbotAI::IsDps(player, true))
        engine->addStrategy("behind", false);

    if (PlayerbotAI::IsHeal(player, true))
    {
        if (sPlayerbotAIConfig.AutoSaveMana)
            engine->addStrategy("save mana", false);
        if (!sPlayerbotAIConfig.IsRestrictedHealerDPSMap(player->GetMapId()))
            engine->addStrategy("healer dps", false);
    }

    if (IsSelfBot(player) || sRandomPlayerbotMgr.IsRandomBot(player))
    {
        if (!player->GetGroup())
        {
            // change for heal spec
            engine->addStrategy("boost", false);
            engine->addStrategy("dps assist", false);
            engine->removeStrategy("threat", false);

            switch (player->getClass())
            {
                case CLASS_PRIEST:
                {
                    if (tab != PRIEST_TAB_SHADOW)
                        engine->addStrategiesNoInit("holy dps", "shadow debuff", "shadow aoe", nullptr);
                    break;
                }
                case CLASS_DRUID:
                {
                    if (tab == DRUID_TAB_RESTORATION)
                        engine->addStrategiesNoInit("aoe", nullptr);
                    break;
                }
                case CLASS_SHAMAN:
                {
                    if (tab == SHAMAN_TAB_RESTORATION)
                        engine->addStrategiesNoInit("ele", "caster aoe", nullptr);
                    break;
                }
                case CLASS_PALADIN:
                {
                    if (tab == PALADIN_TAB_HOLY)
                        engine->addStrategiesNoInit("ret", "baoe", nullptr);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void AiFactory::AddClassNonCombatStrategies(Player* player, PlayerbotAI* const /*facade*/, Engine* nonCombatEngine)
{
    uint8 tab = GetPlayerSpecTab(player);

    switch (player->getClass())
    {
        case CLASS_PRIEST:
            nonCombatEngine->addStrategiesNoInit("dps assist", "cure", "rshadow", nullptr);
            break;
        case CLASS_PALADIN:
            if (tab == PALADIN_TAB_PROTECTION)
            {
                nonCombatEngine->addStrategiesNoInit("bthreat", "tank assist", "pull", "barmor", nullptr);
                if (player->GetLevel() >= 20)
                    nonCombatEngine->addStrategy("bsanc", false);
                else
                    nonCombatEngine->addStrategy("bmight", false);
            }
            else if (tab == PALADIN_TAB_HOLY)
                nonCombatEngine->addStrategiesNoInit("dps assist", "bwisdom", "bcast", nullptr);
            else
                nonCombatEngine->addStrategiesNoInit("dps assist", "bmight", "baoe", nullptr);

            nonCombatEngine->addStrategiesNoInit("cure", nullptr);
            break;
        case CLASS_HUNTER:
            nonCombatEngine->addStrategiesNoInit("bdps", "dps assist", "pet", nullptr);
            break;
        case CLASS_SHAMAN:
            nonCombatEngine->addStrategiesNoInit("dps assist", "cure", nullptr);
            break;
        case CLASS_MAGE:
            if (tab == MAGE_TAB_ARCANE || tab == MAGE_TAB_FIRE)
                nonCombatEngine->addStrategy("bdps", false);
            else
                nonCombatEngine->addStrategy("bmana", false);

            nonCombatEngine->addStrategiesNoInit("dps assist", "cure", nullptr);
            break;
        case CLASS_DRUID:
            if (tab == DRUID_TAB_FERAL)
            {
                if (!IsFeralTank(player) && player->HasSpell(SPELL_CAT_FORM))
                    nonCombatEngine->addStrategy("dps assist", false);
                else
                    nonCombatEngine->addStrategiesNoInit("tank assist", "pull", nullptr);
            }
            else
                nonCombatEngine->addStrategiesNoInit("dps assist", "cure", nullptr);
            break;
        case CLASS_WARRIOR:
            if (tab == WARRIOR_TAB_PROTECTION)
                nonCombatEngine->addStrategiesNoInit("tank assist", "pull", nullptr);
            else
                nonCombatEngine->addStrategy("dps assist", false);
            break;
        case CLASS_WARLOCK:
        {
            // Historical warlock stones use equipment slots, rather than bag-use actions.
            uint32 eraTabs[3] = {0, 0, 0};
            bool eraManaged = EraTalentBots_SpecTabs(player, eraTabs);
            if (tab == WARLOCK_TAB_AFFLICTION)
                eraManaged ? nonCombatEngine->addStrategiesNoInit("felhunter", nullptr)
                           : nonCombatEngine->addStrategiesNoInit("felhunter", "spellstone", nullptr);
            else if (tab == WARLOCK_TAB_DEMONOLOGY)
                eraManaged ? nonCombatEngine->addStrategiesNoInit("felguard", nullptr)
                           : nonCombatEngine->addStrategiesNoInit("felguard", "spellstone", nullptr);
            else if (tab == WARLOCK_TAB_DESTRUCTION)
                eraManaged ? nonCombatEngine->addStrategiesNoInit("imp", nullptr)
                           : nonCombatEngine->addStrategiesNoInit("imp", "firestone", nullptr);

            nonCombatEngine->addStrategiesNoInit("dps assist", "ss self", nullptr);
            break;
        }
        case CLASS_DEATH_KNIGHT:
            if (tab == DEATH_KNIGHT_TAB_BLOOD)
                nonCombatEngine->addStrategiesNoInit("tank assist", "pull", nullptr);
            else
                nonCombatEngine->addStrategy("dps assist", false);
            break;
        default:
            nonCombatEngine->addStrategy("dps assist", false);
            break;
    }

    if (sPlayerbotAIConfig.AutoSaveMana && PlayerbotAI::IsHeal(player, true))
        nonCombatEngine->addStrategy("save mana", false);

}

void AiFactory::RefreshCombatStrategies(Player* player, PlayerbotAI* facade, Engine* combat, Engine* nonCombat)
{
    // Only class and role defaults belong to a profile; user travel, loot and instance choices stay.
    static char const* const managed[] = {
        "affli", "aoe", "arcane", "arms", "assassin", "balance", "baoe",
        "barmor", "bcast", "bdps", "bear", "behind", "blood", "bm",
        "bmana", "bmight", "bsanc", "bthreat", "bwisdom", "caster aoe", "cat",
        "cc", "combat", "cure", "curse of agony", "curse of elements", "demo", "destro",
        "disc", "dps assist", "ele", "enh", "felguard", "felhunter", "feral charge",
        "fire", "firestone", "flametongue", "frost", "frost aoe", "frostfire", "fury",
        "healer dps", "healing stream", "holy", "holy dps", "holy heal", "imp", "magma",
        "mana spring", "meta melee", "mm", "pet", "prot", "pull", "pull back",
        "resto", "ret", "rshadow", "save mana", "shadow", "shadow aoe", "shadow debuff",
        "spellstone", "ss self", "stoneskin", "strength of earth", "surv", "tank assist", "tank face",
        "tranquility", "unholy", "unholy aoe", "windfury", "wrath", "wrath of air",
    };
    for (char const* name : managed)
    {
        combat->removeStrategy(name, false);
        nonCombat->removeStrategy(name, false);
    }
    AddClassCombatStrategies(player, facade, combat);
    AddClassNonCombatStrategies(player, facade, nonCombat);
    combat->Init();
    nonCombat->Init();
}

void AiFactory::AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine)
{
    if (!player->InBattleground())
        engine->addStrategiesNoInit("racials", "chat", "default", "cast time", "potions", "duel", "boost", nullptr);

    if (sPlayerbotAIConfig.AutoAvoidAoe && facade->HasGameClientMaster())
        engine->addStrategy("avoid aoe", false);

    engine->addStrategy("formation", false);

    AddClassCombatStrategies(player, facade, engine);

    if (sRandomPlayerbotMgr.IsRandomBot(player))
        engine->ChangeStrategy(sPlayerbotAIConfig.RandomBotCombatStrategies);
    else
        engine->ChangeStrategy(sPlayerbotAIConfig.CombatStrategies);

    // Battleground switch
    if (player->InBattleground() && player->GetBattleground())
    {
        BattlegroundTypeId bgType = player->GetBattlegroundTypeId();
        if (bgType == BATTLEGROUND_RB)
            bgType = player->GetBattleground()->GetBgTypeID(true);

        if (bgType == BATTLEGROUND_WS)
            engine->addStrategy("warsong", false);

        if (bgType == BATTLEGROUND_AB)
            engine->addStrategy("arathi", false);

        if (bgType == BATTLEGROUND_AV)
            engine->addStrategy("alterac", false);

        if (bgType == BATTLEGROUND_EY)
            engine->addStrategy("eye", false);

        if (bgType == BATTLEGROUND_IC)
            engine->addStrategy("isle", false);

        if (player->InArena())
        {
            engine->addStrategy("arena", false);
            engine->addStrategiesNoInit("boost", "racials", "chat", "default", "aoe", "cast time", "dps assist", nullptr);
        }
        else
            engine->addStrategiesNoInit("boost", "racials", "chat", "default", "aoe", "potions", "cast time", "dps assist", nullptr);

        engine->removeStrategy("custom::say", false);
        engine->removeStrategy("threat", false);
        engine->addStrategy("boost", false);
    }
}

Engine* AiFactory::createCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext)
{
    Engine* engine = new Engine(facade, aiObjectContext);
    AddDefaultCombatStrategies(player, facade, engine);
    engine->Init();
    return engine;
}

void AiFactory::AddDefaultNonCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* nonCombatEngine)
{
    AddClassNonCombatStrategies(player, facade, nonCombatEngine);

    if (!player->InBattleground())
    {
        nonCombatEngine->addStrategiesNoInit("nc", "food", "chat", "follow", "default", "force rebuff", "quest", "loot",
                                            "gather", "duel", "pvp", "buff", "mount", "emote", nullptr);
    }

    if ((sRandomPlayerbotMgr.IsRandomBot(player)) && !player->InBattleground())
    {
        Player* master = facade->GetMaster();

        // let 25% of free bots start duels.
        if (!urand(0, 3))
            nonCombatEngine->addStrategy("start duel", false);

        if (sPlayerbotAIConfig.RandomBotJoinLfg)
            nonCombatEngine->addStrategy("lfg", false);

        if (!player->GetGroup() || player->GetGroup()->GetLeaderGUID() == player->GetGUID())
        {
            // let 25% of random not grouped (or grp leader) bots help other players
            // if (!urand(0, 3))
            //     nonCombatEngine->addStrategy("attack tagged");

            // nonCombatEngine->addStrategy("pvp", false);
            // nonCombatEngine->addStrategy("collision");
            // nonCombatEngine->addStrategy("group");
            // nonCombatEngine->addStrategy("guild");
            nonCombatEngine->addStrategy("grind", false);

            if (sPlayerbotAIConfig.EnableNewRpgStrategy)
                nonCombatEngine->addStrategy("new rpg", false);
            else if (sPlayerbotAIConfig.AutoDoQuests)
            {
                // nonCombatEngine->addStrategy("travel");
                nonCombatEngine->addStrategy("rpg", false);
            }
            else
                nonCombatEngine->addStrategy("move random", false);

            if (sPlayerbotAIConfig.RandomBotJoinBG)
                nonCombatEngine->addStrategy("bg", false);

            // if (!master || GET_PLAYERBOT_AI(master))
            //     nonCombatEngine->addStrategy("maintenance");

            nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.RandomBotNonCombatStrategies);
        }
        else
        {
            if (facade)
            {
                if (master)
                {
                    PlayerbotAI* masterBotAI = GET_PLAYERBOT_AI(master);
                    if (masterBotAI || sRandomPlayerbotMgr.IsRandomBot(player))
                    {
                        // nonCombatEngine->addStrategy("pvp", false);
                        // nonCombatEngine->addStrategy("collision");
                        // nonCombatEngine->addStrategy("group");
                        // nonCombatEngine->addStrategy("guild");

                        // if (sPlayerbotAIConfig.AutoDoQuests)
                        // {
                        //     // nonCombatEngine->addStrategy("travel");
                        //     nonCombatEngine->addStrategy("rpg");
                        // }
                        // else
                        // {
                        //     nonCombatEngine->addStrategy("move random");
                        // }

                        // if (masterBotAI)
                        //     nonCombatEngine->addStrategy("maintenance");

                        nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.RandomBotNonCombatStrategies);
                    }
                    else
                    {
                        // nonCombatEngine->addStrategy("pvp", false);
                        nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.NonCombatStrategies);
                    }
                }
            }
        }
    }
    else
        nonCombatEngine->ChangeStrategy(sPlayerbotAIConfig.NonCombatStrategies);

    // Battleground switch
    if (player->InBattleground() && player->GetBattleground())
    {
        nonCombatEngine->addStrategiesNoInit("nc", "chat", "default", "force rebuff", "buff", "food", "mount", "pvp",
                                       "dps assist", "attack tagged", "emote", nullptr);
        nonCombatEngine->removeStrategy("custom::say", false);
        nonCombatEngine->removeStrategy("travel", false);
        nonCombatEngine->removeStrategy("rpg", false);
        nonCombatEngine->removeStrategy("grind", false);

        BattlegroundTypeId bgType = player->GetBattlegroundTypeId();
        if (bgType == BATTLEGROUND_RB)
            bgType = player->GetBattleground()->GetBgTypeID(true);

        if ((bgType <= BATTLEGROUND_EY || bgType == BATTLEGROUND_IC) &&
            !player->InArena())  // do not add for not supported bg or arena
            nonCombatEngine->addStrategy("battleground", false);

        if (bgType == BATTLEGROUND_WS)
            nonCombatEngine->addStrategy("warsong", false);

        if (bgType == BATTLEGROUND_AV)
            nonCombatEngine->addStrategy("alterac", false);

        if (bgType == BATTLEGROUND_AB)
            nonCombatEngine->addStrategy("arathi", false);

        if (bgType == BATTLEGROUND_EY)
            nonCombatEngine->addStrategy("eye", false);

        if (bgType == BATTLEGROUND_IC)
            nonCombatEngine->addStrategy("isle", false);

        if (player->InArena())
        {
            nonCombatEngine->addStrategy("arena", false);
            nonCombatEngine->removeStrategy("mount", false);
        }
    }
}

Engine* AiFactory::createNonCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext)
{
    Engine* nonCombatEngine = new Engine(facade, aiObjectContext);

    AddDefaultNonCombatStrategies(player, facade, nonCombatEngine);
    nonCombatEngine->Init();
    return nonCombatEngine;
}

void AiFactory::AddDefaultDeadStrategies(Player* player, PlayerbotAI* const facade, Engine* deadEngine)
{
    (void)facade;  // unused and remove warning
    deadEngine->addStrategiesNoInit("dead", "stay", "chat", "default", "follow", nullptr);

    if (sRandomPlayerbotMgr.IsRandomBot(player) && !player->GetGroup())
        deadEngine->removeStrategy("follow", false);
}

Engine* AiFactory::createDeadEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* AiObjectContext)
{
    Engine* deadEngine = new Engine(facade, AiObjectContext);
    AddDefaultDeadStrategies(player, facade, deadEngine);
    deadEngine->Init();
    return deadEngine;
}

void AiFactory::AddDefaultReactionStrategies(Player* player, PlayerbotAI* const /*facade*/,
                                             ReactionEngine* reactionEngine)
{
    reactionEngine->addStrategies("react", "chat", nullptr);

    if (sRandomPlayerbotMgr.IsRandomBot(player))
        reactionEngine->ChangeStrategy(sPlayerbotAIConfig.RandomBotReactStrategies);
    else
        reactionEngine->ChangeStrategy(sPlayerbotAIConfig.ReactStrategies);
}

ReactionEngine* AiFactory::createReactionEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext)
{
    ReactionEngine* reactionEngine = new ReactionEngine(facade, aiObjectContext);
    AddDefaultReactionStrategies(player, facade, reactionEngine);
    reactionEngine->Init();
    return reactionEngine;
}
