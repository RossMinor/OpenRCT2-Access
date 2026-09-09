/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Zones.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/core/Console.hpp>
#include <openrct2/core/Json.hpp>
#include <unordered_map>

namespace OpenRCT2::Ui::Accessibility
{
    static std::vector<Zone> _zones;

    // tile -> index into _zones. Rebuilt whenever the zones change; the enter/leave announcement
    // asks for the zone under the cursor every frame, so that lookup must not be a scan.
    static std::unordered_map<uint32_t, int32_t> _tileToZone;

    // The save file the loaded zones belong to. Empty for a park that has never been saved, and on
    // the title screen.
    static std::string _loadedParkKey;

    // The park-load counter as of the last time we looked. A change means a different world was
    // loaded, as opposed to the same one being saved under a new name.
    static uint32_t _seenLoadGeneration = 0;

    static uint32_t TileKey(const TileCoordsXY& tile)
    {
        // Map coordinates never exceed the technical maximum of 1001, so 16 bits each is ample.
        return (static_cast<uint32_t>(tile.x & 0xFFFF) << 16) | static_cast<uint32_t>(tile.y & 0xFFFF);
    }

    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static void RebuildTileIndex()
    {
        _tileToZone.clear();
        for (size_t i = 0; i < _zones.size(); i++)
        {
            for (const auto& tile : _zones[i].tiles)
                _tileToZone[TileKey(tile)] = static_cast<int32_t>(i);
        }
    }

    size_t ZoneCount()
    {
        return _zones.size();
    }

    const Zone* ZoneByIndex(size_t index)
    {
        return index < _zones.size() ? &_zones[index] : nullptr;
    }

    int32_t ZoneIndexAtTile(const TileCoordsXY& tile)
    {
        const auto it = _tileToZone.find(TileKey(tile));
        return it == _tileToZone.end() ? -1 : it->second;
    }

    int32_t ZoneIndexByName(const std::string& name)
    {
        const std::string needle = ToLower(name);
        for (size_t i = 0; i < _zones.size(); i++)
        {
            if (ToLower(_zones[i].name) == needle)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    TileCoordsXY ZoneAnchorTile(size_t index)
    {
        if (index >= _zones.size() || _zones[index].tiles.empty())
            return {};

        const auto& tiles = _zones[index].tiles;
        int64_t sumX = 0, sumY = 0;
        for (const auto& t : tiles)
        {
            sumX += t.x;
            sumY += t.y;
        }
        const int32_t cx = static_cast<int32_t>(sumX / static_cast<int64_t>(tiles.size()));
        const int32_t cy = static_cast<int32_t>(sumY / static_cast<int64_t>(tiles.size()));

        // The centroid of an L-shaped or hollow zone can fall outside it, so land on the member
        // tile closest to the centroid instead of the centroid itself.
        const TileCoordsXY* best = &tiles.front();
        int32_t bestDist = std::numeric_limits<int32_t>::max();
        for (const auto& t : tiles)
        {
            const int32_t d = std::abs(t.x - cx) + std::abs(t.y - cy);
            if (d < bestDist)
            {
                bestDist = d;
                best = &t;
            }
        }
        return *best;
    }

    std::vector<std::string> ZonesOverlapping(const std::vector<TileCoordsXY>& tiles, const std::string& excludingName)
    {
        const std::string skip = ToLower(excludingName);
        std::vector<std::string> names;
        for (const auto& tile : tiles)
        {
            const int32_t idx = ZoneIndexAtTile(tile);
            if (idx < 0)
                continue;
            const std::string& name = _zones[static_cast<size_t>(idx)].name;
            if (ToLower(name) == skip)
                continue;
            if (std::find(names.begin(), names.end(), name) == names.end())
                names.push_back(name);
        }
        return names;
    }

    // ---- persistence ----

    // One file holding every park's zones. A separate file rather than a chunk inside the .park
    // save: the save format is the game's, and a mod writing into it risks producing a file stock
    // OpenRCT2 will not open.
    //
    // The key is the SAVE FILE's path. It was originally the park's name, which is not an identity
    // at all - every save started from the same scenario carries the same name, so zones drawn in
    // one park turned up in the next park loaded. A path identifies exactly one save.
    static std::string ZonesFilePath()
    {
        auto& env = GetContext()->GetPlatformEnvironment();
        return (std::filesystem::path(env.GetDirectoryPath(DirBase::user)) / "access-zones.json").u8string();
    }

    // Empty means "nowhere to keep these": the title screen, or a park that has never been saved.
    static std::string CurrentParkKey()
    {
        if (gLegacyScene != LegacyScene::playing || gCurrentLoadedPath.empty())
            return {};
        // Normalised so the same file reached by a differently spelled path is still the same key.
        return std::filesystem::u8path(gCurrentLoadedPath).lexically_normal().generic_u8string();
    }

    // The file's shape: { "version": 1, "parks": { "<save path>": [ {name, tiles}, ... ] } }. The
    // version is what lets the keys change meaning safely - a file written when keys were park
    // names has no version, so it is ignored rather than half-read as though it were paths.
    static constexpr int32_t kZonesFileVersion = 1;

    static json_t ReadParksObject()
    {
        const auto path = ZonesFilePath();
        if (!std::filesystem::exists(std::filesystem::u8path(path)))
            return json_t::object();
        try
        {
            const auto root = Json::ReadFromFile(path);
            if (!root.is_object() || Json::GetNumber<int32_t>(root.value("version", json_t())) != kZonesFileVersion)
                return json_t::object();
            const auto parks = root.value("parks", json_t());
            return parks.is_object() ? parks : json_t::object();
        }
        catch (const std::exception& e)
        {
            // A corrupt or hand-edited file must not take the game down - report it and act as
            // though no park has zones yet.
            Console::Error::WriteLine("Could not read %s: %s", path.c_str(), e.what());
            return json_t::object();
        }
    }

    static void SaveZonesForCurrentPark()
    {
        const std::string key = _loadedParkKey;
        if (key.empty())
            return;

        try
        {
            json_t parks = ReadParksObject();

            if (_zones.empty())
            {
                parks.erase(key); // no zones left: drop the park's entry rather than leaving "[]"
            }
            else
            {
                json_t parkZones = json_t::array();
                for (const auto& zone : _zones)
                {
                    // Tiles as a flat x,y,x,y array: half the JSON of an array of objects, and the
                    // shape is arbitrary after merges so there is no rectangle list to store.
                    json_t coords = json_t::array();
                    for (const auto& tile : zone.tiles)
                    {
                        coords.push_back(tile.x);
                        coords.push_back(tile.y);
                    }
                    parkZones.push_back(json_t{ { "name", zone.name }, { "tiles", std::move(coords) } });
                }
                parks[key] = std::move(parkZones);
            }

            Json::WriteToFile(ZonesFilePath(), json_t{ { "version", kZonesFileVersion }, { "parks", std::move(parks) } });
        }
        catch (const std::exception& e)
        {
            Console::Error::WriteLine("Could not save zones: %s", e.what());
        }
    }

    static void LoadZonesForKey(const std::string& key)
    {
        _zones.clear();
        _loadedParkKey = key;

        if (!key.empty())
        {
            const json_t parks = ReadParksObject();
            if (parks.contains(key) && parks[key].is_array())
            {
                for (const auto& entry : parks[key])
                {
                    if (!entry.is_object())
                        continue;
                    Zone zone;
                    zone.name = Json::GetString(entry.value("name", json_t()));
                    const auto& coords = entry.value("tiles", json_t::array());
                    if (coords.is_array())
                    {
                        for (size_t i = 0; i + 1 < coords.size(); i += 2)
                        {
                            zone.tiles.push_back(TileCoordsXY{ Json::GetNumber<int32_t>(coords[i]),
                                                               Json::GetNumber<int32_t>(coords[i + 1]) });
                        }
                    }
                    if (!zone.name.empty() && !zone.tiles.empty())
                        _zones.push_back(std::move(zone));
                }
            }
        }

        RebuildTileIndex();
    }

    bool ZonesArePersistable()
    {
        return !CurrentParkKey().empty();
    }

    void TickZoneStorage()
    {
        // A park load replaces the world, so whatever zones are in memory belong to the park that
        // is gone. Bank them under the key they were made with, then start clean from the new
        // save's own entry - even if the new save has no zones at all. Getting this wrong is how
        // zones drawn in an unsaved park ended up in the next park loaded.
        if (gParkLoadGeneration != _seenLoadGeneration)
        {
            _seenLoadGeneration = gParkLoadGeneration;
            SaveZonesForCurrentPark();
            LoadZonesForKey(CurrentParkKey());
            return;
        }

        const std::string key = CurrentParkKey();
        if (key == _loadedParkKey)
            return;

        // Same world, different file: the park was just saved, possibly for the first time or
        // under a new name. The zones on screen are this park's, so they follow it to the new file.
        // Any entry under the old key is left alone, because Save As leaves the original file too.
        _loadedParkKey = key;
        if (key.empty())
            return; // returned to the title screen; keep them in memory, there is nowhere to write
        SaveZonesForCurrentPark();
    }

    // ---- editing ----

    size_t CreateOrExtendZone(const std::string& name, const std::vector<TileCoordsXY>& tiles)
    {
        int32_t target = ZoneIndexByName(name);
        if (target < 0)
        {
            _zones.push_back(Zone{ name, {} });
            target = static_cast<int32_t>(_zones.size()) - 1;
        }

        // Take each tile from whoever holds it. Done before adding, so a tile the target zone
        // already owns is not added twice.
        for (const auto& tile : tiles)
        {
            const int32_t owner = ZoneIndexAtTile(tile);
            if (owner < 0)
                continue;
            auto& ownerTiles = _zones[static_cast<size_t>(owner)].tiles;
            ownerTiles.erase(
                std::remove_if(
                    ownerTiles.begin(), ownerTiles.end(),
                    [&tile](const TileCoordsXY& t) { return t.x == tile.x && t.y == tile.y; }),
                ownerTiles.end());
            _tileToZone.erase(TileKey(tile));
        }

        auto& targetTiles = _zones[static_cast<size_t>(target)].tiles;
        for (const auto& tile : tiles)
            targetTiles.push_back(tile);

        // A zone whose every tile was taken has nothing left to be; drop it. Erasing shifts the
        // indices after it, so fix up the target's own index rather than trusting the old one.
        const std::string targetName = _zones[static_cast<size_t>(target)].name;
        _zones.erase(
            std::remove_if(_zones.begin(), _zones.end(), [](const Zone& z) { return z.tiles.empty(); }), _zones.end());

        RebuildTileIndex();
        SaveZonesForCurrentPark();
        const int32_t finalIndex = ZoneIndexByName(targetName);
        return finalIndex < 0 ? 0 : static_cast<size_t>(finalIndex);
    }

    bool RenameZone(size_t index, const std::string& newName)
    {
        if (index >= _zones.size())
            return false;
        const int32_t clash = ZoneIndexByName(newName);
        if (clash >= 0 && static_cast<size_t>(clash) != index)
            return false;

        _zones[index].name = newName;
        SaveZonesForCurrentPark();
        return true;
    }

    void DeleteZone(size_t index)
    {
        if (index >= _zones.size())
            return;
        _zones.erase(_zones.begin() + static_cast<ptrdiff_t>(index));
        RebuildTileIndex();
        SaveZonesForCurrentPark();
    }
} // namespace OpenRCT2::Ui::Accessibility
