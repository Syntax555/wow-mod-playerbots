/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_CONTINENTROUTES_H
#define PLAYERBOTS_CONTINENTROUTES_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ContinentRoutePolicy.h"
#include "Define.h"
#include "ObjectGuid.h"
#include "TravelMgr.h"

class MotionTransport;
class Player;
class Transport;

// A bot's way to a destination on another map. It outlives the far-teleport AI reset.
struct ContinentRouteState
{
    WorldPosition destination;
    uint32 startedAt = 0;
    uint32 noRouteAt = 0;
    bool forMaster = false;  // a companion's way to its master, not an RPG goal

    bool hasLink = false;
    ContinentRoutes::Link link;
    uint32 linkMap = 0;
    uint32 plannedAt = 0;
    uint32 usedAt = 0;  // when the bot entered, used, cast or boarded the link
    uint32 attempts = 0;
    WorldPosition dock;  // pier or tower point where the bot waits for its transport

    std::vector<uint32> flightPath;
    uint32 flightMasterEntry = 0;
    WorldPosition flightMasterPos;
    WorldPosition flightTarget;
    uint32 flightCheckedAt = 0;

    // Links whose use failed, by key, with the time of failure.
    std::unordered_map<uint64, uint32> failedAt;

    bool Active() const { return destination != WorldPosition(); }
    // A moving target, such as a master, keeps the link in progress while it stays nearby.
    void Start(WorldPosition const& target, bool master);
    void Finish();
    void ClearLink();
    void ClearFlight();
};

class ContinentRouteCatalog
{
public:
    static ContinentRouteCatalog& instance()
    {
        static ContinentRouteCatalog catalog;
        return catalog;
    }

    // World thread, after transports and world objects are loaded.
    void Build();

    bool Connects(uint32 mapId) const { return _maps.contains(mapId); }

    // Links the bot may take now: open to its faction, era and expansion, not failed recently,
    // and its own teleport spells and hearthstone when it can cast them.
    std::vector<ContinentRoutes::Link> UsableLinks(Player* bot, ContinentRouteState const& state) const;

    static uint64 Key(ContinentRoutes::Link const& link);

private:
    struct Entry
    {
        ContinentRoutes::Link link;
        uint8 teams = 0;  // 1 Alliance, 2 Horde
        uint32 arrivalZone = 0;
    };

    struct TeleportSpell
    {
        uint32 spellId = 0;
        ContinentRoutes::Point to;
        uint32 arrivalZone = 0;
    };

    void Add(ContinentRoutes::Link const& link, bool alliance, bool horde);
    static bool CanArrive(Player* bot, ContinentRoutes::Point const& to, uint32 arrivalZone);

    std::vector<Entry> _entries;
    std::vector<TeleportSpell> _teleportSpells;
    std::unordered_set<uint32> _maps;
};

#define sContinentRoutes ContinentRouteCatalog::instance()

namespace ContinentTravel
{
constexpr uint32 LinkRetryMs = 30 * MINUTE * IN_MILLISECONDS;
constexpr uint32 UseTimeoutMs = 20 * IN_MILLISECONDS;
constexpr uint32 BoardingTimeoutMs = 15 * IN_MILLISECONDS;
constexpr uint32 BoardingAttempts = 4;
constexpr uint32 RouteTimeoutMs = 2 * HOUR * IN_MILLISECONDS;
constexpr uint32 LegTimeoutMs = 20 * MINUTE * IN_MILLISECONDS;
constexpr uint32 ReplanMs = 30 * IN_MILLISECONDS;
constexpr uint32 NoRouteRetryMs = MINUTE * IN_MILLISECONDS;
constexpr uint32 FlightCheckMs = MINUTE * IN_MILLISECONDS;
constexpr float TargetMovedYards = 200.0f;
constexpr float CompanionFarYards = 1500.0f;
constexpr float ArrivalRadius = 60.0f;
constexpr float DockSearchRadius = 40.0f;
constexpr float DockedRadius = 12.0f;
constexpr float NearStopRadius = 120.0f;

ContinentRoutes::Point ToPoint(WorldPosition const& position);
WorldPosition ToPosition(ContinentRoutes::Point const& point);

// Whether the transport waits at stop, or at any of its stops when stop is null.
bool IsDocked(MotionTransport const* transport, ContinentRoutes::Point const* stop);
// Dry ground near a transport stop that the bot can walk to, where it waits for the transport.
bool FindDock(Player* bot, ContinentRoutes::Point const& stop, WorldPosition& dock);
// A deck point of the docked transport close to the bot.
bool FindBoardingPoint(Player* bot, Transport* transport, WorldPosition& point);
// Dry ground off the transport close to the bot, at about deck height.
bool FindLandingPoint(Player* bot, Transport* transport, WorldPosition& point);
}  // namespace ContinentTravel

#endif
