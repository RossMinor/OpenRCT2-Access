/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RidePlacement.h"

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
#include <openrct2/actions/track/TrackPlaceAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
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
#include <optional>
#include <string>

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

    static constexpr const char* kDirectionNames[] = { "North", "East", "South", "West" };

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
        const auto startPiece = GetRideTypeDescriptor(rideType).StartTrackPiece;
        const bool needsEntranceExit = GetRideTypeDescriptor(rideType).flags.has(RtdFlag::isFlatRide);

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

            ScreenReaderSpeak(
                "Placing " + rideName
                + ". Move the cursor to position it, R to rotate, Enter to build, Escape to cancel.");
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
        ScreenReaderSpeak(
            std::string("Rotated, facing ") + kDirectionNames[(_direction + GetCurrentRotation()) & 3]);
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
        Audio::Play3D(Audio::SoundId::placeItem, trackLoc);

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

    static void PlaceFootprint(const CoordsXY& mapCoords)
    {
        auto* surface = MapGetSurfaceElementAt(mapCoords);
        if (surface == nullptr)
        {
            ScreenReaderSpeak("Cannot build here");
            return;
        }

        int32_t baseZ = floor2(surface->getBaseZ(), kCoordsZStep);
        if (surface->GetWaterHeight() > 0)
            baseZ = std::max<int32_t>(baseZ, surface->GetWaterHeight());

        // Search upward for a height where the footprint fits, mirroring the construction tool.
        for (int32_t i = 0; i < 7; i++, baseZ += kCoordsZStep)
        {
            const CoordsXYZD trackLoc = { mapCoords.x, mapCoords.y, baseZ, _direction };
            auto query = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, trackLoc, 0, 0, 0, {}, false);
            auto queryRes = GameActions::Query(&query, getGameState());
            if (queryRes.error != GameActions::Status::ok)
            {
                if (queryRes.error == GameActions::Status::insufficientFunds)
                    break; // raising height won't help
                continue;
            }

            const CoordsXYZ placedLoc = { mapCoords.x, mapCoords.y, baseZ };
            auto place = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, trackLoc, 0, 0, 0, {}, false);
            place.SetCallback([placedLoc](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error == GameActions::Status::ok)
                    OnFootprintPlaced(placedLoc);
            });
            GameActions::Execute(&place, getGameState());
            return;
        }

        // No valid height found: announce the error and stay active for another attempt.
        Audio::Play3D(Audio::SoundId::error, { mapCoords, baseZ });
        auto* windowMgr = GetWindowManager();
        const CoordsXYZD failLoc = { mapCoords.x, mapCoords.y, baseZ, _direction };
        auto query = GameActions::TrackPlaceAction(_rideId, _trackType, _rideType, failLoc, 0, 0, 0, {}, false);
        auto res = GameActions::Query(&query, getGameState());
        windowMgr->ShowError(res.getErrorTitle(), res.getErrorMessage());
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
                Audio::Play3D(Audio::SoundId::error, result->position);
                auto* windowMgr = GetWindowManager();
                windowMgr->ShowError(result->getErrorTitle(), result->getErrorMessage());
                return;
            }

            Audio::Play3D(Audio::SoundId::placeItem, result->position);

            auto ride = GetRide(_rideId);
            const bool complete = ride != nullptr && RideAreAllPossibleEntrancesAndExitsBuilt(*ride).Successful;
            if (complete)
            {
                ScreenReaderSpeak(_rideName + " complete");
                OpenRideWindow();
                Finish();
            }
            else if (!isExit)
            {
                _stage = Stage::exit;
                ScreenReaderSpeak("Entrance placed. Now place the exit the same way.");
            }
            else
            {
                // Exit placed but more entrances/exits remain (unusual for flat rides): keep going.
                _stage = Stage::entrance;
                ScreenReaderSpeak("Exit placed. Place the next entrance.");
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
