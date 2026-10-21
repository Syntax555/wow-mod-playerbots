/*
 * Raid clear: Tranquilizing Shot on the frenzies that matter.
 *
 * Playerbots' own rule only looks at each hunter's own target. A boss's Frenzy (Magmadar's drives
 * a Lava Breath every few seconds, Flamegor's a Fire Nova at the whole raid) and a trash mob's
 * Enrage (a Death Talon Seether's doubles its attack speed) are taken off here by one hunter: the
 * nearest of the group's hunter bots that can shoot it now, so they don't all fire at the same one.
 * Each raid lists the mobs and the auras.
 *
 * Released under the MIT License.
 */

#ifndef MOD_RAID_CLEAR_TRANQUILIZE_H
#define MOD_RAID_CLEAR_TRANQUILIZE_H

#include "Action.h"
#include "Trigger.h"

#include <vector>

namespace RaidClear::Tranq
{
    // Tranquilizing Shot's range.
    constexpr float TRANQUILIZING_SHOT_RANGE = 35.0f;

    // A mob whose enrage a Tranquilizing Shot takes off. A boss is found anywhere in the instance
    // while it is fighting; trash only within the shot's range of the hunter.
    struct Enrage
    {
        uint32 entry;
        uint32 aura;
        bool boss;
    };
}

// The hunter is the one to take an enrage off now.
class RcTranqTrigger : public Trigger
{
public:
    RcTranqTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc tranq") {}
    bool IsActive() override;
};

class RcTranqAction : public Action
{
public:
    RcTranqAction(PlayerbotAI* botAI) : Action(botAI, "rc tranq") {}
    bool Execute(Event event) override;
};

#endif
