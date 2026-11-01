/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRAVELACTION_H
#define PLAYERBOTS_TRAVELACTION_H

#include "MovementActions.h"

class PlayerbotAI;

class TravelAction : public MovementAction
{
public:
    TravelAction(PlayerbotAI* botAI) : MovementAction(botAI, "travel") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
