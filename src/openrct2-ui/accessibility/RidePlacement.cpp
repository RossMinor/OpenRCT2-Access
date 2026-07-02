/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RidePlacement.h"

#include "MapNavigation.h"
#include "ScreenReader.h"

#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/Identifiers.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/ResultWithMessage.h>
#include <openrct2/actions/ride/RideCreateAction.h>
#include <openrct2/actions/ride/RideDemolishAction.h>
#include <openrct2/actions/ride/RideEntranceExitPlaceAction.h>
#include <openrct2/actions/ride/RideSetStatusAction.h>
#include <openrct2/actions/terraform/ClearAction.h>
#include <openrct2/actions/track/TrackPlaceAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Money.hpp>
#include <openrct2/core/Numerics.hpp>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/RideEntry.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <openrct2/world/tile_element/TrackElement.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Numerics;
using namespace OpenRCT2::TrackMetadata;
using OpenRCT2::GameActions::CommandFlag;

namespace OpenRCT2::Ui::Accessibility
{
    // A placement runs through one or more stages. Shops/stalls finish after the footprint; flat
    // rides continue to entrance then exit before the ride is complete.
    enum class Stage
    {
        footprint,
        entrance,
        exit,
    };

    // Placement session state. Active between BeginAccessibleRidePlacement and a successful finish
    // or cancel. The ride is created up front and exists for the whole session.
    static bool _active = false;
    static Stage _stage = Stage::footprint;
    static bool _needsEntranceExit = false;
    static RideId _rideId = RideId::GetNull();
    static ride_type_t _rideType = kRideTypeNull;
    static TrackElemType _trackType{};
    static Direction _direction = 0;
    static StationIndex _station = StationIndex::FromUnderlying(0);
    static std::string _rideName;

    // World direction (the value stored on the track piece) to compass name, matching the
    // movement keys' fixed, camera-independent frame: dir 0 = -x = East, 1 = +y = South,
    // 2 = +x = West (the way the Left arrow moves), 3 = -y = North. This is absolute - it does NOT
    // shift with the camera rotation, so a given orientation always reports the same compass name,
    // consistent with the absolute spoken coordinates.
    static constexpr const char* kFacingNames[] = { "East", "South", "West", "North" };

    // Rotated tile offsets (world units, multiples of kCoordsXYStep) of every tile the current
    // footprint occupies, relative to the placement origin. Mirrors how TrackPlaceAction lays a
    // piece out: each occupied tile is origin + clearance.Rotate(direction).
    static std::vector<CoordsXY> FootprintOffsets()
    {
        std::vector<CoordsXY> offsets;
        const auto& ted = GetTrackElementDescriptor(_trackType);
        for (uint8_t i = 0; i < ted.sequenceData.numSequences; i++)
        {
            const auto& bl = ted.sequenceData.sequences[i].clearance;
            offsets.push_back(CoordsXY{ bl.x, bl.y }.Rotate(_direction));
        }
        if (offsets.empty())
            offsets.push_back(CoordsXY{ 0, 0 });
        return offsets;
    }

    // The keyboard cursor always anchors the footprint's "bottom-left" corner so a blind player
    // has a single, predictable grab point no matter the ride's size or orientation. In the spoken
    // coordinate frame "bottom-left" is the lowest X and Y, which corresponds to the largest world
    // x/y of the footprint; so the placement origin is the cursor minus the maximum rotated offset.
    static CoordsXY AnchorOriginFromCursor(const CoordsXY& cursor)
    {
        int32_t maxX = 0, maxY = 0;
        for (const auto& o : FootprintOffsets())
        {
            maxX = std::max(maxX, o.x);
            maxY = std::max(maxY, o.y);
        }
        return CoordsXY{ cursor.x - maxX, cursor.y - maxY };
    }

    // Footprint extent in tiles, for announcing the ride's size to the player.
    static void FootprintSize(int32_t& widthTiles, int32_t& heightTiles)
    {
        int32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        for (const auto& o : FootprintOffsets())
        {
            minX = std::min(minX, o.x);
            maxX = std::max(maxX, o.x);
            minY = std::min(minY, o.y);
            maxY = std::max(maxY, o.y);
        }
        widthTiles = (maxX - minX) / kCoordsXYStep + 1;
        heightTiles = (maxY - minY) / kCoordsXYStep + 1;
    }

    bool AccessibleRidePlacementFootprintRange(const CoordsXY& cursor, MapRange& outRange)
    {
        if (!_active || _stage != Stage::footprint)
            return false;

        const CoordsXY origin = AnchorOriginFromCursor(cursor);
        const auto offsets = FootprintOffsets();
        CoordsXY first{ origin.x + offsets.front().x, origin.y + offsets.front().y };
        int32_t minX = first.x, maxX = first.x, minY = first.y, maxY = first.y;
        for (const auto& off : offsets)
        {
            const int32_t tx = origin.x + off.x;
            const int32_t ty = origin.y + off.y;
            minX = std::min(minX, tx);
            maxX = std::max(maxX, tx);
            minY = std::min(minY, ty);
            maxY = std::max(maxY, ty);
        }
        outRange = MapRange(CoordsXY{ minX, minY }, CoordsXY{ maxX, maxY });
        return true;
    }

    // The natural ground height under the footprint: the highest terrain (or water) corner, which is
    // where a ride sits when nothing forces it upward. Returns nullopt if there is no surface here.
    static std::optional<int32_t> FootprintGroundZ(const CoordsXY& origin)
    {
        int32_t baseZ = 0;
        bool haveSurface = false;
        for (const auto& off : FootprintOffsets())
        {
            auto* surface = MapGetSurfaceElementAt(CoordsXY{ origin.x + off.x, origin.y + off.y });
            if (surface == nullptr)
                continue;
            int32_t z = floor2(surface->getBaseZ(), kCoordsZStep);
            if (surface->GetWaterHeight() > 0)
                z = std::max<int32_t>(z, surface->GetWaterHeight());
            if (!haveSurface || z > baseZ)
            {
                baseZ = z;
                haveSurface = true;
            }
        }
        if (!haveSurface)
            return std::nullopt;
        return baseZ;
    }

    // Dry-run a footprint placement at a specific height and report the engine's verdict.
    static GameActions::Status QueryFootprintAt(const CoordsXY& origin, int32_t z)
    {
        const CoordsXYZD loc{ origin.x, origin.y, z, _direction };
        auto query = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, loc, 0, 0, 0, {}, false);
        return GameActions::Query(&query, getGameState()).error;
    }

    // Finds a base height where the whole footprint fits at `origin`. Starts at ground level and
    // searches upward, mirroring the construction tool. Returns the height, or nullopt if none fits.
    static std::optional<int32_t> FindFootprintBaseZ(const CoordsXY& origin)
    {
        auto ground = FootprintGroundZ(origin);
        if (!ground.has_value())
            return std::nullopt;

        int32_t baseZ = *ground;
        for (int32_t i = 0; i < 7; i++, baseZ += kCoordsZStep)
        {
            const auto err = QueryFootprintAt(origin, baseZ);
            if (err == GameActions::Status::ok)
                return baseZ;
            if (err == GameActions::Status::insufficientFunds)
                break; // raising height won't help
        }
        return std::nullopt;
    }

    static std::string GetRideName(const RideSelection& item)
    {
        const auto* entry = GetRideEntryByIndex(item.EntryIndex);
        if (entry == nullptr)
            return "ride";
        const RideNaming naming = GetRideNaming(item.Type, entry);
        return FormatStringID(naming.Name);
    }

    bool AccessibleRidePlacementSupported(const RideSelection& item)
    {
        if (item.Type >= kRideTypeNull)
            return false;
        // Shops/stalls are a single footprint with no entrance/exit; flat rides are a single
        // footprint plus an entrance and exit. Both can be positioned with the keyboard cursor.
        // Tracked rides (coasters etc.) still need the piece-by-piece construction window.
        const auto& flags = GetRideTypeDescriptor(item.Type).flags;
        return flags.has(RtdFlag::isShopOrFacility) || flags.has(RtdFlag::isFlatRide);
    }

    void BeginAccessibleRidePlacement(const RideSelection& item)
    {
        if (!AccessibleRidePlacementSupported(item))
            return;

        const auto rideName = GetRideName(item);
        const auto rideType = item.Type;
        const auto& rtdFlags = GetRideTypeDescriptor(rideType).flags;
        const auto startPiece = GetRideTypeDescriptor(rideType).StartTrackPiece;
        // Genuine flat rides need a separate entrance and exit. Shops and facilities (food stalls,
        // toilets, etc.) are flagged isFlatRide too but have no entrance/exit - guests just walk up
        // to them - so exclude anything marked as a shop/facility.
        const bool needsEntranceExit = rtdFlags.has(RtdFlag::isFlatRide) && !rtdFlags.has(RtdFlag::isShopOrFacility);

        // Build orientation, matching the construction window's initial facing for placement.
        const Direction direction = (2 - GetCurrentRotation()) & 3;

        const int32_t rideEntryIndex = RideGetEntryIndex(item.Type, item.EntryIndex);
        const int32_t colour1 = RideGetRandomColourPresetIndex(item.Type);
        const int32_t colour2 = RideGetUnusedPresetVehicleColour(rideEntryIndex);

        auto createAction = GameActions::RideCreateAction(
            item.Type, item.EntryIndex, colour1, colour2, getGameState().lastEntranceStyle,
            Config::Get().general.defaultInspectionInterval);

        createAction.SetCallback([rideName, rideType, startPiece, direction, needsEntranceExit](
                                     const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
            {
                ScreenReaderSpeak("Could not create ride");
                return;
            }

            _rideId = result->getData<RideId>();
            _rideType = rideType;
            _trackType = startPiece;
            _direction = direction;
            _station = StationIndex::FromUnderlying(0);
            _rideName = rideName;
            _needsEntranceExit = needsEntranceExit;
            _stage = Stage::footprint;
            _active = true;

            int32_t w = 1, h = 1;
            FootprintSize(w, h);
            std::string size = std::to_string(w) + " by " + std::to_string(h);
            ScreenReaderSpeak(
                "Placing " + rideName + ", " + size + ", entrance facing " + kFacingNames[_direction & 3]
                + ". The cursor holds the bottom left corner. Move to position it, R to rotate, Enter to build, "
                  "Escape to cancel.");
        });

        GameActions::Execute(&createAction, getGameState());
    }

    bool IsAccessibleRidePlacementActive()
    {
        return _active;
    }

    void AccessibleRidePlacementRotate()
    {
        if (!_active)
            return;
        // Only the footprint has a chosen orientation; the entrance/exit facing is derived from
        // where the cursor sits relative to the ride.
        if (_stage != Stage::footprint)
        {
            ScreenReaderSpeak("Rotation only applies to the ride itself");
            return;
        }
        _direction = (_direction + 1) & 3;
        ScreenReaderSpeak(std::string("Rotated, entrance facing ") + kFacingNames[_direction & 3]);
    }

    static void Finish()
    {
        _active = false;
        _rideId = RideId::GetNull();
        _stage = Stage::footprint;
    }

    static void OpenRideWindow()
    {
        auto intent = Intent(WindowClass::ride);
        intent.PutExtra(INTENT_EXTRA_RIDE_ID, _rideId.ToUnderlying());
        ContextOpenIntent(&intent);
    }

    static void OnFootprintPlaced(const CoordsXYZ& trackLoc)
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->CloseByClass(WindowClass::error);
        PlayCue(Audio::SoundId::placeItem, trackLoc);

        if (_needsEntranceExit)
        {
            // Flat ride: the footprint is down, now collect the entrance, then the exit.
            _stage = Stage::entrance;
            ScreenReaderSpeak(
                _rideName
                + " built. Now place the entrance: move the cursor to a tile next to the ride and press Enter. "
                  "Escape removes the ride.");
            return;
        }

        // Shop/stall: no entrance or exit needed, so it's complete.
        auto ride = GetRide(_rideId);
        if (ride != nullptr && ride->getRideTypeDescriptor().flags.has(RtdFlag::isShopOrFacility)
            && Config::Get().general.autoOpenShops)
        {
            auto openAction = GameActions::RideSetStatusAction(_rideId, RideStatus::open);
            GameActions::Execute(&openAction, getGameState());
        }

        ScreenReaderSpeak(_rideName + " placed");
        OpenRideWindow();
        Finish();
    }

    // Clears small and large scenery on every tile the footprint occupies, so the ride can be built
    // without manually removing trees and decorations first. Limited to the ride's own footprint:
    // each occupied tile is cleared individually (not a bounding box) so nothing outside the ride is
    // touched. Returns the total cost, or kMoney64Undefined if nothing was clearable.
    static money64 ClearFootprintScenery(const CoordsXY& origin)
    {
        const GameActions::ClearableItems items = GameActions::CLEARABLE_ITEMS::kScenerySmall
            | GameActions::CLEARABLE_ITEMS::kSceneryLarge;

        money64 total = kMoney64Undefined;
        for (const auto& o : FootprintOffsets())
        {
            const CoordsXY tile{ origin.x + o.x, origin.y + o.y };
            auto clear = GameActions::ClearAction(MapRange(tile, tile), items);
            auto res = GameActions::Execute(&clear, getGameState());
            if (res.error == GameActions::Status::ok)
                total = (total == kMoney64Undefined ? 0 : total) + res.cost;
        }
        return total;
    }

    // True if any footprint tile holds a non-scenery blocker (a path, another ride, an entrance/exit
    // or a banner) - something clearing scenery would not resolve. Walls/fences are clearable.
    static bool FootprintHasHardBlocker(const CoordsXY& origin)
    {
        for (const auto& o : FootprintOffsets())
        {
            const CoordsXY tile{ origin.x + o.x, origin.y + o.y };
            if (TileHasNonSceneryBlocker(TileCoordsXY{ tile }))
                return true;
        }
        return false;
    }

    // Plays the error sound and reports why the footprint cannot be built at `z`, leaving placement
    // active so the player can try another tile.
    static void AnnounceFootprintError(const CoordsXY& origin, int32_t z)
    {
        PlayCue(Audio::SoundId::error, { origin, z });
        const CoordsXYZD failLoc = { origin.x, origin.y, z, _direction };
        auto query = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, failLoc, 0, 0, 0, {}, false);
        auto res = GameActions::Query(&query, getGameState());
        GetWindowManager()->ShowError(res.getErrorTitle(), res.getErrorMessage());
    }

    // Builds the footprint at `baseZ`. On success, sweeps any scenery still sharing the ride's tiles
    // (some small scenery does not block a ride, so it survives placement otherwise) and announces.
    static void ExecuteFootprintPlace(const CoordsXY& origin, int32_t baseZ)
    {
        const CoordsXYZD trackLoc = { origin.x, origin.y, baseZ, _direction };
        const CoordsXYZ placedLoc = { origin.x, origin.y, baseZ };
        auto place = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, trackLoc, 0, 0, 0, {}, false);
        place.SetCallback([placedLoc, origin](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return;
            // Sweep any scenery still sharing the ride's tiles (some small scenery does not block a
            // ride, so it survives placement otherwise).
            ClearFootprintScenery(origin);
            OnFootprintPlaced(placedLoc);
        });
        GameActions::Execute(&place, getGameState());
    }

    static void PlaceFootprint(const CoordsXY& cursor)
    {
        // The cursor marks the footprint's bottom-left corner; derive the real placement origin.
        const CoordsXY origin = AnchorOriginFromCursor(cursor);

        auto ground = FootprintGroundZ(origin);
        if (!ground.has_value())
        {
            ScreenReaderSpeak("Cannot build here");
            return;
        }

        const bool hardBlocker = FootprintHasHardBlocker(origin);
        const auto groundErr = QueryFootprintAt(origin, *ground);

        // Case A: it already builds at ground level. Place it, then sweep any coexisting scenery so
        // nothing is left under it.
        if (groundErr == GameActions::Status::ok)
        {
            ExecuteFootprintPlace(origin, *ground);
            return;
        }

        // Case B: blocked by something other than scenery (a path, wall, fence or another ride).
        // Clearing scenery would not make it buildable, so report the reason and leave scenery alone.
        if (hardBlocker)
        {
            AnnounceFootprintError(origin, *ground);
            return;
        }

        // Case C: only scenery (or uneven terrain) is in the way. Clear the footprint scenery first
        // so the ride sits at ground level instead of floating above it, then build (raising it as
        // the construction tool would if the terrain is uneven).
        ClearFootprintScenery(origin);
        if (auto baseZ = FindFootprintBaseZ(origin); baseZ.has_value())
        {
            ExecuteFootprintPlace(origin, *baseZ);
            return;
        }
        AnnounceFootprintError(origin, *ground);
    }

    // If `tile` is a valid square to place an entrance/exit on, returns the direction from that
    // tile toward the ride's station (which is what the place action expects). Mirrors the search
    // in RideGetEntranceOrExitPositionFromScreenPosition, but tile-based for the keyboard cursor.
    static std::optional<Direction> FindStationSideDirection(const CoordsXY& tile)
    {
        auto ride = GetRide(_rideId);
        if (ride == nullptr)
            return std::nullopt;

        const int32_t stationBaseZ = ride->getStation(_station).GetBaseZ();
        const CoordsXY tileStart = tile.ToTileStart();

        for (Direction d = 0; d < kNumOrthogonalDirections; d++)
        {
            const CoordsXY next = tileStart + CoordsDirectionDelta[d];
            if (!MapIsLocationValid(next))
                continue;

            TileElement* el = MapGetFirstElementAt(next);
            if (el == nullptr)
                continue;
            do
            {
                if (el->getType() != TileElementType::Track)
                    continue;
                if (el->getBaseZ() != stationBaseZ)
                    continue;
                const auto* track = el->asTrack();
                if (track->GetRideIndex() != _rideId)
                    continue;

                // Which side of this track piece the candidate tile is on, in the piece's frame.
                const Direction side = (DirectionReverse(d) - el->getDirection()) & 3;
                const auto& ted = GetTrackElementDescriptor(track->GetTrackType());
                const auto connectionSides = ted.sequenceData.sequences[track->GetSequenceIndex()]
                                                 .getEntranceConnectionSides();
                if (connectionSides & (1 << side))
                    return d;
            } while (!(el++)->isLastForTile());
        }
        return std::nullopt;
    }

    static void PlaceEntranceExit(const CoordsXY& mapCoords, bool isExit)
    {
        const char* label = isExit ? "exit" : "entrance";

        auto direction = FindStationSideDirection(mapCoords);
        if (!direction.has_value())
        {
            ScreenReaderSpeak(
                std::string("Cannot place the ") + label + " here. Move the cursor to a tile beside the ride.");
            return;
        }

        const CoordsXY tileStart = mapCoords.ToTileStart();
        auto action = GameActions::RideEntranceExitPlaceAction(tileStart, *direction, _rideId, _station, isExit);
        action.SetCallback([isExit](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
            {
                PlayCue(Audio::SoundId::error, result->position);
                auto* windowMgr = GetWindowManager();
                windowMgr->ShowError(result->getErrorTitle(), result->getErrorMessage());
                return;
            }

            PlayCue(Audio::SoundId::placeItem, result->position);

            auto ride = GetRide(_rideId);
            // Describe the just-placed entrance/exit: which way its doorway faces and whether a path
            // already connects there, so the player knows where guests will enter and what to build.
            std::string conn;
            if (ride != nullptr)
            {
                const auto& station = ride->getStation(_station);
                conn = DescribeEntranceExitConnection(isExit ? station.Exit : station.Entrance);
            }
            const std::string what = isExit ? "Exit placed" : "Entrance placed";

            const bool complete = ride != nullptr && RideAreAllPossibleEntrancesAndExitsBuilt(*ride).Successful;
            if (complete)
            {
                ScreenReaderSpeak(what + conn + ". " + _rideName + " complete.");
                OpenRideWindow();
                Finish();
            }
            else if (!isExit)
            {
                _stage = Stage::exit;
                ScreenReaderSpeak(what + conn + ". Now place the exit the same way.");
            }
            else
            {
                // Exit placed but more entrances/exits remain (unusual for flat rides): keep going.
                _stage = Stage::entrance;
                ScreenReaderSpeak(what + conn + ". Place the next entrance.");
            }
        });
        GameActions::Execute(&action, getGameState());
    }

    void AccessibleRidePlacementAtTile(const CoordsXY& mapCoords)
    {
        if (!_active)
            return;

        switch (_stage)
        {
            case Stage::footprint:
                PlaceFootprint(mapCoords);
                break;
            case Stage::entrance:
                PlaceEntranceExit(mapCoords, false);
                break;
            case Stage::exit:
                PlaceEntranceExit(mapCoords, true);
                break;
        }
    }

    void AccessibleRidePlacementCancel()
    {
        if (!_active)
            return;

        // Demolish the ride: before the footprint it's an empty ride; after it (a flat ride mid
        // entrance/exit) it can't operate without both, so a clean removal is the safe default.
        auto ride = GetRide(_rideId);
        if (ride != nullptr)
        {
            auto demolish = GameActions::RideDemolishAction(_rideId, GameActions::RideModifyType::demolish);
            demolish.SetFlags({ CommandFlag::allowDuringPaused });
            GameActions::Execute(&demolish, getGameState());
        }

        ScreenReaderSpeak("Placement cancelled");
        Finish();
    }
} // namespace OpenRCT2::Ui::Accessibility
