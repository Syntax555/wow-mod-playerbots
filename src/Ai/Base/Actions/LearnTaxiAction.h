/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LEARNTAXIACTION_H
#define PLAYERBOTS_LEARNTAXIACTION_H

#include "Action.h"

class LearnTaxiAction : public Action
{
public:
    LearnTaxiAction(PlayerbotAI* botAI) : Action(botAI, "learn taxi") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

#endif
