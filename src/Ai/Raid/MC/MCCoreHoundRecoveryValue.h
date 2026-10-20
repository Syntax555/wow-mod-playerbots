/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MC_CORE_HOUND_RECOVERY_VALUE_H
#define PLAYERBOTS_MC_CORE_HOUND_RECOVERY_VALUE_H

#include "Value.h"

class McCoreHoundRecoveryValue : public BoolCalculatedValue
{
public:
    McCoreHoundRecoveryValue(PlayerbotAI* botAI)
        : BoolCalculatedValue(botAI, "mc core hound recovery", 1 * IN_MILLISECONDS)
    {
    }
    bool Calculate() override;

private:
    bool _waiting = false;
};

#endif
