/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_CONTINENTROUTEPOLICY_H
#define PLAYERBOTS_CONTINENTROUTEPOLICY_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

// Routes between maps over links an ordinary player can take: ships, zeppelins, portals,
// teleporting area triggers and the bot's own teleport spells or hearthstone.
namespace ContinentRoutes
{
constexpr float RunSpeed = 7.0f;
constexpr float WalkDetour = 1.3f;  // walked routes are longer than the straight line
constexpr float FlightSpeed = 30.0f;
constexpr float FlightDetour = 1.25f;
constexpr float FlightOverheadSeconds = 20.0f;
constexpr float FlightMinimumYards = 600.0f;
constexpr float PortalSeconds = 5.0f;
constexpr float CastSeconds = 10.0f;
// A reagent or a long cooldown is spent only when it saves clearly more than walking.
constexpr float SpellPenaltySeconds = 60.0f;
constexpr float HearthstonePenaltySeconds = 180.0f;
constexpr float NearBestFactor = 1.25f;
constexpr float NearBestSeconds = 60.0f;

// AreaTable team values (DBCEnums.h AreaTeams).
constexpr std::uint32_t AreaTeamAlliance = 2;
constexpr std::uint32_t AreaTeamHorde = 4;

enum class LinkKind : std::uint8_t
{
    Transport,
    Portal,
    AreaTrigger,
    Spell,
    Hearthstone
};

struct Point
{
    std::uint32_t map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Link
{
    LinkKind kind = LinkKind::Portal;
    std::uint32_t object = 0;  // transport entry, portal spawn, trigger, spell or item
    Point from;                // ignored for links usable anywhere
    Point to;
    float seconds = 0.0f;  // expected wait and travel once the bot stands at from
    bool anywhere = false;
};

inline float Distance(Point const& a, Point const& b)
{
    float const dx = a.x - b.x;
    float const dy = a.y - b.y;
    float const dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float WalkSeconds(Point const& a, Point const& b)
{
    return a.map == b.map ? Distance(a, b) * WalkDetour / RunSpeed : std::numeric_limits<float>::infinity();
}

// A transport waits half a cycle on average, then rides from its departure to the next stop.
inline float TransportSeconds(std::uint32_t departureMs, std::uint32_t arrivalMs, std::uint32_t pathMs)
{
    if (!pathMs)
        return std::numeric_limits<float>::infinity();

    std::uint32_t const ride = (arrivalMs + pathMs - departureMs % pathMs) % pathMs;
    return (ride + pathMs / 2.0f) / 1000.0f;
}

// Hostile territory at either end makes a link unusable; contested and neutral areas are open.
inline bool TeamMayUse(std::uint32_t departureAreaTeam, std::uint32_t arrivalAreaTeam, bool alliance)
{
    std::uint32_t const hostile = alliance ? AreaTeamHorde : AreaTeamAlliance;
    return departureAreaTeam != hostile && arrivalAreaTeam != hostile;
}

inline bool FlightSaves(float directYards, float toFlightMasterYards, float flightYards, float landingToTargetYards)
{
    if (!(directYards >= FlightMinimumYards))
        return false;

    float const walk = directYards * WalkDetour / RunSpeed;
    float const fly = (toFlightMasterYards + landingToTargetYards) * WalkDetour / RunSpeed +
                      flightYards * FlightDetour / FlightSpeed + FlightOverheadSeconds;
    return fly < walk;
}

constexpr std::size_t NoNode = std::numeric_limits<std::size_t>::max();

// Node 0 is the start; node i + 1 is the arrival of link i.
struct Search
{
    Point start;
    std::vector<float> cost;
    std::vector<std::size_t> previous;
};

struct Route
{
    std::vector<std::size_t> links;  // in travel order; empty when walking directly is shortest
    float seconds = std::numeric_limits<float>::infinity();
    bool Found() const { return std::isfinite(seconds); }
};

inline Point Arrival(std::span<Link const> links, Search const& search, std::size_t node)
{
    return node ? links[node - 1].to : search.start;
}

template <typename Usable>
Search Explore(std::span<Link const> links, Point const& start, Usable usable)
{
    std::size_t const nodes = links.size() + 1;
    Search search{start, std::vector<float>(nodes, std::numeric_limits<float>::infinity()),
                  std::vector<std::size_t>(nodes, NoNode)};
    std::vector<bool> allowed(links.size());
    for (std::size_t i = 0; i < links.size(); ++i)
        allowed[i] = usable(links[i]);

    std::vector<bool> settled(nodes);
    search.cost[0] = 0.0f;
    while (true)
    {
        std::size_t node = NoNode;
        for (std::size_t i = 0; i < nodes; ++i)
            if (!settled[i] && std::isfinite(search.cost[i]) && (node == NoNode || search.cost[i] < search.cost[node]))
                node = i;

        if (node == NoNode)
            break;

        settled[node] = true;
        Point const at = Arrival(links, search, node);
        for (std::size_t i = 0; i < links.size(); ++i)
        {
            if (!allowed[i] || settled[i + 1])
                continue;

            float const leg = links[i].anywhere ? 0.0f : WalkSeconds(at, links[i].from);
            float const cost = search.cost[node] + leg + links[i].seconds;
            if (cost < search.cost[i + 1])
            {
                search.cost[i + 1] = cost;
                search.previous[i + 1] = node;
            }
        }
    }

    return search;
}

inline Route RouteTo(std::span<Link const> links, Search const& search, Point const& goal)
{
    Route route;
    route.seconds = WalkSeconds(search.start, goal);
    std::size_t last = 0;
    for (std::size_t node = 1; node < search.cost.size(); ++node)
    {
        float const total = search.cost[node] + WalkSeconds(links[node - 1].to, goal);
        if (total < route.seconds)
        {
            route.seconds = total;
            last = node;
        }
    }

    for (std::size_t node = last; node && node != NoNode; node = search.previous[node])
        route.links.push_back(node - 1);
    std::reverse(route.links.begin(), route.links.end());
    return route;
}

// Destinations whose route is close to the shortest one, so bots spread over equivalent choices.
inline std::vector<std::size_t> NearBest(std::span<float const> seconds)
{
    std::vector<std::size_t> chosen;
    float const best = seconds.empty() ? 0.0f : *std::min_element(seconds.begin(), seconds.end());
    if (!std::isfinite(best))
        return chosen;

    for (std::size_t i = 0; i < seconds.size(); ++i)
        if (seconds[i] <= best * NearBestFactor + NearBestSeconds)
            chosen.push_back(i);
    return chosen;
}
}  // namespace ContinentRoutes

#endif
