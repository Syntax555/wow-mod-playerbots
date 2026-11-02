/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BINDHOMEACTION_H
#define PLAYERBOTS_BINDHOMEACTION_H

#include "Action.h"

// Binds a companion at the innkeeper where its master just made its home.
class BindHomeAction : public Action
{
public:
    BindHomeAction(PlayerbotAI* botAI) : Action(botAI, "bind home") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

#endif
