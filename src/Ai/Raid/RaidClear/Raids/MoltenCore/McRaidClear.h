/*
 * Raid clear: Molten Core (map 409).
 *
 * Runs next to mod-playerbots' "moltencore" strategy, which already handles the Golemagg / Core
 * Rager tank split and Magma Splash back-off (with this strategy on, the melee take turns there:
 * see MCHelpers.h), Core Hound skull rotation, lava escape and the resistance auras/totems. Its
 * own Shazzrah rule stands down while this one runs.
 *
 * Added here (mechanics from the core scripts in scripts/EasternKingdoms/BlackrockMountain/
 * MoltenCore):
 *   - Kill order: Lava Spawn (splits every 15s), the healing adds (Sulfuron's priests,
 *     Majordomo's healers), then the other boss adds and Ragnaros' Sons of Flame.
 *   - Fire on the ground and breaths (Common/Hazards.h): bots walk out of Gehennas' Rain of Fire
 *     while it burns and out of Magmadar's Lava Bombs (fire traps, 5 yd), and everyone but the
 *     tanks out of the front of an Ancient Core Hound or Magmadar (Lava Breath, a 54 degree cone
 *     35 yd long: one hit 19 players in a log) or a Flameguard (Cone of Fire) it isn't on.
 *     Playerbots' "avoid aoe" only goes on bots with a real player as master, and only steps a
 *     bot back out of fire that is already burning it.
 *   - Magmadar's Frenzy (an enrage, and a Lava Breath every 4 s while it lasts; a log showed no
 *     Tranquilizing Shot on any of five): the nearest hunter takes it off (Common/Tranquilize.h).
 *   - Sulfuron's Flamewaker Priests cast Dark Mending (19775, 2 s), which heals the friend
 *     lowest on health for 27,750. Any bot with an interrupt ready and in reach stops it, its own
 *     target or not, one bot per cast (Common/Interrupts.h); where no interrupt gets through, a
 *     stun or an incapacitate. A bot that isn't casting is woken to look the moment one starts.
 *   - Shazzrah's Arcane Explosion (19712) reaches 30 yd from his middle, and every 45 s his
 *     Gate puts him on top of a random player out of his reach (wiping his threat) and he casts
 *     one at once. Playerbots kept its ranged 26 yd from his edge, by a step straight back, found
 *     him only through threat and left the healers out. Here every ranged bot and healer within
 *     32.5 yd of his middle walks to the nearest spot 34.5 yd out (28.5 from his edge, where its
 *     spells still reach him), wherever he has gone, a bot in the middle of a cast is told to drop
 *     it when he starts one, and nobody walks back toward anything that close to him.
 *   - Magmadar: every shaman keeps Tremor Totem down. Panic is a 45 yd raid-wide fear.
 *     Playerbots keeps each shaman's own earth totem (Stoneskin, Strength of Earth) down the
 *     same way, and the two share a slot: left alone they replace each other every second and
 *     a half and the shaman does nothing else all fight. So near Magmadar a shaman who knows
 *     Tremor Totem places no other earth totem (and no Call of the Elements, which would).
 *   - Baron Geddon: Inferno (19695, 8 s) burns everyone within 20 yd of him once a second, for
 *     500 at first and 5,000 at the end; he stands still for it. Playerbots walks its bots out,
 *     but only those it finds through "find target", which is a bot's own threat list: a tank
 *     holding an add, a bot on the trash that came with him, a bot revived mid-fight never see
 *     him and stand in all eight pulses. Here every bot within 23 yd of him walks to the nearest
 *     spot 25 yd out (see Common/WayOut.h), whoever he is on included, and nobody walks back
 *     toward anything that close to him until it is over. A bot in the middle of a cast is
 *     told to drop it the moment Inferno starts, and one still inside casts nothing it has to
 *     stand still for.
 *   - Majordomo: nobody casts at an enemy with Magic Reflection, and melee (not tanks) stop
 *     swinging at one with Damage Reflection. Both shields cover all of his adds.
 *     The Hot Coal in the middle of his room (the pit his Teleport throws players into) is a
 *     trap: with a player within about 7 yd of its middle, everyone within 10 yd takes Intense
 *     Heat (20533, about 750 fire a second, logged as environmental damage). Nothing in
 *     playerbots knows it (its lava check looks at the liquid under a bot; this is a game
 *     object). In combat a bot that has stopped within 12 yd of it walks out, on the side its
 *     target is on; one only crossing the pit is left alone. A tank with a mob on it goes 22 yd
 *     out when either of them is within 12 yd, so the mob follows it out of reach. Melee whose
 *     target stands in it stay on that target.
 *   - Ragnaros' Lava Burst (21908): three eruptions every 10 s or so at eight fixed places,
 *     seven of them on the rock 30 to 45 yd from him, each hitting whoever stands around it (logs
 *     of 2026-10-15/16: 157 and 252 hits, up to 38 players by one eruption; Wrath of Ragnaros hit
 *     no ranged bot or healer at all). Ranged bots and healers each take a spot of their own on
 *     the rock 30 to 42 yd from him, as far from all eight as there is, dry and in his line of
 *     sight, 7 yd apart (Might of Ragnaros puts a Flame on a random mana user whose Intense Heat
 *     throws everyone within 20 yd of it) where there is room for that no closer to the eruptions
 *     than 3.5 yd apart would put them, 3.5 yd apart for whoever is left: the west side of the
 *     room. Healers pick first, among spots within 36 yd of the main tank so he stays in reach
 *     of their heals. Spots are handed out once and
 *     kept, a dead bot's too, so a death or the tank thrown back doesn't move everyone. While he
 *     is up a bot walks back to its spot when it is more than 4 yd off, and doesn't walk away
 *     from it for anything else but the lava (a heal out of reach is somebody else's).
 *   - Ragnaros' main tank holds him from a spot 18 yd in front of him (in his reach: he is
 *     rooted and 20 yd wide). Out of his reach for 4 s and he Magma Blasts his target for 6,000
 *     every 4 s, and his Wrath throws the tank out every 25 s: the tank walks straight back when
 *     it is out of his reach or 6 yd off the spot, ahead of anything else. Back in his reach it
 *     taunts him off whoever he is on (he picks a random target when he comes out of the lava).
 *   - Ragnaros: ranged and healers stay out of Wrath of Ragnaros (20566), which reaches 25 yd
 *     from his middle. He is 20 yd wide himself (his combat reach), so this is measured centre
 *     to centre: 27.5 yd from his middle is only 6 yd from his edge. A bot closer than that
 *     walks to the nearest dry spot 29.5 yd out, and doesn't walk back toward anything that
 *     close to him. Not while he is under the lava.
 *   - Baron Geddon's Living Bomb (20475) goes off after 8 s and hits every raid member within
 *     18 yd of its carrier. Playerbots moves the carrier away from the middle of the raid by
 *     as many yards as its nearest neighbour is short of 20: enough for a bot at the edge of
 *     the group, not for one in the middle of it, which is where the healers stand. Here the
 *     carrier walks to the nearest spot 24.5 yd from every other raid member (looked for up to
 *     40 yd away) and not within Inferno's reach of Geddon, or to the emptiest spot there is.
 *   - Molten Giants and Molten Destroyers hit everyone within 10 and 13 yd of them (Smash,
 *     Massive Tremor). Ranged and healers have no business there, but playerbots lets them
 *     stand anywhere in range (logs of 2026-10-15/16: about half of the ~450 hits of each per
 *     run were on healers and ranged). A ranged bot or healer inside either walks out to the
 *     nearest spot 5 yd clear of every one in the fight.
 *   - Core Hounds: one brought to 0 health plays dead for 10 s and takes no damage; then it
 *     dies if no other Core Hound within 80 yd is still standing in the fight, else it gets up
 *     at full health. So the pack has to come down together. Playerbots skulls the healthiest
 *     hound, but only among those on the main tank's own threat list, and the off-tank split
 *     spread the pack over four tanks and three pets (a log of 2026-10-16: ten hounds at
 *     Lucifron went down one after another, two seconds apart, fourteen times each in 7.5
 *     minutes; five at Magmadar's door took two minutes). Here the hounds are tanked in one
 *     heap (the raid's stack adds, see Common/TankRoles.h) and:
 *       - skull sits on the healthiest standing hound in the fight, and stays there until
 *         another has more than half the band (CORE_HOUND_HEALTH_BAND) more health;
 *       - nobody but the tank holding it aims a single-target attack at a hound more than the
 *         band below the healthiest (bots already on one turn away, pets too); area and chain
 *         spells still go out while no hound is that far behind, they even the pack out, and
 *         wait while one is (a hound that got up again at full health);
 *       - nobody hits a hound that is playing dead (a log of 2026-10-16: a quarter of all
 *         hits on hounds landed on those, for nothing), and from the moment one plays dead
 *         skull goes to the standing hound with the least health, so the rest, all within the
 *         band of each other, go down inside its 10 s.
 *   - Garr: a Firesworn more than 40 yd from him gets Separation Anxiety (23492, +300% damage),
 *     and a Firesworn erupts when it dies (Eruption, 19497: about 2,000 fire to everyone within
 *     15 yd). With the two together that is 8,000, which kills everyone around it. So the fight
 *     stays with Garr: a bot more than 30 yd from him walks back to 22 (a Firesworn follows
 *     whoever it is on; a tank only comes when a Firesworn is on it, so trash pulled with the
 *     boss stays where its tank holds it), nobody but the tanks hits a Firesworn that has
 *     Separation Anxiety or is about to get it (bots already on one stop, pets too, and a cast
 *     or channel already going at it is broken off), no area or chain spell is cast where it
 *     would land on one (a log of 2026-10-15: a Multi-Shot at the Firesworn beside it and an
 *     Arcane Missiles that kept channelling finished one, and its Eruption killed nine), a
 *     running Blizzard or Volley it walks into is broken off, a tank holds one below 40% with
 *     taunts only (no swings, no area spells) until Separation Anxiety is gone, and the kill
 *     order skips it.
 *
 * Released under the MIT License.
 */

#ifndef MOD_RAID_CLEAR_MOLTEN_CORE_H
#define MOD_RAID_CLEAR_MOLTEN_CORE_H

#include "Common/Hazards.h"
#include "Common/KillOrder.h"
#include "Common/Knockback.h"
#include "Common/TankRoles.h"
#include "Common/Tranquilize.h"
#include "Common/WayOut.h"

#include "Action.h"
#include "MovementActions.h"
#include "Multiplier.h"
#include "Strategy.h"
#include "Trigger.h"

#include <array>

#include <vector>

namespace RaidClear::MoltenCore
{
    constexpr uint32 MAP_ID = 409;

    enum Creatures : uint32
    {
        NPC_MAGMADAR             = 11982,
        NPC_GARR                 = 12057,
        NPC_BARON_GEDDON         = 12056,
        NPC_GOLEMAGG             = 11988,

        NPC_MOLTEN_GIANT         = 11658,  // Smash: 10 yd AoE, Knock Away
        NPC_MOLTEN_DESTROYER     = 11659,  // Massive Tremor: 13 yd AoE, Knockdown
        NPC_MAJORDOMO            = 12018,
        NPC_RAGNAROS             = 11502,

        NPC_FLAMEWALKER          = 11661,  // Gehennas
        NPC_FLAMEWALKER_PRIEST   = 11662,  // Sulfuron
        NPC_FLAMEWAKER_HEALER    = 11663,  // Majordomo
        NPC_FLAMEWAKER_ELITE     = 11664,  // Majordomo
        NPC_FIRESWORN            = 12099,  // Garr
        NPC_FLAMEWALKER_PROTECTOR = 12119, // Lucifron
        NPC_SON_OF_FLAME         = 12143,  // Ragnaros, submerge phase
        NPC_LAVA_SPAWN           = 12265,  // Firelord trash, splits every 15s
        NPC_LAVA_SURGER          = 12101,  // patrols; Surge
        NPC_SULFURON             = 12098,
        NPC_CORE_HOUND           = 11671,  // packs of five: Lucifron, Magmadar
        NPC_SHAZZRAH             = 12264,
        NPC_GEHENNAS             = 12259,
        NPC_ANCIENT_CORE_HOUND   = 11673,  // Lava Breath
        NPC_FLAMEGUARD           = 11667,  // Cone of Fire
    };

    enum GameObjects : uint32
    {
        GO_LAVA_BOMB             = 177704, // Magmadar's Lava Bomb: a fire trap (Conflagration, 19428)
    };

    enum Spells : uint32
    {
        SPELL_MAGIC_REFLECTION   = 20619,
        SPELL_DAMAGE_REFLECTION  = 21075,
        SPELL_SEPARATION_ANXIETY = 23492,  // on a Firesworn more than 40 yd from Garr
        SPELL_INFERNO            = 19695,  // on Baron Geddon; each pulse is 19698, 20 yd around him
        SPELL_LIVING_BOMB        = 20475,  // on a player; explodes as 20476, 18 yd around it
        SPELL_GIANT_KNOCK_AWAY   = 18945,  // Molten Giant: the nearest player, 9 yd
        SPELL_SURGE              = 19196,  // Lava Surger: a charge, then everyone within 10 yd of it, 5 yd
        SPELL_HAND_OF_RAGNAROS   = 19780,  // Sulfuron: everyone within 10 yd, 6 yd
        SPELL_PLAY_DEAD          = 19822,  // Core Hound at 0 health, 10 s, takes no damage
        SPELL_ARCANE_EXPLOSION   = 19712,  // Shazzrah: 30 yd round his middle, also right after his Gate
        SPELL_DARK_MENDING       = 19775,  // Flamewaker Priest: 2 s cast, heals a friend for 27,750
        SPELL_MAGMADAR_FRENZY    = 19451,  // an enrage; breathes Lava Breath every 4 s while it lasts
        SPELL_LAVA_BREATH        = 19272,  // a 54 degree cone, 35 yd
        SPELL_CONE_OF_FIRE       = 19630,  // a cone, 20 yd (82 degrees in the spell_cone table)
        SPELL_GEHENNAS_RAIN_OF_FIRE = 19717,  // 10 yd round where it lands
    };

    // The Living Bomb's explosion reaches 18 yd, centre to centre. Its carrier walks to the
    // nearest spot at least SPOT from every other raid member, looked for up to SEARCH yards
    // away, or to one WORTH_IT yards emptier than where it stands when there is none.
    // (KEEP_AWAY is what playerbots' own rule asks for, 20 yd edge to edge: with less, that
    // rule would nudge a bot this one had just put in a good spot.)
    constexpr float LIVING_BOMB_KEEP_AWAY = 23.0f;
    constexpr float LIVING_BOMB_SPOT = 24.5f;
    constexpr float LIVING_BOMB_WORTH_IT = 3.0f;
    constexpr float LIVING_BOMB_SEARCH = 40.0f;

    // Inferno reaches 20 yd, centre to centre. A bot closer than KEEP_AWAY to him walks to the
    // nearest spot at least SPOT from him (looked for up to SEARCH yards round the bot), or to
    // one WORTH_IT yards further out than it stands when there is none.
    constexpr float INFERNO_KEEP_AWAY = 23.0f;
    constexpr float INFERNO_SPOT = 25.0f;
    constexpr float INFERNO_WORTH_IT = 3.0f;
    constexpr float INFERNO_SEARCH = 30.0f;

    // The Hot Coal in Majordomo's room (game object 178164, a trap 10 yd across; its one spawn
    // is where his Teleport lands). Intense Heat reaches 10 yd from it.
    constexpr float HOT_COAL_X = 736.653f;
    constexpr float HOT_COAL_Y = -1176.32f;
    constexpr float HOT_COAL_Z = -118.097f;
    constexpr float HOT_COAL_KEEP_AWAY = 12.0f;
    constexpr float HOT_COAL_SPOT = 13.5f;
    // A tank with a mob on it goes further out: the mob follows and stops a few yards short.
    constexpr float HOT_COAL_TANK_KEEP_AWAY = 20.0f;
    constexpr float HOT_COAL_TANK_SPOT = 22.0f;
    constexpr float HOT_COAL_WORTH_IT = 2.0f;
    constexpr float HOT_COAL_SEARCH = 27.0f;

    // Molten Giant's Smash (18944) reaches 10 yd from its middle, Molten Destroyer's Massive
    // Tremor (19129) 13 yd. Ranged and healers closer than KEEP_AWAY past that edge walk to the
    // nearest spot SPOT past it (looked for up to SEARCH yards round the bot).
    constexpr float SMASH_RADIUS = 10.0f;
    constexpr float MASSIVE_TREMOR_RADIUS = 13.0f;
    constexpr float SPLASH_TRASH_KEEP_AWAY = 2.5f;
    constexpr float SPLASH_TRASH_SPOT = 5.0f;
    constexpr float SPLASH_TRASH_WORTH_IT = 2.0f;
    constexpr float SPLASH_TRASH_SEARCH = 24.0f;

    // Panic (19408) radius.
    constexpr float PANIC_RANGE = 45.0f;
    // Arcane Explosion (19712) reaches 30 yd from Shazzrah's middle, and the core counts a player
    // in it out to its own size past that (1.5 yd). Ranged and healers closer than KEEP_AWAY to his
    // middle walk to the nearest spot at least SPOT from it: 28.5 yd from his edge (he is 4.5 yd
    // wide), playerbots' own spell distance, so they can still cast at him. Within HOLD of him they
    // don't walk toward anything closer to him than KEEP_AWAY.
    constexpr float SHAZZRAH_KEEP_AWAY = 32.5f;
    constexpr float SHAZZRAH_SPOT = 34.5f;
    constexpr float SHAZZRAH_WORTH_IT = 2.0f;
    constexpr float SHAZZRAH_SEARCH = 24.0f;
    constexpr float SHAZZRAH_HOLD = 40.0f;
    // Bots this close to a Flamewaker Priest when it starts Dark Mending are woken to look for an
    // interrupt (Silencing Shot, the farthest, reaches 35 yd).
    constexpr float DARK_MENDING_WAKE_RANGE = 40.0f;
    // Wrath of Ragnaros (20566) reaches 25 yd from his middle. Ranged and healers closer than
    // KEEP_AWAY to his middle walk to the nearest dry spot at least SPOT from it.
    constexpr float RAGNAROS_KEEP_AWAY = 27.5f;
    constexpr float RAGNAROS_SPOT = 29.5f;
    constexpr float RAGNAROS_WORTH_IT = 2.0f;
    constexpr float RAGNAROS_SEARCH = 30.0f;
    // Within this of his middle they don't walk toward anything closer to him than KEEP_AWAY.
    constexpr float RAGNAROS_HOLD = 36.0f;
    // Lava Burst (21908) erupts at three of eight fixed places (Ragnaros' LAVA_BURSTS) every
    // 10 s or so while he is up, and an eruption hits most of the raid around it. Seven of the
    // eight are on the rock 30 to 45 yd from him, the ring KEEP_AWAY sends ranged bots to. So
    // each ranged bot and healer holds a spot of its own on rings this far from his middle,
    // this far apart, as far from all eight as the rock allows; a healer's within this of the
    // main tank, so it can still heal him.
    constexpr std::array<float, 4> RAGNAROS_RINGS = { 30.0f, 34.0f, 38.0f, 42.0f };
    constexpr float RAGNAROS_SPOT_SPACING = 3.5f;
    constexpr float RAGNAROS_HEAL_REACH = 36.0f;
    // Might of Ragnaros (21154) puts a Flame of Ragnaros on a random mana user, and its Intense
    // Heat (21155) hits and throws everyone within 20 yd of it. Spots go out this far apart while
    // there are enough of them no closer to the eruptions than the tight packing above would hand
    // out; only the bots left over get one the tight spacing apart.
    constexpr float RAGNAROS_WIDE_SPOT_SPACING = 7.0f;
    // A bot further than this from its spot walks back to it; one this close holds it.
    constexpr float RAGNAROS_SPOT_LEAVE = 4.0f;
    constexpr float RAGNAROS_SPOT_HOLD = 6.0f;
    // A spot has dry ground this far round it too.
    constexpr float RAGNAROS_SPOT_DRY = 2.5f;
    // Where his main tank stands: this far in front of him as he was summoned. He is rooted and
    // 20 yd wide, so that is in his melee reach. Wrath of Ragnaros throws it out, and once nobody
    // is in his reach he Magma Blasts his target for 6,000 every 4 s: the main tank walks back
    // when it is out of his reach or this far off the spot.
    constexpr float RAGNAROS_TANK_SPOT = 18.0f;
    constexpr float RAGNAROS_TANK_SPOT_LEAVE = 6.0f;
    // A layout nobody has asked for in this long belongs to an earlier pull.
    constexpr uint32 RAGNAROS_LAYOUT_FORGET_MS = 30000;

    // Garr checks his Firesworn once a second: more than 40 yd from him (edge to edge) and it
    // gets Separation Anxiety. A Firesworn this far out counts as having it already.
    constexpr float GARR_ANXIOUS_RANGE = 36.0f;
    // A bot further than this from Garr walks back to the target distance.
    constexpr float GARR_GATHER = 30.0f;
    constexpr float GARR_GATHER_TARGET = 22.0f;
    // A tank's area spells (Consecration, Thunder Clap, a Swipe) leave an anxious Firesworn alone
    // below this much health: it would die to them before it is back with Garr.
    constexpr float GARR_TANK_SPARES_BELOW_PCT = 40.0f;

    // A Core Hound's Play Dead ends in its death only if no other Core Hound within this of it
    // is still standing in the fight; otherwise it gets up at full health.
    constexpr float CORE_HOUND_LINK_RANGE = 80.0f;
    // While none plays dead, a hound more than this many percent of health below the healthiest
    // one is left to area spells and the tanks.
    constexpr float CORE_HOUND_HEALTH_BAND = 10.0f;

    std::vector<KillOrderEntry> const& KillOrder();
    // Adds the kill order leaves alone for now (a Firesworn with Separation Anxiety, a Core Hound
    // playing dead).
    bool KillOrderSkips(Player* bot, Unit const* unit);
    // While Core Hounds are being finished (one plays dead) skull follows the hounds' own rule.
    bool KillOrderSuspended(Player* bot);

    // Bosses where mod-playerbots places the tanks itself (Golemagg: main tank on him, assist
    // tank on the Core Ragers at fixed spots), so the generic off-tank split stays out.
    std::vector<uint32> const& TankSplitSkipBosses();

    // Trash whose AoE needs more room than the default tank separation.
    std::vector<Tanks::SplashRadius> const& TankSplitSplashRadii();
    // Adds tanked in one heap: off-tanks bring theirs to the tank holding the pack.
    std::vector<uint32> const& TankSplitStackAdds();

    // The mobs that throw players about, with the spell they do it with (Common/Knockback.h).
    std::vector<Knockback::Thrower> const& Throwers();

    // Magmadar's Frenzy (Common/Tranquilize.h).
    std::vector<Tranq::Enrage> const& TranqEnrages();

    // Gehennas' Rain of Fire, Magmadar's Lava Bombs, the Ancient Core Hounds' Lava Breath,
    // Magmadar's own and the Flameguards' Cone of Fire (Common/Hazards.h).
    Hazards::Table const& HazardTable();

    void AddScripts();
}

class RaidClearMoltenCoreStrategy : public Strategy
{
public:
    RaidClearMoltenCoreStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    std::string const getName() override { return "rc moltencore"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

// --- Magmadar -------------------------------------------------------------

class RcMcMagmadarTremorTrigger : public Trigger
{
public:
    RcMcMagmadarTremorTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc magmadar tremor") {}
    bool IsActive() override;
};

class RcMcTremorTotemAction : public Action
{
public:
    RcMcTremorTotemAction(PlayerbotAI* botAI) : Action(botAI, "rc mc tremor totem") {}
    bool Execute(Event event) override;
};

// Near Magmadar the earth slot is Tremor Totem's: nothing else is placed in it.
class RcMcTremorMultiplier : public Multiplier
{
public:
    RcMcTremorMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc tremor") {}
    float GetValue(Action* action) override;
};

// --- Shazzrah ------------------------------------------------------------

// A ranged bot or healer inside Shazzrah's Arcane Explosion.
class RcMcShazzrahRangedTrigger : public Trigger
{
public:
    RcMcShazzrahRangedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc shazzrah ranged") {}
    bool IsActive() override;
};

class RcMcLeaveArcaneExplosionAction : public Action
{
public:
    RcMcLeaveArcaneExplosionAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave arcane explosion"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

// Ranged bots and healers near Shazzrah don't walk toward something inside his Arcane Explosion.
class RcMcShazzrahHoldMultiplier : public Multiplier
{
public:
    RcMcShazzrahHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc shazzrah hold") {}
    float GetValue(Action* action) override;
};

// --- Sulfuron Harbinger ---------------------------------------------------

// A Flamewaker Priest is casting Dark Mending, this bot can stop it now and no other bot has just
// sent an interrupt at that priest.
class RcMcDarkMendingTrigger : public Trigger
{
public:
    RcMcDarkMendingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc dark mending") {}
    bool IsActive() override;
};

class RcMcInterruptDarkMendingAction : public Action
{
public:
    RcMcInterruptDarkMendingAction(PlayerbotAI* botAI) : Action(botAI, "rc mc interrupt dark mending") {}
    bool Execute(Event event) override;
};

// --- Baron Geddon ---------------------------------------------------------

// Baron Geddon has Inferno up and the bot is within its reach.
class RcMcInfernoTrigger : public Trigger
{
public:
    RcMcInfernoTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc inferno") {}
    bool IsActive() override;
};

// Not a movement action itself: playerbots' own Geddon rule switches those off (all but its two)
// for as long as Inferno lasts.
class RcMcLeaveInfernoAction : public Action
{
public:
    RcMcLeaveInfernoAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave inferno"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

// The bot carries a Living Bomb and a raid member is within its reach.
class RcMcLivingBombTrigger : public Trigger
{
public:
    RcMcLivingBombTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc living bomb") {}
    bool IsActive() override;
};

// Not a movement action itself, for the same reason as the way out of Inferno: playerbots lets a
// bot with the bomb do no walking but its own.
class RcMcCarryLivingBombAction : public Action
{
public:
    RcMcCarryLivingBombAction(PlayerbotAI* botAI) : Action(botAI, "rc mc carry living bomb"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

// While Inferno lasts nobody walks (or charges) toward something within its reach.
class RcMcInfernoHoldMultiplier : public Multiplier
{
public:
    RcMcInfernoHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc inferno hold") {}
    float GetValue(Action* action) override;
};

// --- Majordomo Executus ---------------------------------------------------

class RcMcDamageReflectionTrigger : public Trigger
{
public:
    RcMcDamageReflectionTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc damage reflection") {}
    bool IsActive() override;
};

class RcMcStopMeleeAction : public Action
{
public:
    RcMcStopMeleeAction(PlayerbotAI* botAI) : Action(botAI, "rc mc stop melee") {}
    bool Execute(Event event) override;
};

class RcMcReflectionMultiplier : public Multiplier
{
public:
    RcMcReflectionMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc reflection") {}
    float GetValue(Action* action) override;
};

// The bot is in combat and has stopped within reach of the Hot Coal.
class RcMcHotCoalTrigger : public Trigger
{
public:
    RcMcHotCoalTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc hot coal") {}
    bool IsActive() override;

private:
    uint32 _lastLookMs = 0;
    bool _lastStands = false;
    bool _lastResult = false;
    Position _lastSpot;
};

// A bot that has just stopped at the Hot Coal starts no spell it has to stand still for.
class RcMcHotCoalHoldMultiplier : public Multiplier
{
public:
    RcMcHotCoalHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc hot coal hold") {}
    float GetValue(Action* action) override;

private:
    uint32 _sinceMs = 0;
};

class RcMcLeaveHotCoalAction : public Action
{
public:
    RcMcLeaveHotCoalAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave hot coal"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

// --- Garr -----------------------------------------------------------------

class RcMcGarrGatherTrigger : public Trigger
{
public:
    RcMcGarrGatherTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc garr gather") {}
    bool IsActive() override;
};

class RcMcGarrGatherAction : public MovementAction
{
public:
    RcMcGarrGatherAction(PlayerbotAI* botAI) : MovementAction(botAI, "rc mc garr gather") {}
    bool Execute(Event event) override;
};

// A bot that isn't a tank is attacking, casting at or channelling into a Firesworn that has
// Separation Anxiety (or a running area spell or chain would land on one), or its pet is on one;
// or a tank is swinging at one that is low.
class RcMcAnxiousFireswornTrigger : public Trigger
{
public:
    RcMcAnxiousFireswornTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc anxious firesworn") {}
    bool IsActive() override;
};

// Stops the swing, the Auto Shot or the wand, the cast or channel, and the pet.
class RcMcLeaveFireswornAction : public Action
{
public:
    RcMcLeaveFireswornAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave firesworn") {}
    bool Execute(Event event) override;
};

// Nobody but the tanks hits a Firesworn that would erupt for four times the damage, and no
// splash or chain spell lands on one.
class RcMcAnxiousFireswornMultiplier : public Multiplier
{
public:
    RcMcAnxiousFireswornMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc anxious firesworn") {}
    float GetValue(Action* action) override;
};

// --- Molten Giants and Destroyers ---------------------------------------------

// A ranged bot or healer stands inside a fighting Giant's Smash or Destroyer's Massive Tremor.
class RcMcSplashTrashTrigger : public Trigger
{
public:
    RcMcSplashTrashTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc splash trash") {}
    bool IsActive() override;
};

class RcMcLeaveSplashTrashAction : public Action
{
public:
    RcMcLeaveSplashTrashAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave splash trash"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

// --- Core Hounds -----------------------------------------------------------

// The bot (or its pet) is hitting a Core Hound that is playing dead or that is held back for the
// rest of the pack to catch up, or is casting or channelling at one that is playing dead.
class RcMcHoundTargetTrigger : public Trigger
{
public:
    RcMcHoundTargetTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc hound target") {}
    bool IsActive() override;
};

class RcMcLeaveHoundAction : public Action
{
public:
    RcMcLeaveHoundAction(PlayerbotAI* botAI) : Action(botAI, "rc mc leave hound") {}
    bool Execute(Event event) override;
};

// The group's marker, with Core Hounds in the fight and skull not where the hounds' rule wants it.
class RcMcHoundSkullTrigger : public Trigger
{
public:
    RcMcHoundSkullTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc hound skull") {}
    bool IsActive() override;
};

class RcMcMarkHoundAction : public Action
{
public:
    RcMcMarkHoundAction(PlayerbotAI* botAI) : Action(botAI, "rc mc mark hound") {}
    bool Execute(Event event) override;
};

// Nothing is aimed at a hound playing dead, and no single-target attack but a tank's at a hound
// held back. (Playerbots' own hound marking stands down by itself: its trigger checks
// RaidClear::Runs.)
class RcMcCoreHoundMultiplier : public Multiplier
{
public:
    RcMcCoreHoundMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc core hound") {}
    float GetValue(Action* action) override;
};

// --- Ragnaros -------------------------------------------------------------

class RcMcRagnarosRangedTrigger : public Trigger
{
public:
    RcMcRagnarosRangedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc ragnaros ranged") {}
    bool IsActive() override;
};

// A ranged bot or healer away from its spot at Ragnaros (see RAGNAROS_RINGS).
class RcMcRagnarosSpotTrigger : public Trigger
{
public:
    RcMcRagnarosSpotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc ragnaros spot") {}
    bool IsActive() override;
};

class RcMcRagnarosSpotAction : public MovementAction
{
public:
    RcMcRagnarosSpotAction(PlayerbotAI* botAI) : MovementAction(botAI, "rc mc ragnaros spot") {}
    bool Execute(Event event) override;
};

// Ranged bots and healers at Ragnaros don't walk toward something inside his Wrath, and one on
// its spot doesn't walk off it.
class RcMcRagnarosHoldMultiplier : public Multiplier
{
public:
    RcMcRagnarosHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "rc mc ragnaros hold") {}
    float GetValue(Action* action) override;
};

// The main tank, with Ragnaros up, out of his reach or off its spot in front of him.
class RcMcRagnarosTankTrigger : public Trigger
{
public:
    RcMcRagnarosTankTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc ragnaros tank") {}
    bool IsActive() override;
};

class RcMcRagnarosTankAction : public MovementAction
{
public:
    RcMcRagnarosTankAction(PlayerbotAI* botAI) : MovementAction(botAI, "rc mc ragnaros tank") {}
    bool Execute(Event event) override;
};

// The main tank is back in his reach and he is on someone else (he picks a random target when he
// comes out of the lava, and anyone in his reach while the tank was thrown out).
class RcMcRagnarosTauntTrigger : public Trigger
{
public:
    RcMcRagnarosTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, "rc mc ragnaros taunt") {}
    bool IsActive() override;
};

class RcMcRagnarosTauntAction : public Action
{
public:
    RcMcRagnarosTauntAction(PlayerbotAI* botAI) : Action(botAI, "rc mc ragnaros taunt") {}
    bool Execute(Event event) override;
};

class RcMcRagnarosMoveOutAction : public Action
{
public:
    RcMcRagnarosMoveOutAction(PlayerbotAI* botAI) : Action(botAI, "rc mc ragnaros move out"), _walk(botAI) {}
    bool Execute(Event event) override;

private:
    RaidClear::WayOutWalk _walk;
};

#endif
