/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_PALADINBLESSINGREGISTRY_H
#define PLAYERBOTS_PALADINBLESSINGREGISTRY_H

#include "EraTalentBridge.h"
#include "PaladinBlessingTypes.h"
#include "Player.h"
#include "SpellMgr.h"
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Blessing spell-id registry: every rank of every blessing keyed by id, plus per-(category,
// variant) ascending rank chains, built once.  Lets callers map an aura's spell id to its
// blessing category / variant, and resolve a bot's highest known rank id straight from the
// chain -- no name lookup, no full spell-map scan.
namespace ai::blessing
{
    struct BlessingSpellInfo
    {
        BaseBlessingCategory category;
        bool                 isGreater;
    };

    static constexpr std::size_t BLESSING_CHAIN_SLOTS = BLESSING_CATEGORY_SLOTS * 2;

    inline std::size_t BlessingChainIndex(BaseBlessingCategory category, bool greater)
    {
        return static_cast<std::size_t>(category) * 2 + (greater ? 1 : 0);
    }

    struct BlessingRegistry
    {
        std::unordered_map<uint32, BlessingSpellInfo> byId;
        // EraTalents clones keep the stock blessing names under their own ids.
        std::unordered_map<std::string, BlessingSpellInfo> byName;
        // slots 0/1 (BASE_NONE) are intentionally unused
        std::array<std::vector<uint32>, BLESSING_CHAIN_SLOTS> chains;
    };

    inline BlessingRegistry const& Blessings()
    {
        static BlessingRegistry const reg = []
        {
            BlessingRegistry registry;
            auto addChain = [&](uint32 firstRankId, BaseBlessingCategory cat, bool greater)
            {
                if (SpellInfo const* first = firstRankId ? sSpellMgr->GetSpellInfo(firstRankId) : nullptr)
                    registry.byName[first->SpellName[LOCALE_enUS]] = { cat, greater };
                for (SpellInfo const* info = firstRankId ? sSpellMgr->GetSpellInfo(firstRankId) : nullptr;
                     info; info = info->GetNextRankSpell())
                {
                    registry.byId[info->Id] = { cat, greater };
                    registry.chains[BlessingChainIndex(cat, greater)].push_back(info->Id);  // ascending
                }
            };
            addChain(SPELL_BLESSING_OF_MIGHT,             BASE_MIGHT,     false);
            addChain(SPELL_GREATER_BLESSING_OF_MIGHT,     BASE_MIGHT,     true);
            addChain(SPELL_BLESSING_OF_WISDOM,            BASE_WISDOM,    false);
            addChain(SPELL_GREATER_BLESSING_OF_WISDOM,    BASE_WISDOM,    true);
            addChain(SPELL_BLESSING_OF_KINGS,             BASE_KINGS,     false);
            addChain(SPELL_GREATER_BLESSING_OF_KINGS,     BASE_KINGS,     true);
            addChain(SPELL_BLESSING_OF_SANCTUARY,         BASE_SANCTUARY, false);
            addChain(SPELL_GREATER_BLESSING_OF_SANCTUARY, BASE_SANCTUARY, true);
            return registry;
        }();
        return reg;
    }

    inline BlessingSpellInfo const* FindBlessing(uint32 spellId)
    {
        BlessingRegistry const& registry = Blessings();
        if (auto it = registry.byId.find(spellId); it != registry.byId.end())
            return &it->second;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || info->SpellFamilyName != SPELLFAMILY_PALADIN || !info->SpellName[LOCALE_enUS])
            return nullptr;
        auto it = registry.byName.find(info->SpellName[LOCALE_enUS]);
        return it == registry.byName.end() ? nullptr : &it->second;
    }

    // Category of any blessing spell rank, or BASE_NONE for an unrecognised spell.
    inline BaseBlessingCategory CategoryOfSpell(uint32 spellId)
    {
        BlessingSpellInfo const* blessing = FindBlessing(spellId);
        return blessing ? blessing->category : BASE_NONE;
    }

    // True if the spell id is a GREATER (class-wide) blessing variant.
    inline bool IsGreaterBlessingSpell(uint32 spellId)
    {
        BlessingSpellInfo const* blessing = FindBlessing(spellId);
        return blessing && blessing->isGreater;
    }

    // The bot's HIGHEST known rank id of (category, greater?), or nullopt if it knows none.
    // Resolves the castable id straight from the known rank chain -- no name lookup / full
    // spell-map scan (SpellIdValue), and no mana-save downgrade (we always want the strongest).
    inline std::optional<uint32> HighestKnownBlessingRank(Player* bot, BaseBlessingCategory category, bool greater)
    {
        if (!bot || category == BASE_NONE)
            return std::nullopt;
        std::optional<uint32> best;
        std::vector<uint32> const& chain = Blessings().chains[BlessingChainIndex(category, greater)];
        for (uint32 id : chain)  // ascending
            if (bot->HasSpell(id))
                best = id;
        // A historical talent blessing is an earned EraTalents clone of the stock spell.
        if (!best && !chain.empty())
            if (uint32 const era = EraKnown(bot, chain.front()))
                best = era;
        return best;
    }
}

#endif
