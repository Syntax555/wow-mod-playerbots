/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_EARNEDAUCTION_H
#define PLAYERBOTS_EARNEDAUCTION_H

#include "ItemUsageValue.h"
#include "ObjectGuid.h"

class Item;
class Player;
class PlayerbotAI;

namespace EarnedAuction
{
bool IsEnabled(PlayerbotAI* ai);
bool MeetsAuctionLevel(Player* bot);
// Container slots of a bag the bot won or leads a bid on and has not collected yet.
uint32 PendingBagSlots(PlayerbotAI* ai);
bool HasSurplus(PlayerbotAI* ai);
bool HasVendorSurplus(PlayerbotAI* ai);
bool SellsToVendor(PlayerbotAI* ai, Item* item, ItemUsage usage);
bool ShouldReserveForAuction(PlayerbotAI* ai, Item* item, ItemUsage usage);
bool QueueVisit(PlayerbotAI* ai, ObjectGuid target, bool mailbox);
}  // namespace EarnedAuction
#endif
