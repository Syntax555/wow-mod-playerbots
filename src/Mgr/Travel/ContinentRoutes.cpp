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
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Transport.h"
#include "TransportMgr.h"
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

bool IsContinent(uint32 mapId)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    return entry && entry->IsContinent();
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

void ContinentRouteCatalog::Add(Link const& link, bool alliance, bool horde)
{
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

void ContinentRouteCatalog::Build()
{
    _entries.clear();
    _teleportSpells.clear();
    _maps.clear();

    uint32 transports = 0;
    uint32 portals = 0;
    uint32 triggers = 0;

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
            if (departure->Node->mapid == arrival->Node->mapid || !IsContinent(departure->Node->mapid) ||
                !IsContinent(arrival->Node->mapid))
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

    for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
    {
        if (!IsContinent(data.mapid) || !(data.phaseMask & PHASEMASK_NORMAL))
            continue;

        GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!goInfo)
            continue;

        uint32 const spellId = goInfo->type == GAMEOBJECT_TYPE_SPELLCASTER ? goInfo->spellcaster.spellId
                               : goInfo->type == GAMEOBJECT_TYPE_GOOBER    ? goInfo->goober.spellId
                                                                           : 0;
        Link link;
        link.kind = LinkKind::Portal;
        link.object = spawnId;
        link.from = {data.mapid, data.posX, data.posY, data.posZ};
        link.seconds = ContinentRoutes::PortalSeconds;
        if (!spellId || !TeleportDestination(spellId, link.to) || !IsContinent(link.to.map) ||
            (link.to.map == link.from.map && ContinentRoutes::Distance(link.from, link.to) < SameMapPortalMinimumYards))
            continue;

        bool alliance = true;
        bool horde = true;
        if (GameObjectTemplateAddon const* addon = sObjectMgr->GetGameObjectTemplateAddon(data.id))
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
        if (!trigger || trigger->map == teleport->target_mapId || !IsContinent(trigger->map) ||
            !IsContinent(teleport->target_mapId))
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
            !IsContinent(to.map))
            continue;

        _teleportSpells.push_back({spellId, to, ZoneOf(AreaAt(to))});
    }

    LOG_INFO("playerbots",
             ">> Continent routes: {} transport, {} portal and {} area trigger links, {} teleport spells.", transports,
             portals, triggers, _teleportSpells.size());
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

namespace
{
constexpr float DeckProbeStep = 1.0f;
constexpr float DeckHeightTolerance = 15.0f;
constexpr uint32 DockPathChecks = 4;

Transport* TransportAt(Player* bot, float x, float y, float z)
{
    for (float const probe : {z, z + 1.5f, z - 0.5f})
        if (Transport* transport = bot->GetMap()->GetTransportForPos(bot->GetPhaseMask(), x, y, probe, bot))
            return transport;
    return nullptr;
}

// Dry ground within range of center at about height z, nearest to center first.
std::vector<WorldPosition> DryGround(Player* bot, float centerX, float centerY, float z, float range,
                                     Transport* exclude)
{
    Map* map = bot->GetMap();
    std::vector<WorldPosition> points;
    for (float radius = 3.0f; radius <= range; radius += 3.0f)
    {
        for (uint32 step = 0; step < 24; ++step)
        {
            float const angle = step * float(M_PI) / 12.0f;
            float const x = centerX + std::cos(angle) * radius;
            float const y = centerY + std::sin(angle) * radius;
            float const ground = map->GetHeight(bot->GetPhaseMask(), x, y, z + 5.0f, true, DeckHeightTolerance + 5.0f);
            if (ground <= INVALID_HEIGHT || std::fabs(ground - z) > DeckHeightTolerance ||
                map->IsInWater(bot->GetPhaseMask(), x, y, ground, bot->GetCollisionHeight()) ||
                (exclude && TransportAt(bot, x, y, ground) == exclude))
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

bool FindDock(Player* bot, Point const& stop, WorldPosition& dock)
{
    uint32 checks = 0;
    for (WorldPosition const& point : DryGround(bot, stop.x, stop.y, stop.z, DockSearchRadius, nullptr))
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
        if (TransportAt(bot, x, y, bot->GetPositionZ()) != transport)
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
    std::vector<WorldPosition> const points =
        DryGround(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), DockSearchRadius, transport);
    if (points.empty())
        return false;

    point = points.front();
    return true;
}
}  // namespace ContinentTravel
