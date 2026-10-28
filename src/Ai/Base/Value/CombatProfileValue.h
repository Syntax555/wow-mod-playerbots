/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_COMBATPROFILEVALUE_H
#define PLAYERBOTS_COMBATPROFILEVALUE_H

#include "AiFactory.h"
#include "Player.h"
#include "Value.h"
#include <algorithm>

class CombatProfileValue : public ManualSetValue<std::string>
{
public:
    CombatProfileValue(PlayerbotAI* botAI) : ManualSetValue<std::string>(botAI, "auto", "combat profile") {}

    std::string const Save() override { return value; }

    bool Load(std::string const text) override
    {
        auto profiles = AiFactory::GetCombatProfiles(bot->getClass());
        if (std::find(profiles.begin(), profiles.end(), text) == profiles.end())
            return false;
        value = text;
        return true;
    }
};

#endif
