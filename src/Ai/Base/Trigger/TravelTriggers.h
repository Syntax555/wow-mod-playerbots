/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_TRAVELTRIGGERS_H
#define PLAYERBOTS_TRAVELTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

class NoTravelTargetTrigger : public Trigger
{
public:
    NoTravelTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "no travel target") {}

    bool IsActive() override;
};

class FarFromTravelTargetTrigger : public Trigger
{
public:
    FarFromTravelTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "far from travel target") {}

    bool IsActive() override;
};

// The master is on another map or island the bot can reach, or so far away that a flight may be shorter.
class MasterFarAwayTrigger : public Trigger
{
public:
    MasterFarAwayTrigger(PlayerbotAI* botAI) : Trigger(botAI, "master far away", 2) {}

    bool IsActive() override;
};

#endif
