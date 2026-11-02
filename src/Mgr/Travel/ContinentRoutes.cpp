/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ContinentRoutes.h"

#include <array>
#include <cmath>
#include <limits>

#include "AreaDefines.h"
#include "ContinentRouteProgression.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GameObjectModel.h"
#include "Map.h"
#include "MapMgr.h"
#include "ModelIgnoreFlags.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotsDatabase.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Transport.h"
#include "TransportMgr.h"
#include "TravelNode.h"
#include "WorldSession.h"

using ContinentRoutes::Link;
using ContinentRoutes::LinkKind;
using ContinentRoutes::Point;

namespace
{
constexpr uint32 ITEM_HEARTHSTONE = 6948;
constexpr uint32 SPELL_HEARTHSTONE = 8690;
constexpr float SameMapPortalMinimumYards = 500.0f;

uint32 ZoneOf(uint32 areaId)
{
    AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId);
    return area ? (area->zone ? area->zone : area->ID) : 0;
}

// The area's own team, else its zone's: Grom'gol is Horde inside contested Stranglethorn.
uint32 AreaTeam(uint32 areaId)
{
    AreaTableEntry const* area = sAreaTableStore.LookupEntry(areaId);
    if (!area)
        return 0;

    if (area->team || !area->zone)
        return area->team;

    AreaTableEntry const* zone = sAreaTableStore.LookupEntry(area->zone);
    return zone ? zone->team : 0;
}

uint32 AreaAt(Point const& point)
{
    Map* map = sMapMgr->CreateBaseMap(point.map);
    return map ? map->GetAreaId(PHASEMASK_NORMAL, point.x, point.y, point.z) : 0;
}

// Continents and the other non-instanced maps between them, such as the Deeprun Tram.
bool IsWorldMap(uint32 mapId)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    return entry && entry->IsWorldMap();
}

uint64 IslandCell(uint32 mapId, int32 cellX, int32 cellY)
{
    return (uint64(mapId) << 32) | (uint64(uint16(int16(cellX))) << 16) | uint16(int16(cellY));
}

// A portal spell may teleport directly or through the spell it triggers.
bool TeleportDestination(uint32 spellId, Point& to, bool triggered = false)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (info->Effects[i].Effect == SPELL_EFFECT_TELEPORT_UNITS)
        {
            if (SpellTargetPosition const* position = sSpellMgr->GetSpellTargetPosition(spellId, SpellEffIndex(i)))
            {
                to = {position->target_mapId, position->target_X, position->target_Y, position->target_Z};
                return true;
            }
        }
        else if (!triggered && info->Effects[i].Effect == SPELL_EFFECT_TRIGGER_SPELL &&
                 TeleportDestination(info->Effects[i].TriggerSpell, to, true))
            return true;
    }

    return false;
}

bool HasReagents(Player* bot, SpellInfo const* info)
{
    if (bot->CanNoReagentCast(info))
        return true;

    for (uint8 i = 0; i < MAX_SPELL_REAGENTS; ++i)
        if (info->Reagent[i] > 0 && info->ReagentCount[i] > 0 &&
            !bot->HasItemCount(uint32(info->Reagent[i]), info->ReagentCount[i]))
            return false;

    return true;
}

bool Open(ContinentRouteState const& state, Link const& link)
{
    auto const failed = state.failedAt.find(ContinentRouteCatalog::Key(link));
    return failed == state.failedAt.end() || GetMSTimeDiffToNow(failed->second) >= ContinentTravel::LinkRetryMs;
}

}  // namespace

void ContinentRouteState::Start(WorldPosition const& target, bool master)
{
    bool const moved = !Active() || forMaster != master || destination.GetMapId() != target.GetMapId() ||
                       destination.GetExactDist(target) > ContinentTravel::TargetMovedYards;
    if (moved)
    {
        noRouteAt = 0;
        ClearLink();
        ClearFlight();
    }
    if (!Active() || forMaster != master)
        startedAt = getMSTime();
    destination = target;
    forMaster = master;
}

void ContinentRouteState::Finish()
{
    destination = WorldPosition();
    startedAt = 0;
    noRouteAt = 0;
    forMaster = false;
    ClearLink();
    ClearFlight();
}

void ContinentRouteState::ClearLink()
{
    hasLink = false;
    link = Link();
    linkMap = 0;
    plannedAt = 0;
    usedAt = 0;
    attempts = 0;
    dock = WorldPosition();
}

void ContinentRouteState::ClearFlight()
{
    flightPath.clear();
    flightMasterEntry = 0;
    flightMasterPos = WorldPosition();
    flightTarget = WorldPosition();
}

uint64 ContinentRouteCatalog::Key(Link const& link)
{
    return (uint64(link.kind) << 56) | (uint64(link.anywhere ? 0 : link.from.map) << 32) | link.object;
}

void ContinentRouteCatalog::Add(Link const& added, bool alliance, bool horde)
{
    Link link = added;
    if (!link.anywhere && !link.from.island)
        link.from.island = IslandAt(link.from.map, link.from.x, link.from.y, link.from.z);
    if (!link.to.island)
        link.to.island = IslandAt(link.to.map, link.to.x, link.to.y, link.to.z);

    uint32 const fromTeam = link.anywhere ? 0 : AreaTeam(AreaAt(link.from));
    uint32 const arrivalArea = AreaAt(link.to);
    uint32 const toTeam = AreaTeam(arrivalArea);
    uint8 teams = 0;
    if (alliance && ContinentRoutes::TeamMayUse(fromTeam, toTeam, true))
        teams |= 1;
    if (horde && ContinentRoutes::TeamMayUse(fromTeam, toTeam, false))
        teams |= 2;
    if (!teams)
        return;

    _entries.push_back({link, teams, ZoneOf(arrivalArea)});
    if (!link.anywhere)
        _maps.insert(link.from.map);
    _maps.insert(link.to.map);
}

void ContinentRouteCatalog::LoadTravelNodes(TravelNodeData& data)
{
    if (PreparedQueryResult result =
            PlayerbotsDatabase.Query(PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_TRAVELNODE)))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 const mapId = fields[2].Get<uint32>();
            if (IsWorldMap(mapId))
                data.nodes.push_back({fields[0].Get<uint32>(),
                                      {mapId, fields[3].Get<float>(), fields[4].Get<float>(), fields[5].Get<float>()}});
        } while (result->NextRow());
    }

    if (PreparedQueryResult result =
            PlayerbotsDatabase.Query(PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_TRAVELNODE_LINK)))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const type = fields[2].Get<uint8>();
            if (type == uint8(TravelNodePathType::walk))
                data.walks.emplace_back(fields[0].Get<uint32>(), fields[1].Get<uint32>());
            else if (type == uint8(TravelNodePathType::transport))
                data.lifts.push_back({fields[0].Get<uint32>(), fields[1].Get<uint32>(), uint32(fields[3].Get<uint64>()),
                                      fields[6].Get<float>()});
        } while (result->NextRow());
    }
}

void ContinentRouteCatalog::Build()
{
    _entries.clear();
    _teleportSpells.clear();
    _maps.clear();
    _islandCells.clear();

    uint32 transports = 0;
    uint32 portals = 0;
    uint32 triggers = 0;
    uint32 lifts = 0;

    // Islands are the areas the travel data joins by walking: Teldrassil above Rut'theran,
    // Thunder Bluff's mesas above Mulgore, either end of the Deeprun Tram.
    TravelNodeData data;
    LoadTravelNodes(data);
    std::vector<uint32> const islands = ContinentRoutes::Islands(data.nodes, data.walks);
    std::unordered_map<uint32, Point> nodes;
    for (std::size_t i = 0; i < data.nodes.size(); ++i)
    {
        Point at = data.nodes[i].at;
        at.island = islands[i];
        nodes[data.nodes[i].id] = at;
        _islandCells[IslandCell(at.map, int32(std::floor(at.x / ContinentTravel::IslandCellSize)),
                                int32(std::floor(at.y / ContinentTravel::IslandCellSize)))]
            .push_back({at.x, at.y, at.z, at.island});
    }

    for (auto const& [entry, goInfo] : *sObjectMgr->GetGameObjectTemplates())
    {
        if (goInfo.type != GAMEOBJECT_TYPE_MO_TRANSPORT)
            continue;

        TransportTemplate const* path = sTransportMgr->GetTransportTemplate(entry);
        if (!path || path->inInstance || !path->pathTime)
            continue;

        std::vector<KeyFrame const*> stops;
        for (KeyFrame const& frame : path->keyFrames)
            if (frame.IsStopFrame())
                stops.push_back(&frame);

        for (std::size_t i = 0; i < stops.size(); ++i)
        {
            KeyFrame const* departure = stops[i];
            KeyFrame const* arrival = stops[(i + 1) % stops.size()];
            if (departure == arrival || !IsWorldMap(departure->Node->mapid) || !IsWorldMap(arrival->Node->mapid))
                continue;

            Link link;
            link.kind = LinkKind::Transport;
            link.object = entry;
            link.from = {departure->Node->mapid, departure->Node->x, departure->Node->y, departure->Node->z};
            link.to = {arrival->Node->mapid, arrival->Node->x, arrival->Node->y, arrival->Node->z};
            link.seconds =
                ContinentRoutes::TransportSeconds(departure->DepartureTime, arrival->ArriveTime, path->pathTime);
            Add(link, true, true);
            ++transports;
        }
    }

    std::unordered_map<uint32, std::vector<Point>> liftSpawns;
    for (auto const& [spawnId, spawn] : sObjectMgr->GetAllGOData())
    {
        if (!IsWorldMap(spawn.mapid) || !(spawn.phaseMask & PHASEMASK_NORMAL))
            continue;

        GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(spawn.id);
        if (!goInfo)
            continue;

        if (goInfo->type == GAMEOBJECT_TYPE_TRANSPORT)
            liftSpawns[spawn.id].push_back({spawn.mapid, spawn.posX, spawn.posY, spawn.posZ});

        uint32 const spellId = goInfo->type == GAMEOBJECT_TYPE_SPELLCASTER ? goInfo->spellcaster.spellId
                               : goInfo->type == GAMEOBJECT_TYPE_GOOBER    ? goInfo->goober.spellId
                                                                           : 0;
        Link link;
        link.kind = LinkKind::Portal;
        link.object = spawnId;
        link.from = {spawn.mapid, spawn.posX, spawn.posY, spawn.posZ};
        link.seconds = ContinentRoutes::PortalSeconds;
        if (!spellId || !TeleportDestination(spellId, link.to) || !IsWorldMap(link.to.map) ||
            (link.to.map == link.from.map && ContinentRoutes::Distance(link.from, link.to) < SameMapPortalMinimumYards))
            continue;

        bool alliance = true;
        bool horde = true;
        if (GameObjectTemplateAddon const* addon = sObjectMgr->GetGameObjectTemplateAddon(spawn.id))
            if (FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(addon->faction))
            {
                alliance = !(faction->hostileMask & FACTION_MASK_ALLIANCE);
                horde = !(faction->hostileMask & FACTION_MASK_HORDE);
            }

        Add(link, alliance, horde);
        ++portals;
    }

    for (uint32 id = 1; id <= std::numeric_limits<uint16>::max(); ++id)
    {
        AreaTriggerTeleport const* teleport = sObjectMgr->GetAreaTriggerTeleport(id);
        AreaTrigger const* trigger = teleport ? sObjectMgr->GetAreaTrigger(id) : nullptr;
        if (!trigger || trigger->map == teleport->target_mapId || !IsWorldMap(trigger->map) ||
            !IsWorldMap(teleport->target_mapId))
            continue;

        Link link;
        link.kind = LinkKind::AreaTrigger;
        link.object = id;
        link.from = {trigger->map, trigger->x, trigger->y, trigger->z};
        link.to = {teleport->target_mapId, teleport->target_X, teleport->target_Y, teleport->target_Z};
        link.seconds = ContinentRoutes::PortalSeconds;
        Add(link, true, true);
        ++triggers;
    }

    // Elevators and trams run back and forth between two stops of the travel data; only static
    // transports spawned at one of the stops count, as the travel data also lists ships.
    for (LiftSource const& lift : data.lifts)
    {
        auto const from = nodes.find(lift.from);
        auto const to = nodes.find(lift.to);
        auto const spawns = liftSpawns.find(lift.entry);
        if (from == nodes.end() || to == nodes.end() || spawns == liftSpawns.end() ||
            from->second.map != to->second.map ||
            ContinentRoutes::Distance(from->second, to->second) < ContinentRoutes::MinimumLiftYards)
            continue;

        bool spawned = false;
        for (Point const& spawn : spawns->second)
            for (Point const& stop : {from->second, to->second})
                spawned = spawned || (spawn.map == stop.map && std::hypot(spawn.x - stop.x, spawn.y - stop.y) <=
                                                                   ContinentTravel::LiftSpawnYards);
        if (!spawned)
            continue;

        Link link;
        link.kind = LinkKind::Lift;
        link.object = lift.entry;
        link.from = from->second;
        link.to = to->second;
        link.seconds = lift.seconds + ContinentRoutes::LiftWaitSeconds;
        Add(link, true, true);
        ++lifts;
    }

    // Trainer-taught teleports with a fixed destination, such as a mage's city teleports.
    for (uint32 spellId = 1; spellId < sSpellMgr->GetSpellInfoStoreSize(); ++spellId)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info || info->SpellFamilyName == SPELLFAMILY_GENERIC || info->IsPassive())
            continue;

        bool teleports = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            teleports = teleports || info->Effects[i].Effect == SPELL_EFFECT_TELEPORT_UNITS;

        SkillLineAbilityMapBounds const abilities = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        Point to;
        if (!teleports || abilities.first == abilities.second || !TeleportDestination(spellId, to) ||
            !IsWorldMap(to.map))
            continue;

        to.island = IslandAt(to.map, to.x, to.y, to.z);
        _teleportSpells.push_back({spellId, to, ZoneOf(AreaAt(to))});
    }

    _built = true;
    LOG_INFO("playerbots",
             ">> Continent routes: {} transport, {} portal, {} area trigger and {} lift links, {} teleport spells, "
             "{} islands.",
             transports, portals, triggers, lifts, _teleportSpells.size(),
             islands.empty() ? 0 : *std::max_element(islands.begin(), islands.end()));
}

uint32 ContinentRouteCatalog::IslandAt(uint32 mapId, float x, float y, float z) const
{
    int32 const cellX = int32(std::floor(x / ContinentTravel::IslandCellSize));
    int32 const cellY = int32(std::floor(y / ContinentTravel::IslandCellSize));
    uint32 island = 0;
    float nearest = ContinentTravel::IslandNodeRadius * ContinentTravel::IslandNodeRadius;
    for (int32 dx = -1; dx <= 1; ++dx)
        for (int32 dy = -1; dy <= 1; ++dy)
        {
            auto const cell = _islandCells.find(IslandCell(mapId, cellX + dx, cellY + dy));
            if (cell == _islandCells.end())
                continue;

            for (IslandNode const& node : cell->second)
            {
                float const distance =
                    (node.x - x) * (node.x - x) + (node.y - y) * (node.y - y) + (node.z - z) * (node.z - z);
                if (distance <= nearest)
                {
                    nearest = distance;
                    island = node.island;
                }
            }
        }
    return island;
}

Point ContinentRouteCatalog::Locate(WorldPosition const& position) const
{
    Point point = ContinentTravel::ToPoint(position);
    point.island = IslandAt(point.map, point.x, point.y, point.z);
    return point;
}

bool ContinentRouteCatalog::CanArrive(Player* bot, Point const& to, uint32 arrivalZone)
{
    return ContinentRouteProgression::CanArrive(bot, to.map, to.x, to.y, to.z, arrivalZone);
}

std::vector<Link> ContinentRouteCatalog::UsableLinks(Player* bot, ContinentRouteState const& state) const
{
    std::vector<Link> links;
    uint8 const team = bot->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
    for (Entry const& entry : _entries)
        if ((entry.teams & team) && Open(state, entry.link) && CanArrive(bot, entry.link.to, entry.arrivalZone))
            links.push_back(entry.link);

    for (TeleportSpell const& spell : _teleportSpells)
    {
        if (!bot->HasSpell(spell.spellId) || bot->HasSpellCooldown(spell.spellId))
            continue;

        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell.spellId);
        Link link;
        link.kind = LinkKind::Spell;
        link.object = spell.spellId;
        link.to = spell.to;
        link.seconds = ContinentRoutes::CastSeconds + ContinentRoutes::SpellPenaltySeconds;
        link.anywhere = true;
        if (info && HasReagents(bot, info) && Open(state, link) && CanArrive(bot, link.to, spell.arrivalZone))
            links.push_back(link);
    }

    if (bot->HasItemCount(ITEM_HEARTHSTONE) && !bot->HasSpellCooldown(SPELL_HEARTHSTONE))
    {
        Link link;
        link.kind = LinkKind::Hearthstone;
        link.object = ITEM_HEARTHSTONE;
        link.to = {bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ};
        link.to.island = IslandAt(link.to.map, link.to.x, link.to.y, link.to.z);
        link.seconds = ContinentRoutes::CastSeconds + ContinentRoutes::HearthstonePenaltySeconds;
        link.anywhere = true;
        if (Open(state, link) && CanArrive(bot, link.to, ZoneOf(bot->m_homebindAreaId)))
            links.push_back(link);
    }

    return links;
}

namespace ContinentTravel
{
Point ToPoint(WorldPosition const& position)
{
    return {position.GetMapId(), position.GetPositionX(), position.GetPositionY(), position.GetPositionZ()};
}

WorldPosition ToPosition(Point const& point) { return WorldPosition(point.map, point.x, point.y, point.z); }

bool OnTransport(GameObject const* transport, uint32 phaseMask, float x, float y, float z)
{
    if (!transport || !transport->m_model)
        return false;

    G3D::Ray const ray(G3D::Vector3(x, y, z + 2.0f), G3D::Vector3(0.0f, 0.0f, -1.0f));
    float distance = 4.0f;
    return transport->m_model->intersectRay(ray, distance, false, phaseMask, VMAP::ModelIgnoreFlags::Nothing);
}

GameObject* NearestTransport(Player* bot, uint32 entry, float range)
{
    // Ships and zeppelins move between grids and are kept apart from them.
    GameObject* nearest = nullptr;
    float distance = range;
    for (Transport* transport : bot->GetMap()->GetAllTransports())
        if (transport->GetEntry() == entry && bot->GetExactDist2d(transport) <= distance)
        {
            nearest = transport;
            distance = bot->GetExactDist2d(transport);
        }
    return nearest ? nearest : bot->FindNearestGameObject(entry, range);
}

Transport* LiftUnder(Player* bot, uint32 entry)
{
    uint32 const phase = bot->GetPhaseMask();
    if (Transport* current = bot->GetTransport())
        if (!current->ToMotionTransport() &&
            OnTransport(current, phase, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ()))
            return current;

    if (!entry)
        return nullptr;

    GameObject* lift = bot->FindNearestGameObject(entry, NearStopRadius);
    Transport* transport = lift ? lift->ToTransport() : nullptr;
    return transport && OnTransport(transport, phase, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ())
               ? transport
               : nullptr;
}

namespace
{
constexpr float DeckProbeStep = 1.0f;
constexpr float DeckHeightTolerance = 15.0f;
constexpr uint32 DockPathChecks = 4;

// Dry ground within range of center at about height z and off the excluded transport, nearest first.
std::vector<WorldPosition> DryGround(Player* bot, float centerX, float centerY, float z, float minimum, float range,
                                     GameObject const* exclude)
{
    Map* map = bot->GetMap();
    std::vector<WorldPosition> points;
    for (float radius = minimum; radius <= range; radius += 3.0f)
    {
        for (uint32 step = 0; step < 24; ++step)
        {
            float const angle = step * float(M_PI) / 12.0f;
            float const x = centerX + std::cos(angle) * radius;
            float const y = centerY + std::sin(angle) * radius;
            float const ground = map->GetHeight(bot->GetPhaseMask(), x, y, z + 5.0f, true, DeckHeightTolerance + 5.0f);
            if (ground <= INVALID_HEIGHT || std::fabs(ground - z) > DeckHeightTolerance ||
                map->IsInWater(bot->GetPhaseMask(), x, y, ground, bot->GetCollisionHeight()) ||
                OnTransport(exclude, bot->GetPhaseMask(), x, y, ground))
                continue;

            points.push_back(WorldPosition(bot->GetMapId(), x, y, ground + 0.5f));
        }
    }
    return points;
}
}  // namespace

bool IsDocked(MotionTransport const* transport, Point const* stop)
{
    uint32 const period = transport->GetPeriod();
    if (!period)
        return false;

    uint32 const timer = transport->GetPathProgress() % period;
    for (KeyFrame const& frame : transport->GetKeyFrames())
    {
        if (!frame.IsStopFrame() || timer < frame.ArriveTime || timer >= frame.DepartureTime)
            continue;

        if (!stop || (frame.Node->mapid == stop->map && std::fabs(frame.Node->x - stop->x) < 1.0f &&
                      std::fabs(frame.Node->y - stop->y) < 1.0f))
            return true;
    }
    return false;
}

bool HoldsStill(Transport const* transport, uint32 ms)
{
    StaticTransport const* car = transport ? transport->ToStaticTransport() : nullptr;
    if (!car)
        return false;

    // StaticTransport::Update moves a car only along its animation: in a cycle, or between two ends
    // where it waits for its state to change.
    TransportAnimation const* animation = car->GetGOValue()->Transport.AnimationInfo;
    if (!animation)
        return true;

    uint32 const progress = car->GetPathProgress();
    if (uint32 const pause = car->GetPauseTime())
        return progress == (car->GetGoState() == GO_STATE_READY ? 0 : pause);

    uint32 const period = car->GetPeriod();
    if (!period)
        return true;

    auto offset = [animation, period](uint32 time, G3D::Vector3& at)
    {
        TransportAnimationEntry const* curr = nullptr;
        TransportAnimationEntry const* next = nullptr;
        float part = 0.0f;
        if (!animation->GetAnimNode(time % period, curr, next, part))
            return false;

        at = G3D::Vector3(curr->X + part * (next->X - curr->X), curr->Y + part * (next->Y - curr->Y),
                          curr->Z + part * (next->Z - curr->Z));
        return true;
    };

    G3D::Vector3 start;
    if (!offset(progress, start))
        return false;

    uint32 ahead = 0;
    do
    {
        ahead = std::min(ahead + HoldStepMs, ms);
        G3D::Vector3 at;
        if (!offset(progress + ahead, at) || (at - start).squaredLength() > 0.01f)
            return false;
    } while (ahead < ms);
    return true;
}

bool FindDock(Player* bot, Point const& stop, GameObject const* transport, WorldPosition& dock)
{
    uint32 checks = 0;
    for (WorldPosition const& point :
         DryGround(bot, stop.x, stop.y, stop.z, DockMinimumYards, DockSearchRadius, transport))
    {
        if (checks++ >= DockPathChecks)
            break;

        PathGenerator path(bot);
        path.CalculatePath(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ());
        G3D::Vector3 const& end = path.GetActualEndPosition();
        if ((path.GetPathType() & PATHFIND_NORMAL) && point.GetExactDist(end.x, end.y, end.z) < 3.0f)
        {
            dock = point;
            return true;
        }
    }
    return false;
}

bool FindBoardingPoint(Player* bot, Transport* transport, WorldPosition& point)
{
    // Walk from the transport's centre towards the bot and keep the last point still on its deck.
    float const dx = bot->GetPositionX() - transport->GetPositionX();
    float const dy = bot->GetPositionY() - transport->GetPositionY();
    float const distance = std::sqrt(dx * dx + dy * dy);
    if (distance < DeckProbeStep)
        return false;

    bool found = false;
    for (float travelled = 0.0f; travelled <= distance; travelled += DeckProbeStep)
    {
        float const x = transport->GetPositionX() + dx / distance * travelled;
        float const y = transport->GetPositionY() + dy / distance * travelled;
        if (!OnTransport(transport, bot->GetPhaseMask(), x, y, bot->GetPositionZ()))
        {
            if (found)
                break;
            continue;
        }

        point = WorldPosition(bot->GetMapId(), x, y, bot->GetPositionZ());
        found = true;
    }

    if (!found)
        return false;

    // Two yards inboard of the edge keep the bot from stopping on the rail.
    point = WorldPosition(bot->GetMapId(), point.GetPositionX() - dx / distance * 2.0f,
                          point.GetPositionY() - dy / distance * 2.0f, point.GetPositionZ());
    return true;
}

bool FindLandingPoint(Player* bot, Transport* transport, WorldPosition& point)
{
    std::vector<WorldPosition> const points = DryGround(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                        bot->GetPositionZ(), 3.0f, DockSearchRadius, transport);
    if (points.empty())
        return false;

    point = points.front();
    return true;
}

Ride CompanionRide(Player* bot, Transport* carrier, WorldPosition& point)
{
    Transport* const ride = bot->GetTransport();
    if (ride && ride->IsStaticTransport() && ride != carrier)
        return HoldsStill(ride, StepOffMs) && FindLandingPoint(bot, ride, point) ? Ride::Leave : Ride::Hold;

    if (!carrier)
        return Ride::Follow;

    if (MotionTransport const* ship = carrier->ToMotionTransport())
        return ride == carrier || IsDocked(ship, nullptr) ? Ride::Follow : Ride::Hold;

    if (ride == carrier || !FindBoardingPoint(bot, carrier, point))
        return Ride::Hold;

    // The car must wait until the bot stands on it and the transport check has taken it aboard.
    float const distance = bot->GetExactDist(point);
    uint32 const walk = uint32(distance / ContinentRoutes::RunSpeed * IN_MILLISECONDS) + BoardingMarginMs;
    return distance <= CompanionBoardingYards && HoldsStill(carrier, walk) ? Ride::Board : Ride::Hold;
}
}  // namespace ContinentTravel
