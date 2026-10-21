/*
 * Raid clear: Blackwing Lair, fire on the ground and cleaves in the trash fights (Common/Hazards.h).
 * See BwlRaidClear.h.
 *
 * Released under the MIT License.
 */

#include "BwlRaidClear.h"

using namespace RaidClear::BlackwingLair;

RaidClear::Hazards::Table const& RaidClear::BlackwingLair::HazardTable()
{
    // The Warlocks' Rain of Fire, the Spellbinders' and Hatchers' Flamestrike; a Captain's or
    // Overseer's Cleave.
    static Hazards::Table const table = {
        {
            { NPC_BLACKWING_WARLOCK, SPELL_BWL_RAIN_OF_FIRE },
            { NPC_BLACKWING_SPELLBINDER, SPELL_BWL_FLAMESTRIKE },
            { NPC_DEATH_TALON_HATCHER, SPELL_BWL_FLAMESTRIKE },
        },
        {},
        {
            { NPC_DEATH_TALON_CAPTAIN, CLEAVE_REACH, CLEAVE_HALF_ANGLE },
            { NPC_DEATH_TALON_OVERSEER, CLEAVE_REACH, CLEAVE_HALF_ANGLE },
        },
        {},
    };
    return table;
}
