/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RideDesignPaths.h"

#include <cstdlib>
#include <filesystem>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/actions/CommandFlag.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathRemoveAction.h>
#include <openrct2/core/Console.hpp>
#include <openrct2/core/Json.hpp>
#include <openrct2/ride/Ride.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // rideId -> the path tiles that ride's design brought with it.
    static std::unordered_map<uint16_t, std::vector<CoordsXYZ>> _records;

    // Rides we have actually seen on the map since this park was loaded. A record only becomes
    // eligible for cleanup once its ride has been observed alive and has then disappeared. Without
    // this, loading a park whose rides were demolished in an earlier session would delete paths on
    // sight, and a record for a ride that is simply not in this save would fire the moment it loaded.
    static std::unordered_set<uint16_t> _alive;

    // In-flight capture.
    static bool _capturing = false;
    static TileCoordsXY _captureMin{};
    static TileCoordsXY _captureMax{};
    static std::unordered_set<uint64_t> _captureBefore;

    static std::string _loadedParkKey;
    static uint32_t _seenLoadGeneration = 0;

    // x and y are tile coordinates, z is the element's base height. Keying on the height as well is
    // what lets two stacked paths on one tile be told apart.
    static uint64_t PackTile(const CoordsXYZ& loc)
    {
        const TileCoordsXY tile{ CoordsXY{ loc.x, loc.y } };
        return (static_cast<uint64_t>(static_cast<uint32_t>(tile.x)) << 40)
            | (static_cast<uint64_t>(static_cast<uint32_t>(tile.y)) << 16)
            | static_cast<uint64_t>(static_cast<uint32_t>(loc.z) & 0xFFFF);
    }

    // Every non-ghost path element standing in the area. Ghosts are the translucent preview and are
    // gone by the time anything is really built, so counting them would produce phantom records.
    static std::vector<CoordsXYZ> CollectPaths(const TileCoordsXY& min, const TileCoordsXY& max)
    {
        std::vector<CoordsXYZ> out;
        for (int32_t y = min.y; y <= max.y; y++)
        {
            for (int32_t x = min.x; x <= max.x; x++)
            {
                const TileCoordsXY tile{ x, y };
                const auto coords = tile.ToCoordsXY();
                if (!MapIsLocationValid(coords))
                    continue;
                for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr; el++)
                {
                    if (el->asPath() != nullptr && !el->isGhost())
                        out.push_back(CoordsXYZ{ coords, el->getBaseZ() });
                    if (el->isLastForTile())
                        break;
                }
            }
        }
        return out;
    }

    // ---- persistence -------------------------------------------------------------------------
    // Mirrors the zone storage: one file covering every park, keyed by the SAVE FILE's path. A park
    // name is not an identity (every save started from one scenario shares it) and a scenario path
    // is a template shared by every playthrough of it, so neither can be the key.

    static constexpr int32_t kFileVersion = 1;

    static std::string RidePathsFilePath()
    {
        auto& env = GetContext()->GetPlatformEnvironment();
        return (std::filesystem::path(env.GetDirectoryPath(DirBase::user)) / "access-ride-paths.json").u8string();
    }

    static std::string CurrentParkKey()
    {
        if (gLegacyScene != LegacyScene::playing || gCurrentLoadedPath.empty() || gCurrentLoadedPathIsScenario)
            return {};
        return std::filesystem::u8path(gCurrentLoadedPath).lexically_normal().generic_u8string();
    }

    static json_t ReadParksObject()
    {
        const auto path = RidePathsFilePath();
        if (!std::filesystem::exists(std::filesystem::u8path(path)))
            return json_t::object();
        try
        {
            const auto root = Json::ReadFromFile(path);
            if (!root.is_object() || Json::GetNumber<int32_t>(root.value("version", json_t())) != kFileVersion)
                return json_t::object();
            const auto parks = root.value("parks", json_t());
            return parks.is_object() ? parks : json_t::object();
        }
        catch (const std::exception& e)
        {
            // A corrupt or hand-edited file must not take the game down.
            Console::Error::WriteLine("Could not read ride design paths: %s", e.what());
            return json_t::object();
        }
    }

    static void SaveForCurrentPark()
    {
        const std::string key = _loadedParkKey;
        if (key.empty())
            return;
        try
        {
            json_t parks = ReadParksObject();
            if (_records.empty())
            {
                parks.erase(key);
            }
            else
            {
                json_t rides = json_t::object();
                for (const auto& record : _records)
                {
                    // Flat x,y,z triples: a fraction of the JSON an array of objects would take.
                    json_t coords = json_t::array();
                    for (const auto& loc : record.second)
                    {
                        coords.push_back(loc.x);
                        coords.push_back(loc.y);
                        coords.push_back(loc.z);
                    }
                    rides[std::to_string(record.first)] = std::move(coords);
                }
                parks[key] = std::move(rides);
            }
            Json::WriteToFile(RidePathsFilePath(), json_t{ { "version", kFileVersion }, { "parks", std::move(parks) } });
        }
        catch (const std::exception& e)
        {
            Console::Error::WriteLine("Could not save ride design paths: %s", e.what());
        }
    }

    static void LoadForKey(const std::string& key)
    {
        _records.clear();
        _alive.clear();
        _loadedParkKey = key;
        if (key.empty())
            return;

        const json_t parks = ReadParksObject();
        if (!parks.contains(key) || !parks[key].is_object())
            return;

        for (const auto& entry : parks[key].items())
        {
            const auto& coords = entry.value();
            if (!coords.is_array())
                continue;
            std::vector<CoordsXYZ> tiles;
            for (size_t i = 0; i + 2 < coords.size(); i += 3)
            {
                tiles.push_back(CoordsXYZ{ Json::GetNumber<int32_t>(coords[i]), Json::GetNumber<int32_t>(coords[i + 1]),
                                           Json::GetNumber<int32_t>(coords[i + 2]) });
            }
            if (tiles.empty())
                continue;
            const auto rideId = static_cast<uint16_t>(std::strtoul(entry.key().c_str(), nullptr, 10));
            _records[rideId] = std::move(tiles);
        }
    }

    // ---- capture -----------------------------------------------------------------------------

    void BeginDesignPathCapture(const TileCoordsXY& min, const TileCoordsXY& max)
    {
        _capturing = true;
        _captureMin = min;
        _captureMax = max;
        _captureBefore.clear();
        for (const auto& loc : CollectPaths(min, max))
            _captureBefore.insert(PackTile(loc));
    }

    void AbortDesignPathCapture()
    {
        _capturing = false;
        _captureBefore.clear();
    }

    void EndDesignPathCapture(RideId rideId)
    {
        if (!_capturing)
            return;
        _capturing = false;

        std::vector<CoordsXYZ> added;
        for (const auto& loc : CollectPaths(_captureMin, _captureMax))
        {
            if (_captureBefore.find(PackTile(loc)) == _captureBefore.end())
                added.push_back(loc);
        }
        _captureBefore.clear();

        if (rideId.IsNull() || added.empty())
            return; // the design brought no paths of its own, so there is nothing to take away later

        _records[rideId.ToUnderlying()] = std::move(added);
        _alive.insert(rideId.ToUnderlying());
        SaveForCurrentPark();
    }

    // ---- removal -----------------------------------------------------------------------------

    static void RemovePathsFor(uint16_t rideId)
    {
        const auto it = _records.find(rideId);
        if (it == _records.end())
            return;

        // Take a copy and drop the record before running any action. A game action can reorganise
        // the tile-element vector, so nothing may be held across one.
        const std::vector<CoordsXYZ> tiles = it->second;
        _records.erase(it);

        for (const auto& loc : tiles)
        {
            auto action = GameActions::FootpathRemoveAction(loc);
            // noSpend: the player never paid for these, so taking them away should neither charge
            // nor refund. allowDuringPaused matches the demolish that prompted this.
            action.SetFlags({ GameActions::CommandFlag::allowDuringPaused, GameActions::CommandFlag::noSpend });
            GameActions::Execute(&action, getGameState());
        }
        SaveForCurrentPark();
    }

    void TickRideDesignPaths()
    {
        // A park load replaces the world, so bank the outgoing park's records and start again from
        // the new save's own entry.
        if (gParkLoadGeneration != _seenLoadGeneration)
        {
            _seenLoadGeneration = gParkLoadGeneration;
            SaveForCurrentPark();
            LoadForKey(CurrentParkKey());
            return;
        }

        const std::string key = CurrentParkKey();
        if (key != _loadedParkKey)
        {
            // Same world, different file: the park was saved, perhaps for the first time or under a
            // new name. The records belong to what is on screen, so they follow it to the new file.
            _loadedParkKey = key;
            if (!key.empty())
                SaveForCurrentPark();
            return;
        }

        if (gLegacyScene != LegacyScene::playing || _records.empty())
            return;

        // A ride we watched exist and that is now gone has been demolished, by whatever route the
        // player used. Collect first and act after: RemovePathsFor edits _records.
        std::vector<uint16_t> vanished;
        for (const auto& record : _records)
        {
            if (GetRide(RideId::FromUnderlying(record.first)) != nullptr)
                _alive.insert(record.first);
            else if (_alive.find(record.first) != _alive.end())
                vanished.push_back(record.first);
        }

        for (const auto rideId : vanished)
        {
            _alive.erase(rideId);
            RemovePathsFor(rideId);
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
