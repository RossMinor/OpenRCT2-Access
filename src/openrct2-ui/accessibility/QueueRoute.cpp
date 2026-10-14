/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "QueueRoute.h"

#include "Direction.h"
#include "Elevation.h"
#include "MapNavigation.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <openrct2/GameState.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/tile_element/EntranceElement.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <optional>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // One queue as the game sees it, walked outward from the ride entrance.
    struct QueueWalk
    {
        std::vector<TileCoordsXYZ> tiles; // the queue's tiles, the one at the entrance first
        std::vector<Direction> steps;     // steps[i]: the way walked onto tiles[i] from the tile before it
        std::vector<std::optional<Direction>> slopes; // slopes[i]: the way tiles[i] rises, if it is a ramp
        bool joinsPath = false;           // an ordinary path is joined to the last tile (the queue's start)
    };

    // One arrow-key press along the route, and what it does to the player's height.
    struct RouteMove
    {
        Direction dir;
        int32_t ramp = 0;     // +1 onto a ramp rising this way, -1 onto one falling this way, 0 flat
        int32_t arriveZ = 0;  // for a ramp, the height (half steps) its far end reaches
    };

    // The move onto `tile` heading `dir`, read against that tile's slope.
    static RouteMove MoveOnto(Direction dir, const TileCoordsXYZ& tile, const std::optional<Direction>& slope)
    {
        RouteMove move{ dir };
        if (slope.has_value())
        {
            // A ramp's base height is its low end: climbing it ends a step above, descending ends on it.
            move.ramp = (*slope == dir) ? 1 : -1;
            move.arriveZ = move.ramp > 0 ? tile.z + 2 : tile.z;
        }
        return move;
    }

    // "left 4, left 2 on a ramp up to elevation 3, up 1" - runs of presses in one direction counted
    // together, a ramp kept as its own run so the player knows where the height changes.
    static std::string MovesText(const std::vector<RouteMove>& moves)
    {
        std::string text;
        for (size_t i = 0; i < moves.size();)
        {
            size_t j = i;
            while (j < moves.size() && moves[j].dir == moves[i].dir && moves[j].ramp == moves[i].ramp)
                j++;
            if (!text.empty())
                text += ", ";
            text += std::string(GetScreenDirectionName(moves[i].dir)) + " " + std::to_string(j - i);
            if (moves[i].ramp != 0)
                text += std::string(moves[i].ramp > 0 ? " on a ramp up" : " on a ramp down") + " to elevation "
                    + ElevationText(moves[j - 1].arriveZ);
            i = j;
        }
        return text;
    }

    // Walks a queue out from a ride entrance's door exactly as the game does when a guest looks for
    // the back of the line (GetRideQueueEnd in peep/GuestPathfinding.cpp; FootpathChainRideQueue in
    // world/Footpath.cpp walks it the same way to place the queue sign). Step through the door; take
    // the path on the next tile at this height, or one step down if it is a ramp descending this way;
    // it must be a queue with an edge back toward where we came from; then carry on straight if it has
    // an edge that way, else turn, trying the clockwise turn first. The walk ends at the first tile that
    // is not a joined queue - which makes the last queue tile reached the queue's start.
    static QueueWalk WalkQueueFromEntrance(const TileCoordsXYZ& entranceLoc, Direction doorway)
    {
        QueueWalk walk;
        TileCoordsXY cur{ entranceLoc.x, entranceLoc.y };
        int32_t baseZ = entranceLoc.z;
        Direction dir = doorway;

        const auto& mapSize = getGameState().mapSize;
        const size_t maxTiles = static_cast<size_t>(mapSize.x) * static_cast<size_t>(mapSize.y);
        while (walk.tiles.size() < maxTiles)
        {
            TileCoordsXY next = cur;
            next += TileDirectionDelta[dir];

            const PathElement* found = nullptr;
            int32_t foundZ = baseZ;
            for (auto* path : TileElementsView<PathElement>(next.ToCoordsXY()))
            {
                if (path->isGhost())
                    continue;
                if (path->baseHeight == baseZ)
                {
                    if (!path->isSloped() || path->getSlopeDirection() == dir)
                        found = path;
                    break;
                }
                if (path->baseHeight == baseZ - 2)
                {
                    if (path->isSloped() && path->getSlopeDirection() == DirectionReverse(dir))
                    {
                        found = path;
                        foundZ = baseZ - 2;
                    }
                    break;
                }
            }
            if (found == nullptr)
                break;

            const bool joinedBack = (found->getEdges() & (1 << DirectionReverse(dir))) != 0;
            if (!found->isQueue())
            {
                walk.joinsPath = joinedBack;
                break;
            }
            if (!joinedBack)
                break;

            const TileCoordsXYZ here{ next.x, next.y, found->baseHeight };
            if (std::find(walk.tiles.begin(), walk.tiles.end(), here) != walk.tiles.end())
                break; // a queue that loops back on itself
            walk.tiles.push_back(here);
            walk.steps.push_back(dir);
            walk.slopes.push_back(found->isSloped() ? std::optional<Direction>(found->getSlopeDirection()) : std::nullopt);
            cur = next;
            baseZ = foundZ;

            const uint8_t edges = found->getEdges();
            if (!(edges & (1 << dir)))
            {
                dir = (dir + 1) & 3;
                if (!(edges & (1 << dir)))
                {
                    dir = DirectionReverse(dir);
                    if (!(edges & (1 << dir)))
                        break;
                }
            }
            // Climbing a ramp: the next tile is entered one step higher.
            if (found->isSloped() && found->getSlopeDirection() == dir)
                baseZ += 2;
        }
        return walk;
    }

    // The ride entrance element a station records, and the world direction its door faces.
    static std::optional<std::pair<TileCoordsXYZ, Direction>> FindStationEntrance(RideId rideId, const TileCoordsXYZD& loc)
    {
        for (auto* entrance : TileElementsView<EntranceElement>(loc.ToCoordsXY()))
        {
            if (entrance->baseHeight == loc.z && entrance->getEntranceType() == EntranceType::rideEntrance
                && entrance->getRideIndex() == rideId)
                return std::make_pair(TileCoordsXYZ{ loc.x, loc.y, loc.z }, GetEntranceFacing(*entrance));
        }
        return std::nullopt;
    }

    static std::string TilesText(size_t n)
    {
        return std::to_string(n) + (n == 1 ? " tile" : " tiles");
    }

    std::string DescribeQueueRoute(const TileCoordsXY& tile, int32_t focusHeight, bool directionsOnly)
    {
        // The queue or ride entrance on this tile nearest the focus elevation.
        const PathElement* queue = nullptr;
        const EntranceElement* entrance = nullptr;
        int32_t bestDistance = std::numeric_limits<int32_t>::max();
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr; el++)
        {
            if (!el->isGhost())
            {
                const int32_t distance = std::abs(el->baseHeight - focusHeight);
                if (auto* p = el->asPath(); p != nullptr && p->isQueue() && distance < bestDistance)
                {
                    queue = p;
                    entrance = nullptr;
                    bestDistance = distance;
                }
                else if (auto* e = el->asEntrance();
                         e != nullptr && e->getEntranceType() == EntranceType::rideEntrance && distance < bestDistance)
                {
                    entrance = e;
                    queue = nullptr;
                    bestDistance = distance;
                }
            }
            if (el->isLastForTile())
                break;
        }
        if (queue == nullptr && entrance == nullptr)
            return "Not on a queue or ride entrance";

        const RideId rideId = queue != nullptr ? queue->getRideIndex() : entrance->getRideIndex();
        auto* ride = GetRide(rideId);
        if (ride == nullptr)
            return "This queue does not lead to a ride";
        const std::string rideName = ride->getName();

        // From an entrance, walk its own queue. From a queue tile, walk each of the ride's entrances
        // and keep the one whose queue passes through this tile; a queue cut off from its entrance
        // still remembers the ride, so it can carry the ride's name without being part of its line.
        QueueWalk walk;
        std::optional<size_t> cursorIndex;
        if (entrance != nullptr)
        {
            walk = WalkQueueFromEntrance(TileCoordsXYZ{ tile.x, tile.y, entrance->baseHeight }, GetEntranceFacing(*entrance));
        }
        else
        {
            const TileCoordsXYZ here{ tile.x, tile.y, queue->baseHeight };
            for (const auto& station : ride->getStations())
            {
                if (station.Entrance.IsNull())
                    continue;
                auto door = FindStationEntrance(rideId, station.Entrance);
                if (!door.has_value())
                    continue;
                QueueWalk candidate = WalkQueueFromEntrance(door->first, door->second);
                if (auto it = std::find(candidate.tiles.begin(), candidate.tiles.end(), here); it != candidate.tiles.end())
                {
                    cursorIndex = static_cast<size_t>(it - candidate.tiles.begin());
                    walk = std::move(candidate);
                    break;
                }
            }
            if (!cursorIndex.has_value())
                return "This queue is not connected to the entrance of " + rideName;
        }

        // Directions only: from the entrance there is nowhere left to go.
        if (directionsOnly && entrance != nullptr)
            return "You are at the " + rideName + " entrance";

        if (walk.tiles.empty())
            return rideName + " entrance has no queue";

        const size_t n = walk.tiles.size();
        std::string text;
        if (!directionsOnly)
            text = rideName + " queue, " + TilesText(n) + ". Starts at " + SpokenTileCoordsText(walk.tiles.back())
                + (walk.joinsPath ? ", connected to path" : ", not connected to path") + ". ";

        // The route from where the player stands, one arrow-key press per tile. On the queue it leads
        // to the entrance - the outward walk backwards, each step reversed. On the entrance it leads
        // out to the start, the outward walk as it is.
        std::vector<RouteMove> moves;
        if (cursorIndex.has_value())
        {
            if (*cursorIndex == n - 1 && !directionsOnly)
                text += "You are at the start. ";
            for (size_t i = *cursorIndex + 1; i-- > 0;)
            {
                const Direction dir = DirectionReverse(walk.steps[i]);
                moves.push_back(
                    i > 0 ? MoveOnto(dir, walk.tiles[i - 1], walk.slopes[i - 1]) : RouteMove{ dir }); // the entrance is flat
            }
            text += TilesText(moves.size()) + " to the entrance: " + MovesText(moves);
        }
        else
        {
            for (size_t i = 0; i < n; i++)
                moves.push_back(MoveOnto(walk.steps[i], walk.tiles[i], walk.slopes[i]));
            text += TilesText(moves.size()) + " to the start: " + MovesText(moves);
        }
        return text;
    }
} // namespace OpenRCT2::Ui::Accessibility
