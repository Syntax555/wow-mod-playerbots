/*
 * Raid clear: fire on the ground and the fronts of mobs that cleave or breathe.
 *
 * Playerbots' own "avoid aoe" only takes a bot out of fire it is already burning in, by a step
 * straight back (into a wall or the next pack as often as not), and doesn't know about a mob's
 * cleave or breath at all. Here each raid lists what it has:
 *   - casters whose fire stays on the ground (a Rain of Fire, a Flamestrike): read from the caster
 *     while it burns, not from the cast;
 *   - fire traps on the floor (Magmadar's Lava Bombs): the game object, as far as its trap or the
 *     spell it casts reaches, whichever is further;
 *   - mobs that cleave: everyone but the tanks stays out of the front of one it isn't on, a set
 *     reach past its own size and angle either side of its facing;
 *   - mobs that breathe a cone: the same, with the reach and the angle read from the spell (its
 *     radius, and the cone the core uses for it: the spell_cone table, else its target type).
 * A bot inside any of them walks out by the way the aggro leash allows (Common/WayOut.h).
 *
 * Released under the MIT License.
 */

#ifndef MOD_RAID_CLEAR_HAZARDS_H
#define MOD_RAID_CLEAR_HAZARDS_H

#include "Common/WayOut.h"

#include "Action.h"
#include "Multiplier.h"
#include "Trigger.h"

#include <vector>

namespace RaidClear::Hazards
{
    // A bot keeps this far past the edge of fire on the ground (the bot's own size: the core
    // counts a unit as in it from there), and this far past the reach of a cone beyond its own
    // size, and this far either side of the cone's angle. A walk out ends this far clear. The
    // hazards are looked up per bot every HAZARD_FRESH_MS; fire traps within TRAP_SEARCH of it
    // (see Trap).
    constexpr float GROUND_FIRE_MARGIN = 1.5f;
    constexpr float CONE_MARGIN = 1.0f;
    constexpr float CONE_ANGLE_MARGIN = 0.1745f;  // 10 degrees
    constexpr float HAZARD_SPOT = 2.0f;
    constexpr uint32 HAZARD_FRESH_MS = 100;
    constexpr float TRAP_SEARCH = 30.0f;
    // A thrown trap is looked for this long after its thrower was last in a fight in the
    // instance (Magmadar's last a minute at most).
    constexpr uint32 THROWN_TRAP_MS = 90000;

    // A caster whose spell leaves fire on the ground.
    struct Burner
    {
        uint32 entry;
        uint32 spell;
    };

    // A mob that cleaves whoever it is on: this far past its own size, this far either side of
    // its facing.
    struct Cleaver
    {
        uint32 entry;
        float reach;
        float halfAngle;
    };

    // A fire trap game object: a spawned one only while the mob that throws it is in the fight, a
    // thrown one (no spawn of its own) for as long as it burns, its thrower dead or not (up to
    // THROWN_TRAP_MS after it was last fighting).
    struct Trap
    {
        uint32 entry;
        uint32 owner;
    };

    // A mob that breathes this cone at whoever it is on; only while it has `aura`, if one is set.
    struct Breather
    {
        uint32 entry;
        uint32 spell;
        uint32 aura;
    };

    struct Table
    {
        std::vector<Burner> burners;
        std::vector<Trap> traps;
        std::vector<Cleaver> cleavers;
        std::vector<Breather> breathers;
    };
}

// The bot stands in fire that is burning, or (not a tank) in front of a mob on the raid's list
// that cleaves or breathes and isn't on it.
class RcHazardTrigger : public Trigger
{
public:
    RcHazardTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc hazard") {}
    bool IsActive() override;
};

// With "stay" in the name for playerbots' "passive" (the camp hold mod-dungeon-clear puts on the
// bots waiting for a pull), which lets through only names with "stay", "follow" or "move from
// group" in them.
// Playerbots' "follow" doesn't walk a bot into fire or across it: its formation spot or the one
// it follows is beyond a burning trap, the bot walks out, follows back in, and so on until the
// trap is gone. Not for a bot already inside one (the way out is RcHazardAction's).
class RcHazardFollowMultiplier : public Multiplier
{
public:
    RcHazardFollowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc hazard follow") {}
    float GetValue(Action* action) override;
};

class RcHazardAction : public Action
{
public:
    RcHazardAction(PlayerbotAI* botAI) : Action(botAI, "rc hazard stay clear"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

#endif
