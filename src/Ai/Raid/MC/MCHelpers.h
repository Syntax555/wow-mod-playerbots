/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MCHELPERS_H
#define PLAYERBOTS_MCHELPERS_H

#include "Define.h"

class Player;
class PlayerbotAI;

namespace MoltenCoreHelpers
{
enum MoltenCoreNPCs
{
    // Golemagg
    NPC_CORE_RAGER = 11672,

    // Majordomo Executus
    NPC_MAJORDOMO_EXECUTUS = 12018,

    // Core Hound (trash)
    NPC_CORE_HOUND = 11671,
};
enum MoltenCoreSpells
{
    SPELL_CORE_HOUND_PLAY_DEAD = 19822,

    // Baron Geddon
    SPELL_INFERNO = 19695,
    SPELL_LIVING_BOMB = 20475,

    // Golemagg
    SPELL_GOLEMAGGS_TRUST = 20553,
    SPELL_MAGMA_SPLASH = 13880,
};

constexpr uint32 CORE_HOUND_RECOVERY_WINDOW = 3000;
constexpr float CORE_HOUND_REKINDLE_RANGE = 80.0f;

constexpr uint32 MAGMA_SPLASH_BACK_OFF_STACKS = 20;
constexpr float MAGMA_SPLASH_BACK_OFF_DISTANCE = 12.0f;
// With raid clear's Molten Core strategy on, melee take turns instead: one backs off from this
// many stacks while fewer than half of the group's melee bots are out, and stays out until its
// stacks are gone (MAGMA_SPLASH_BACK_OFF_STACKS still sends anyone out).
constexpr uint32 MAGMA_SPLASH_ROTATE_STACKS = 8;

// Whether this bot should be away from Golemagg for its Magma Splash stacks to run out.
bool GolemaggBacksOff(PlayerbotAI* botAI, Player* bot);

// Shazzrah's Arcane Explosion radius
constexpr float ARCANE_EXPLOSION_DISTANCE = 26.0f;
}

#endif
