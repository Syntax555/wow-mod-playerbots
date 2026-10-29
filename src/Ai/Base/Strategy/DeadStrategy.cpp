/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "DeadStrategy.h"
#include "Playerbots.h"

void DeadStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    PassThroughStrategy::InitTriggers(triggers);

    // Corpse reclaim must remain eligible during minimal-activity updates.
    float const deadRelevance = relevance + (sPlayerbotAIConfig.naturalProgression ? 1.0f : 0.0f);

    triggers.push_back(
        new TriggerNode("often", { NextAction("auto release", deadRelevance) }));
    triggers.push_back(
        new TriggerNode("bg active", { NextAction("auto release", deadRelevance) }));
    triggers.push_back(
        new TriggerNode("dead", { NextAction("find corpse", deadRelevance) }));
    triggers.push_back(new TriggerNode(
        "corpse near", { NextAction("revive from corpse", deadRelevance - 1.0f) }));
    triggers.push_back(new TriggerNode("resurrect request",
                                       { NextAction("accept resurrect", deadRelevance) }));
    triggers.push_back(
        new TriggerNode("falling far", { NextAction("repop", deadRelevance + 1.f) }));
    triggers.push_back(
        new TriggerNode("location stuck", { NextAction("repop", deadRelevance + 1) }));
    triggers.push_back(new TriggerNode(
        "can self resurrect", { NextAction("self resurrect", deadRelevance + 2.0f) }));
}

DeadStrategy::DeadStrategy(PlayerbotAI* botAI) : PassThroughStrategy(botAI) {}
