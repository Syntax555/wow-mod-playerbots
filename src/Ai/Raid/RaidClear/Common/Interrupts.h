/*
 * Raid clear: interrupting an enemy's cast, whoever's target it is.
 *
 * Playerbots only interrupts the bot's own target (Kick, Pummel) or, with Counterspell, Wind Shear
 * and Silence, an enemy healer it happens to see. A raid's must-stop casts (a zealot's or a
 * princess's Great Heal) are interrupted by any bot that has one ready and in reach.
 *
 * The core only stops a cast that silence can stop (its prevention type) and that interrupts can
 * (its interrupt flags), and not on a mob immune to the interrupt's mechanic. Many raid casters
 * are: in Blackwing Lair the Death Talon Wyrmkin, Hatchers and Spellbinders, so no kick stops
 * their Fireball Volley and Flamestrike, nor the Taskmasters' Healing Circle.
 *
 * A stun or an incapacitate stops any cast it lands on, whatever the cast's flags, and several of
 * those casters aren't immune to all of them: Gouge, Repentance and Scatter Shot take a Wyrmkin,
 * Hammer of Justice and Gouge a Spellbinder. Those cost long cooldowns, so a raid spends them
 * only on the casts it lists as worth one.
 *
 * Released under the MIT License.
 */

#ifndef MOD_RAID_CLEAR_INTERRUPTS_H
#define MOD_RAID_CLEAR_INTERRUPTS_H

#include <string>

class Player;
class PlayerbotAI;
class SpellInfo;
class Unit;

namespace RaidClear
{
    // Whether the bot has the power for this spell (its playerbots name) right now. CanCastSpell
    // tests a spell with its cost ignored: a rogue out of energy would pass and then fail the
    // cast on every tick.
    bool CanPayFor(PlayerbotAI* botAI, Player* bot, char const* spell);

    // The first interrupt the bot has ready, paid for and in reach of the target (playerbots'
    // name), or nullptr. A tank only interrupts what it holds: it would turn its back on that.
    // Not one the target is immune to: a kick at a Death Talon Wyrmkin only burns the cooldown.
    char const* ReadyInterrupt(PlayerbotAI* botAI, Player* bot, Unit* target);

    // The first stun or incapacitate the bot has ready, paid for and in reach of the target, that
    // the target isn't immune to (playerbots' name), or nullptr. The same tank rule as above.
    char const* ReadyCastStopper(PlayerbotAI* botAI, Player* bot, Unit* target);

    // Whether an interrupt can stop this cast at all.
    bool Interruptible(SpellInfo const* cast);

    // One of the interrupts above, by its playerbots name.
    bool IsInterrupt(std::string const& name);
}

#endif
