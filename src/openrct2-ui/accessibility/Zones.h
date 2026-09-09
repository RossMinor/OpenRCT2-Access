/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <openrct2/world/Location.hpp>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Named regions of the map, drawn by the player with the area markers and used to give a blind
    // player a sense of place: crossing into one says "Entering Main Street", leaving it says
    // "Leaving Main Street". A sighted player reads the layout of their park at a glance; this is
    // the equivalent for someone who cannot, and it is the player's own vocabulary rather than
    // anything the game knows about.
    //
    // A zone is a SET OF TILES, not a rectangle. It is drawn from rectangles, but merging two
    // overlapping draws and carving one zone out of another both produce arbitrary shapes, so the
    // tile set is the only representation that survives editing. Tiles are absolute map tile
    // coordinates, the same ones the map cursor uses.
    struct Zone
    {
        std::string name;
        std::vector<TileCoordsXY> tiles;
    };

    // ---- lookup ----

    size_t ZoneCount();
    const Zone* ZoneByIndex(size_t index);

    // Index of the zone covering this tile, or -1. Backed by a tile->zone map rather than a scan,
    // so it is cheap enough to call every frame (the enter/leave announcement does).
    int32_t ZoneIndexAtTile(const TileCoordsXY& tile);

    // Index of the zone whose name matches, ignoring case, or -1. Zone names are unique this way:
    // naming a new area with an existing zone's name extends that zone rather than making a second.
    int32_t ZoneIndexByName(const std::string& name);

    // A tile to send the cursor to for this zone - the tile nearest its centre that the zone
    // actually contains, so jumping to a hollow or L-shaped zone still lands inside it.
    TileCoordsXY ZoneAnchorTile(size_t index);

    // Names of the zones that already own any of these tiles, excluding the zone named
    // `excludingName` (case-insensitive), in the order first encountered. This is what the overlap
    // prompt reads out before the player decides whether to carve into them.
    std::vector<std::string> ZonesOverlapping(const std::vector<TileCoordsXY>& tiles, const std::string& excludingName);

    // ---- editing (each saves to disk) ----

    // Creates the zone, or extends the existing one of that name (case-insensitive) with these
    // tiles. Tiles belonging to any OTHER zone are taken from it - the newest naming of a tile
    // wins, which is what "separate what it needs from the old zone" means. A zone left with no
    // tiles at all is removed. Returns the index of the zone that ended up owning the tiles.
    size_t CreateOrExtendZone(const std::string& name, const std::vector<TileCoordsXY>& tiles);

    // Renames a zone. Fails (returning false) if another zone already has that name, since names
    // are the zone's identity - merging two zones by renaming would be a surprising way to do it.
    bool RenameZone(size_t index, const std::string& newName);

    void DeleteZone(size_t index);

    // ---- lifetime ----

    // Called once per frame. Loads and saves zones as the park changes, keyed by the SAVE FILE's
    // path, and holds the loaded-park bookkeeping. Zone speech itself lives with the map cursor,
    // which owns the cursor position (see TickZoneTransitions in MapNavigation).
    void TickZoneStorage();

    // False when there is no save file to key zones to - a park that has never been saved. Zones
    // still work, and are written out as soon as the park is saved, but until then they exist only
    // in memory and the player deserves to be told so before investing in drawing them.
    bool ZonesArePersistable();
} // namespace OpenRCT2::Ui::Accessibility
