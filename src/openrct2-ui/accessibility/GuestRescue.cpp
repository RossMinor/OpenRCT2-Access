/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GuestRescue.h"

#include "ScreenReader.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/peep/PeepPickupAction.h>
#include <openrct2/entity/EntityList.h>
#include <openrct2/entity/Guest.h>
#include <openrct2/entity/Peep.h>
#include <openrct2/network/Network.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Packs a tile's (x, y, z) into one key for the reachable-path set.
    static uint32_t PackTileKey(int32_t x, int32_t y, int32_t z)
    {
        return (static_cast<uint32_t>(x) & 0x3FF) | ((static_cast<uint32_t>(y) & 0x3FF) << 10)
            | ((static_cast<uint32_t>(z) & 0xFFF) << 20);
    }

    // True if the guest currently holds a thought that means "I'm trying to leave / find my way but
    // can't": "I want to go home", "I'm lost!", "I can't find X", or "I can't find the park exit".
    static bool GuestHasLostThought(const Guest& guest)
    {
        for (const auto& thought : guest.thoughts)
        {
            if (thought.type == PeepThoughtType::none)
                break;
            switch (thought.type)
            {
                case PeepThoughtType::goHome:
                case PeepThoughtType::lost:
                case PeepThoughtType::cantFind:
                case PeepThoughtType::cantFindExit:
                    return true;
                default:
                    break;
            }
        }
        return false;
    }

    // Flood-fills the footpath network outward from every park entrance and returns the set of path
    // tiles (packed keys) reachable on foot. A guest standing on a tile NOT in this set has no walking
    // route to any exit. Traversal mirrors the guest pathfinder: it follows a path element's permitted
    // edges and honours sloped tiles via FootpathIsZAndDirectionValid, so slopes and bridges are
    // handled the same way a guest would walk them.
    // Flood-fills the footpath network outward from the given seed tiles and returns the set of path
    // tiles (packed keys) connected to them. Traversal mirrors the guest pathfinder: it follows a path
    // element's permitted edges and honours sloped tiles via FootpathIsZAndDirectionValid.
    static std::unordered_set<uint32_t> FloodPathNetwork(const std::vector<TileCoordsXYZ>& seeds)
    {
        std::unordered_set<uint32_t> reachable;
        std::vector<TileCoordsXYZ> stack;

        const auto enqueue = [&](const TileCoordsXYZ& tile) {
            const uint32_t key = PackTileKey(tile.x, tile.y, tile.z);
            if (reachable.insert(key).second)
                stack.push_back(tile);
        };

        for (const auto& seed : seeds)
            enqueue(seed);

        while (!stack.empty())
        {
            const TileCoordsXYZ loc = stack.back();
            stack.pop_back();

            auto* path = MapGetPathElementAt(loc);
            if (path == nullptr)
                continue;

            uint8_t edges = path->GetEdges();
            for (Direction dir : kAllDirections)
            {
                if (!(edges & (1 << dir)))
                    continue;

                int32_t arrivalZ = loc.z;
                if (path->IsSloped() && path->GetSlopeDirection() == dir)
                    arrivalZ += 2;

                const TileCoordsXY neighbour{ loc.x + TileDirectionDelta[dir].x, loc.y + TileDirectionDelta[dir].y };
                for (auto* nextPath : TileElementsView<PathElement>(neighbour.ToCoordsXY()))
                {
                    if (nextPath->isGhost())
                        continue;
                    if (!FootpathIsZAndDirectionValid(*nextPath, arrivalZ, dir))
                        continue;
                    enqueue(TileCoordsXYZ{ neighbour.x, neighbour.y, nextPath->baseHeight });
                }
            }
        }

        return reachable;
    }

    // The path tiles directly connected to each park entrance (the tiles a guest steps onto walking in
    // through the entrance) - the seeds for the entrance-reachable flood-fill.
    static std::vector<TileCoordsXYZ> EntranceSeedTiles()
    {
        std::vector<TileCoordsXYZ> seeds;
        for (const auto& entrance : getGameState().park.entrances)
        {
            const TileCoordsXYZ entranceTile{ entrance };
            for (Direction dir : kAllDirections)
            {
                const TileCoordsXY neighbour{ entranceTile.x + TileDirectionDelta[dir].x,
                                              entranceTile.y + TileDirectionDelta[dir].y };
                for (auto* path : TileElementsView<PathElement>(neighbour.ToCoordsXY()))
                {
                    if (path->isGhost())
                        continue;
                    if (!FootpathIsZAndDirectionValid(*path, entranceTile.z, dir))
                        continue;
                    seeds.push_back(TileCoordsXYZ{ neighbour.x, neighbour.y, path->baseHeight });
                }
            }
        }
        return seeds;
    }

    // The set of path tiles reachable on foot from any park entrance. A tile NOT in this set has no
    // walking route to an exit.
    static std::unordered_set<uint32_t> ComputeEntranceReachablePaths()
    {
        return FloodPathNetwork(EntranceSeedTiles());
    }

    // The non-ghost footpath tiles sitting on a single map tile - seeds for flooding the network that
    // tile belongs to.
    static std::vector<TileCoordsXYZ> PathSeedTilesAt(const TileCoordsXY& tile)
    {
        std::vector<TileCoordsXYZ> seeds;
        for (auto* path : TileElementsView<PathElement>(tile.ToCoordsXY()))
            if (!path->isGhost())
                seeds.push_back(TileCoordsXYZ{ tile.x, tile.y, path->baseHeight });
        return seeds;
    }

    // Finds a walkable tile at the park entrance nearest to the guest to teleport them to, returned as
    // world coordinates for a PeepPickupAction. Tries the entrance tile then its neighbours (where the
    // connecting path usually is). Returns nullopt if none is placeable. This only queries placement
    // validity (Place with apply=false) - it never mutates game state - so it is safe to call locally.
    static std::optional<CoordsXYZ> FindRescueTargetLoc(Guest& guest)
    {
        const auto& entrances = getGameState().park.entrances;
        if (entrances.empty())
            return std::nullopt;

        const CoordsXYZD* nearest = nullptr;
        int32_t bestDist = std::numeric_limits<int32_t>::max();
        for (const auto& entrance : entrances)
        {
            const int32_t dist = std::abs(entrance.x - guest.x) + std::abs(entrance.y - guest.y);
            if (dist < bestDist)
            {
                bestDist = dist;
                nearest = &entrance;
            }
        }
        if (nearest == nullptr)
            return std::nullopt;

        const TileCoordsXYZ entranceTile{ *nearest };
        std::vector<TileCoordsXYZ> candidates;
        candidates.push_back(entranceTile);
        for (Direction dir : kAllDirections)
            candidates.push_back(
                TileCoordsXYZ{ entranceTile.x + TileDirectionDelta[dir].x, entranceTile.y + TileDirectionDelta[dir].y,
                               entranceTile.z });

        for (const auto& candidate : candidates)
            if (guest.Place(candidate, false).error == GameActions::Status::ok)
                return candidate.ToCoordsXYZ();
        return std::nullopt;
    }

    // The rescue teleports guests through the game's own pick-up-and-place action (GameCommand::PickupGuest)
    // rather than moving them directly, so the change replicates to every client and stays deterministic
    // in multiplayer. That action is two steps (pick up, then place) and, in multiplayer, executes
    // asynchronously across ticks, so we drive one guest at a time through action callbacks: the plan is
    // computed up front while the world is untouched, then each pickup's callback fires its place, whose
    // callback advances to the next guest. Works identically in single player (callbacks fire inline).
    static std::vector<std::pair<EntityId, CoordsXYZ>> _rescuePlan;
    static size_t _rescueCursor = 0;
    static int32_t _rescuePlaced = 0;
    static bool _rescueActive = false;

    static void ProcessNextRescue()
    {
        // Abort the chain if we left normal play (park unloaded, etc.).
        if (gLegacyScene != LegacyScene::playing)
        {
            _rescuePlan.clear();
            _rescueActive = false;
            return;
        }

        if (_rescueCursor >= _rescuePlan.size())
        {
            // Chain complete: report how many were actually moved.
            if (_rescuePlaced > 0)
                ScreenReaderSpeak(
                    std::to_string(_rescuePlaced) + (_rescuePlaced == 1 ? " stranded guest was" : " stranded guests were")
                    + " teleported to the park entrance");
            else
                ScreenReaderSpeak("No stranded guests could be moved");
            _rescuePlan.clear();
            _rescueActive = false;
            return;
        }

        const EntityId guestId = _rescuePlan[_rescueCursor].first;
        const CoordsXYZ loc = _rescuePlan[_rescueCursor].second;
        const auto owner = Network::GetCurrentPlayerId();

        CoordsXYZ nullLoc{};
        nullLoc.SetNull();
        auto pickup = GameActions::PeepPickupAction(GameActions::PeepPickupType::pickup, guestId, nullLoc, owner);
        pickup.SetCallback([guestId, loc, owner](const GameActions::GameAction*, const GameActions::Result* pickupRes) {
            if (pickupRes->error != GameActions::Status::ok)
            {
                // Couldn't pick this guest up (e.g. they boarded a ride meanwhile); skip to the next.
                _rescueCursor++;
                ProcessNextRescue();
                return;
            }
            auto place = GameActions::PeepPickupAction(GameActions::PeepPickupType::place, guestId, loc, owner);
            place.SetCallback([](const GameActions::GameAction*, const GameActions::Result* placeRes) {
                if (placeRes->error == GameActions::Status::ok)
                    _rescuePlaced++;
                _rescueCursor++;
                ProcessNextRescue();
            });
            GameActions::Execute(&place, getGameState());
        });
        GameActions::Execute(&pickup, getGameState());
    }

    EntranceReachability CheckEntranceReachability(const TileCoordsXY& tile)
    {
        if (getGameState().park.entrances.empty())
            return EntranceReachability::noEntrance;

        // Is there a (non-ghost) footpath on this tile at all?
        bool hasPath = false;
        for (auto* path : TileElementsView<PathElement>(tile.ToCoordsXY()))
        {
            if (!path->isGhost())
            {
                hasPath = true;
                break;
            }
        }
        if (!hasPath)
            return EntranceReachability::notOnPath;

        // Flood-fill from the entrances, then see if any path on this tile is in the reachable set.
        const auto reachable = ComputeEntranceReachablePaths();
        for (auto* path : TileElementsView<PathElement>(tile.ToCoordsXY()))
        {
            if (path->isGhost())
                continue;
            if (reachable.count(PackTileKey(tile.x, tile.y, path->baseHeight)) != 0)
                return EntranceReachability::reachable;
        }
        return EntranceReachability::unreachable;
    }

    std::optional<TileCoordsXY> FindPathDisconnectPoint(const TileCoordsXY& tile)
    {
        // The path network the cursor tile belongs to.
        const auto cursorSeeds = PathSeedTilesAt(tile);
        if (cursorSeeds.empty())
            return std::nullopt;
        const auto cursorNet = FloodPathNetwork(cursorSeeds);
        if (cursorNet.empty())
            return std::nullopt;

        const auto unpackXY = [](uint32_t key) {
            return TileCoordsXY{ static_cast<int32_t>(key & 0x3FF), static_cast<int32_t>((key >> 10) & 0x3FF) };
        };

        // Aim at the entrance-connected network (so the reported point is the gap between the two
        // networks). If there is no connected network at all - or comparing against all of it would be
        // too costly - fall back to the park entrance tiles, giving the point where the cursor's path
        // comes nearest the entrance.
        const auto entranceNet = ComputeEntranceReachablePaths();
        std::vector<TileCoordsXY> targets;
        constexpr int64_t kMaxPairs = 4'000'000; // keep the nearest-pair search snappy for one keypress
        if (!entranceNet.empty() && static_cast<int64_t>(entranceNet.size()) * cursorNet.size() <= kMaxPairs)
        {
            targets.reserve(entranceNet.size());
            for (uint32_t k : entranceNet)
                targets.push_back(unpackXY(k));
        }
        else
        {
            for (const auto& entrance : getGameState().park.entrances)
            {
                const TileCoordsXYZ et{ entrance };
                targets.push_back(TileCoordsXY{ et.x, et.y });
            }
        }
        if (targets.empty())
            return std::nullopt;

        // The tile on the cursor's own path that is closest (Manhattan) to a target - i.e. where the
        // path stops on its way to the entrance / connected network.
        std::optional<TileCoordsXY> best;
        int64_t bestDist = std::numeric_limits<int64_t>::max();
        for (uint32_t k : cursorNet)
        {
            const TileCoordsXY c = unpackXY(k);
            for (const auto& t : targets)
            {
                const int64_t d = std::abs(c.x - t.x) + std::abs(c.y - t.y);
                if (d < bestDist)
                {
                    bestDist = d;
                    best = c;
                }
            }
        }
        return best;
    }

    void RescueLostGuests()
    {
        if (_rescueActive)
        {
            ScreenReaderSpeak("Still rescuing guests, please wait");
            return;
        }
        if (getGameState().park.entrances.empty())
        {
            ScreenReaderSpeak("There is no park entrance to send lost guests to");
            return;
        }

        const auto reachable = ComputeEntranceReachablePaths();

        // Build the rescue plan up front, while the world is untouched, so guest positions and the
        // reachability set are consistent. Each entry is a guest to move and the world position to
        // place them at.
        _rescuePlan.clear();
        _rescueCursor = 0;
        _rescuePlaced = 0;
        int32_t lost = 0;
        for (auto* guest : EntityList<Guest>())
        {
            if (guest->outsideOfPark)
                continue;
            if (guest->State != PeepState::walking && guest->State != PeepState::sitting)
                continue;
            if (!GuestHasLostThought(*guest))
                continue;

            lost++;

            // A guest standing on a footpath can reach an exit only if that tile is part of the
            // network we flooded out from the entrances. A guest NOT on any footpath (wandered onto
            // grass, marooned by deleted paths) has nothing to walk on at all, so they are stranded
            // by definition - teleport them too.
            const TileCoordsXYZ guestTile{ guest->NextLoc };
            const auto* pathAtGuest = MapGetPathElementAt(guestTile);
            if (pathAtGuest != nullptr && reachable.count(PackTileKey(guestTile.x, guestTile.y, guestTile.z)) != 0)
                continue; // on a path that still reaches an exit on its own

            if (auto target = FindRescueTargetLoc(*guest); target.has_value())
                _rescuePlan.emplace_back(guest->id, *target);
        }

        if (_rescuePlan.empty())
        {
            if (lost > 0)
                ScreenReaderSpeak("No stranded guests, every lost guest can still reach an exit");
            else
                ScreenReaderSpeak("No lost guests to rescue");
            return;
        }

        // Announce up front (the plan size is deterministic); the chain then teleports them.
        ScreenReaderSpeak(
            "Rescuing " + std::to_string(_rescuePlan.size())
            + (_rescuePlan.size() == 1 ? " stranded guest" : " stranded guests") + " to the park entrance");
        _rescueActive = true;
        ProcessNextRescue();
    }
} // namespace OpenRCT2::Ui::Accessibility
