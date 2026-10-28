/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "LearnTaxiAction.h"

#include "Event.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "WorldPacket.h"

bool LearnTaxiAction::isUseful()
{
    return sPlayerbotAIConfig.LearnFlightPathsWithMaster && botAI->HasGameClientMaster() && bot->IsAlive() &&
           !bot->IsInFlight();
}

bool LearnTaxiAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!sPlayerbotAIConfig.LearnFlightPathsWithMaster || !botAI->HasGameClientMaster() || !bot->IsAlive())
        return false;

    if (!master->IsInWorld() || !bot->IsInWorld() || master->GetMap() != bot->GetMap() || master->IsBeingTeleported() ||
        bot->IsBeingTeleported() || !master->IsAlive())
        return false;

    WorldPacket& packet = event.getPacket();
    if ((packet.GetOpcode() != CMSG_GOSSIP_HELLO && packet.GetOpcode() != CMSG_TAXIQUERYAVAILABLENODES) ||
        packet.size() != 2 * sizeof(uint64))
        return false;

    ObjectGuid flightMasterGuid;
    ObjectGuid packetMasterGuid;
    packet.rpos(0);
    packet >> flightMasterGuid >> packetMasterGuid;
    if (packetMasterGuid != master->GetGUID())
        return false;

    Creature* flightMaster = master->GetNPCIfCanInteractWith(flightMasterGuid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!flightMaster || !flightMaster->IsAlive() ||
        bot->GetNPCIfCanInteractWith(flightMasterGuid, UNIT_NPC_FLAG_FLIGHTMASTER) != flightMaster)
        return false;

    bot->GetSession()->SendLearnNewTaxiNode(flightMaster);
    return true;
}
