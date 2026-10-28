/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_AIFACTORY_H
#define PLAYERBOTS_AIFACTORY_H

#include "Common.h"
#include <map>
#include <vector>

class AiObjectContext;
class Engine;
class Player;
class PlayerbotAI;
class ReactionEngine;

enum BotRoles : uint8;

class AiFactory
{
public:
    static AiObjectContext* createAiObjectContext(Player* player, PlayerbotAI* botAI);
    static Engine* createCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext);
    static Engine* createNonCombatEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext);
    static Engine* createDeadEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aibjectContext);
    static ReactionEngine* createReactionEngine(Player* player, PlayerbotAI* const facade, AiObjectContext* aiObjectContext);
    static void AddDefaultNonCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* nonCombatEngine);
    static void AddDefaultDeadStrategies(Player* player, PlayerbotAI* const facade, Engine* deadEngine);
    static void AddDefaultCombatStrategies(Player* player, PlayerbotAI* const facade, Engine* engine);
    static void AddDefaultReactionStrategies(Player* player, PlayerbotAI* const facade, ReactionEngine* reactionEngine);

    static std::vector<std::string> GetCombatProfiles(uint8 cls);
    static std::string GetCombatProfile(Player* player);
    static std::string GetEffectiveCombatProfile(Player* player);
    static bool SetCombatProfile(Player* player, std::string const& mode);
    static bool IsFeralTank(Player* player);
    static void AddClassCombatStrategies(Player* player, PlayerbotAI* facade, Engine* engine);
    static void AddClassNonCombatStrategies(Player* player, PlayerbotAI* facade, Engine* engine);
    static void RefreshCombatStrategies(Player* player, PlayerbotAI* facade, Engine* combat, Engine* nonCombat);

    static uint8 GetPlayerSpecTab(Player* player);
    static std::map<uint8, uint32> GetPlayerSpecTabs(Player* player);
    static BotRoles GetPlayerRoles(Player* player);
    static std::string GetPlayerSpecName(Player* player);
};

#endif
