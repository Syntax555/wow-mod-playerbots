/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BindHomeAction.h"

#include "Event.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "WorldPacket.h"

bool BindHomeAction::isUseful()
{
    return sPlayerbotAIConfig.BindHomeWithMaster && botAI->HasGameClientMaster() && bot->IsAlive();
}

bool BindHomeAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!isUseful() || !master->IsInWorld() || !bot->IsInWorld() || master->GetMap() != bot->GetMap() ||
        master->IsBeingTeleported() || bot->IsBeingTeleported() || !master->IsAlive())
        return false;

    WorldPacket& packet = event.getPacket();
    if (packet.GetOpcode() != CMSG_BINDER_ACTIVATE || packet.size() != 2 * sizeof(uint64))
        return false;

    ObjectGuid innkeeperGuid;
    ObjectGuid packetMasterGuid;
    packet.rpos(0);
    packet >> innkeeperGuid >> packetMasterGuid;
    if (packetMasterGuid != master->GetGUID())
        return false;

    // The bot speaks to the same innkeeper itself, as each player of a group binds on its own.
    Creature* innkeeper = master->GetNPCIfCanInteractWith(innkeeperGuid, UNIT_NPC_FLAG_INNKEEPER);
    if (!innkeeper || bot->GetNPCIfCanInteractWith(innkeeperGuid, UNIT_NPC_FLAG_INNKEEPER) != innkeeper)
        return false;

    bot->GetSession()->SendBindPoint(innkeeper);
    botAI->TellMaster(
        PlayerbotTextMgr::instance().GetBotTextOrDefault("set_home_success", "This inn is my new home", {}));
    return true;
}
