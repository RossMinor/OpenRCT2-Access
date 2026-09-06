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
#include "AccessSounds.h"
#include "Direction.h"
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
#include <openrct2/util/Util.h>
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

    // Two-stage footprint placement. From positioning (the default), the first Enter does not build:
    // it freezes the footprint at the cursor (previewing = true) so the player can arrow around and
    // hear where it landed. A second Enter then builds at the frozen spot; Backspace picks it back up
    // to reposition. This makes it easy to confirm where a multi-tile ride will go before committing.
    static bool _previewing = false;
    static CoordsXY _previewCursor{};

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

        // While previewing, the footprint is frozen at the spot the player committed to, so the
        // highlight stays put as they arrow the cursor around to inspect it.
        const CoordsXY anchor = _previewing ? _previewCursor : cursor;
        const CoordsXY origin = AnchorOriginFromCursor(anchor);
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
            if (surface->getWaterHeight() > 0)
                z = std::max<int32_t>(z, surface->getWaterHeight());
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

    // Finds a base height where the whole footprint fits at `origin`, searching upward from `startZ`
    // and mirroring the construction tool. Returns the height, or nullopt if none fits.
    //
    // The engine's own query is the authority at every step. Nothing here decides for itself whether
    // an obstruction matters - a flat ride can legitimately stand on supports over a path - so the
    // answer comes from a real TrackPlaceAction at each height rather than from inspecting tiles.
    static std::optional<int32_t> FindFootprintBaseZ(const CoordsXY& origin, int32_t startZ)
    {
        int32_t baseZ = startZ;
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
        // UtilRand, not ScenarioRand: the colour is picked here in UI code and then passed into the
        // action as an explicit parameter, so it travels with the action rather than being re-rolled
        // per client. Drawing from the synced RNG outside an action would desync multiplayer. This
        // matches Ride::setRideEntry, which computes its colour the same way.
        const int32_t colour2 = RideGetUnusedPresetVehicleColour(rideEntryIndex, UtilRand());

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
            _previewing = false;
            _active = true;

            int32_t w = 1, h = 1;
            FootprintSize(w, h);
            std::string size = std::to_string(w) + " by " + std::to_string(h);
            ScreenReaderSpeak(
                "Placing " + rideName + ", " + size + ", entrance facing " + GetWorldDirectionName(_direction)
                + ". The cursor holds the bottom left corner. Move to position it, R to rotate, Enter to place a "
                  "preview, then Enter again to build. Escape to cancel.");
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
        if (_previewing)
        {
            ScreenReaderSpeak("Press Backspace to reposition before rotating");
            return;
        }
        _direction = (_direction + 1) & 3;
        ScreenReaderSpeak(std::string("Rotated, entrance facing ") + GetWorldDirectionName(_direction));
    }

    static void Finish()
    {
        _active = false;
        _rideId = RideId::GetNull();
        _stage = Stage::footprint;
        _previewing = false;
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
        PlayAccessSound(AccessSound::place);
        _previewing = false; // the footprint is down; leave preview for the next stage / completion

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
        // 0.5.5 split walls out of small scenery, so they are named explicitly to keep clearing the
        // same things it always did - a fence in a ride's footprint is exactly what this is for.
        const GameActions::ClearableItems items{ GameActions::ClearableItem::smallScenery,
                                                 GameActions::ClearableItem::largeScenery,
                                                 GameActions::ClearableItem::walls };

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

        // Build from the player's working elevation, not from the terrain. Home and End exist so a
        // ride can be put above something already on the ground, and starting at ground level would
        // throw that away - which is what used to make a path underneath refuse the build no matter
        // how high the cursor was raised. Clamped to the ground because the footprint can span
        // higher terrain than the cursor tile itself.
        const int32_t startZ = std::max(*ground, GetCursorWorkingZ());

        // Does it build exactly where the player asked?
        if (QueryFootprintAt(origin, startZ) == GameActions::Status::ok)
        {
            ExecuteFootprintPlace(origin, startZ);
            return;
        }

        // Clearing only helps when scenery is what is in the way; against a path, wall or another
        // ride it would bulldoze the player's trees for a placement that fails anyway. Doing it here
        // rather than after the height search also keeps a ride sitting down on the ground instead
        // of floating above the bushes it could have removed.
        if (!FootprintHasHardBlocker(origin))
            ClearFootprintScenery(origin);

        // Then let the engine decide, height by height. An unclearable obstruction is not a refusal:
        // the ride may well stand above it on supports, and only the engine can say.
        if (auto baseZ = FindFootprintBaseZ(origin, startZ); baseZ.has_value())
        {
            ExecuteFootprintPlace(origin, *baseZ);
            return;
        }

        // Report against the height the player actually chose, so a refusal at elevation 6 does not
        // describe something lying on the ground far below the cursor.
        AnnounceFootprintError(origin, startZ);
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
                if (el->getType() != TileElementType::track)
                    continue;
                if (el->getBaseZ() != stationBaseZ)
                    continue;
                const auto* track = el->asTrack();
                if (track->getRideIndex() != _rideId)
                    continue;

                // Which side of this track piece the candidate tile is on, in the piece's frame.
                const Direction side = (DirectionReverse(d) - el->getDirection()) & 3;
                const auto& ted = GetTrackElementDescriptor(track->getTrackType());
                const auto connectionSides = ted.sequenceData.sequences[track->getSequenceIndex()]
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

            PlayAccessSound(AccessSound::place);

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
                if (!_previewing)
                {
                    // First Enter: freeze the footprint here so the player can inspect it before
                    // committing. Nothing is built yet.
                    _previewCursor = mapCoords;
                    _previewing = true;
                    int32_t w = 1, h = 1;
                    FootprintSize(w, h);
                    ScreenReaderSpeak(
                        "Ride positioned, " + std::to_string(w) + " by " + std::to_string(h)
                        + " tiles. Arrow around to check the area, Enter to build, Backspace to reposition.");
                }
                else
                {
                    // Second Enter: build at the frozen spot (not wherever the cursor wandered to).
                    // On success OnFootprintPlaced advances the stage; on failure it stays previewing
                    // so the player can Backspace and try elsewhere.
                    PlaceFootprint(_previewCursor);
                }
                break;
            case Stage::entrance:
                PlaceEntranceExit(mapCoords, false);
                break;
            case Stage::exit:
                PlaceEntranceExit(mapCoords, true);
                break;
        }
    }

    void AccessibleRidePlacementPickup()
    {
        if (!_active || _stage != Stage::footprint || !_previewing)
            return;
        _previewing = false;
        ScreenReaderSpeak("Picked back up. Move the cursor and press Enter to position it again.");
    }

    std::optional<std::string> AccessibleRidePlacementPreviewLabel(const TileCoordsXY& tile)
    {
        if (!_active || _stage != Stage::footprint || !_previewing)
            return std::nullopt;

        const CoordsXY origin = AnchorOriginFromCursor(_previewCursor);
        for (const auto& off : FootprintOffsets())
        {
            const TileCoordsXY footprintTile{ CoordsXY{ origin.x + off.x, origin.y + off.y } };
            if (footprintTile.x == tile.x && footprintTile.y == tile.y)
                return _rideName;
        }
        return std::nullopt;
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
