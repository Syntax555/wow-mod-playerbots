/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_ERATALENTBRIDGE_H
#define PLAYERBOTS_ERATALENTBRIDGE_H

#include "Define.h"

class Player;

// Implemented by mod-era-talents (EraTalentBots.h); keep these signatures identical.
uint32 EraTalentBots_ResolveSpellId(Player* bot, uint32 stockSpellId);
bool EraTalentBots_SpecTabs(Player* bot, uint32* tabs3);
bool EraTalentBots_FactoryReconcile(Player* bot, int specTab);

// Resolve only known stock spells or their earned historical variants.
inline uint32 EraKnown(Player* bot, uint32 spellId) { return EraTalentBots_ResolveSpellId(bot, spellId); }

#endif
