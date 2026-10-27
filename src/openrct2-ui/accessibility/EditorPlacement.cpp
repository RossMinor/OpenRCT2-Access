/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "EditorPlacement.h"

#include "Direction.h"
#include "MapNavigation.h"
#include "ScreenReader.h"
#include "graph/GraphVocab.h"

#include <array>
#include <utility>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/park/ParkEntrancePlaceAction.h>
#include <openrct2/actions/peep/PeepSpawnPlaceAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/tile_element/Slope.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

namespace OpenRCT2::Ui::Accessibility
{
    enum class Kind : uint8_t
    {
        parkEntrance,
        guestStart,
    };

    static bool _active = false;
    static Kind _kind = Kind::parkEntrance;
    static ObjectEntryIndex _entranceType = 0;
    // The way new guests walk from a start. The game stores the opposite: the start sits on the
    // tile's edge in its stored direction and guests walk away from it (GenerateGuest, Park.cpp).
    // Kept between placements, so a row of starts along a map edge needs turning only once.
    static Direction _walkDirection = 0;
    // The entrance's own direction, as the game stores it: pointing into the park. Guests arrive
    // from the opposite side - the outside - which is what the player is told it faces.
    static Direction _direction = 0;
    static bool _previewing = false;
    static TileCoordsXY _previewTile{};

    static const char* OutsideFacing()
    {
        return GetScreenDirectionName(DirectionReverse(_direction));
    }

    // Where the game puts an entrance whose middle is on this tile: on the water if there is any,
    // else on the ground, lifted over a sloped tile - PlaceParkEntranceGetMapPosition in
    // EditorParkEntrance.cpp, which the mouse tool uses.
    static std::optional<CoordsXYZD> EntranceLocation(const TileCoordsXY& tile)
    {
        const CoordsXY world = tile.ToCoordsXY();
        const auto* surface = MapGetSurfaceElementAt(world);
        if (surface == nullptr)
            return std::nullopt;

        int32_t z = surface->getWaterHeight();
        if (z == 0)
        {
            z = surface->getBaseZ();
            if ((surface->getSlope() & kTileSlopeRaisedCornersMask) != 0)
            {
                z += 16;
                if (surface->getSlope() & kTileSlopeDiagonalFlag)
                    z += 16;
            }
        }
        return CoordsXYZD{ world.x, world.y, z, _direction };
    }

    // The three tiles, side to side, as ParkEntrancePlaceAction lays them out around the middle one.
    static std::array<TileCoordsXY, 3> EntranceTiles(const TileCoordsXY& middle)
    {
        const CoordsXY m = middle.ToCoordsXY();
        const CoordsXY a = CoordsDirectionDelta[(_direction - 1) & 3];
        const CoordsXY b = CoordsDirectionDelta[(_direction + 1) & 3];
        return { TileCoordsXY{ m + a }, middle, TileCoordsXY{ m + b } };
    }

    // The game's own place action, with the path the entrance lays through its middle taken from
    // the footpath selection, as the entrance window does.
    static GameActions::ParkEntrancePlaceAction MakePlaceAction(const CoordsXYZD& loc)
    {
        Windows::WindowFootpathSelectDefault();
        const bool isLegacyPath = gFootpathSelection.legacyPath != kObjectEntryIndexNull;
        const auto pathIndex = isLegacyPath ? gFootpathSelection.legacyPath : gFootpathSelection.normalSurface;
        return GameActions::ParkEntrancePlaceAction(loc, pathIndex, _entranceType, isLegacyPath);
    }

    void BeginParkEntrancePlacement(ObjectEntryIndex entranceType)
    {
        _active = true;
        _kind = Kind::parkEntrance;
        _entranceType = entranceType;
        _previewing = false;
        // Start with the outside toward the bottom of the screen, whichever way the camera is turned.
        for (Direction d = 0; d < 4; d++)
        {
            if (ScreenSideOf(DirectionReverse(d)) == 2)
            {
                _direction = d;
                break;
            }
        }
        ScreenReaderSpeak(
            std::string("Placing park entrance, outside facing ") + OutsideFacing()
            + ". Arrow keys move it, R rotates, Enter places, Escape cancels.");
    }

    void BeginGuestStartPlacement()
    {
        _active = true;
        _kind = Kind::guestStart;
        _previewing = false;
        ScreenReaderSpeak(
            std::string("Placing guest starting positions, guests walk ") + GetScreenDirectionName(_walkDirection)
            + ". Move to a path outside the park, R turns the way guests walk, Enter places a start or removes one, "
              "Escape finishes.");
    }

    bool IsEditorPlacementActive()
    {
        return _active;
    }

    // The path a start on this tile stands on: the one at the cursor's working elevation, else the
    // lowest. Nullopt with no path, and the caller passes the ground so the game gives its reason.
    static std::optional<int32_t> GuestStartPathZ(const TileCoordsXY& tile)
    {
        const int32_t workingZ = GetCursorWorkingZ();
        std::optional<int32_t> lowest;
        TileElement* el = MapGetFirstElementAt(tile);
        if (el == nullptr)
            return std::nullopt;
        do
        {
            if (el->asPath() == nullptr || el->isGhost())
                continue;
            const int32_t z = el->getBaseZ();
            if (z == workingZ)
                return z;
            if (!lowest.has_value() || z < *lowest)
                lowest = z;
        } while (!(el++)->isLastForTile());
        return lowest;
    }

    static void PlaceGuestStart(const TileCoordsXY& tile)
    {
        const CoordsXY world = tile.ToCoordsXY();
        int32_t z = 0;
        if (auto pathZ = GuestStartPathZ(tile); pathZ.has_value())
            z = *pathZ;
        else if (const auto* surface = MapGetSurfaceElementAt(world); surface != nullptr)
            z = surface->getBaseZ();

        // The game's place action removes a start already on the tile instead of adding one.
        bool removing = false;
        for (const auto& spawn : getGameState().peepSpawns)
        {
            if (CoordsXY{ spawn.x, spawn.y }.ToTileStart() == world)
                removing = true;
        }

        auto action = GameActions::PeepSpawnPlaceAction(CoordsXYZD{ world.x, world.y, z, DirectionReverse(_walkDirection) });
        // Raised from the UI, it lands a tick later (trap 12.2). A refusal - no path, or not outside
        // the park - is spoken by the error window, in the game's own words.
        action.SetCallback([removing](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return;
            if (removing)
            {
                ScreenReaderSpeak("Guest start removed");
                return;
            }
            Audio::Play3D(Audio::SoundId::placeItem, result->position);
            ScreenReaderSpeak(std::string("Guest start placed, guests walk ") + GetScreenDirectionName(_walkDirection));
        });
        GameActions::Execute(&action, getGameState());
    }

    void EditorPlacementRotate()
    {
        if (!_active)
            return;
        if (_kind == Kind::guestStart)
        {
            _walkDirection = (_walkDirection + 1) & 3;
            ScreenReaderSpeak(std::string("Guests walk ") + GetScreenDirectionName(_walkDirection));
            return;
        }
        if (_previewing)
        {
            ScreenReaderSpeak("Press Backspace to move it before rotating");
            return;
        }
        _direction = (_direction + 1) & 3;
        ScreenReaderSpeak(std::string("Outside facing ") + OutsideFacing());
    }

    void EditorPlacementAtTile(const TileCoordsXY& tile)
    {
        if (!_active)
            return;
        if (_kind == Kind::guestStart)
        {
            PlaceGuestStart(tile);
            return;
        }

        if (!_previewing)
        {
            // First press: say where it would go, or why it cannot - the same Query the build runs.
            auto loc = EntranceLocation(tile);
            if (!loc.has_value())
            {
                ScreenReaderSpeak("The park entrance cannot go here");
                return;
            }
            auto action = MakePlaceAction(*loc);
            const auto result = GameActions::Query(&action, getGameState());
            if (result.error != GameActions::Status::ok)
            {
                ScreenReaderSpeak(
                    "The park entrance cannot go here: " + result.getErrorMessage()
                    + ". Move it and press Enter again, or Escape to cancel.");
                return;
            }
            _previewing = true;
            _previewTile = tile;
            // The three tiles as a range, end to end, lowest spoken coordinate first. Spoken values run
            // opposite to tile indices (trap 12.6), so the end with the higher index reads first.
            auto tiles = EntranceTiles(tile);
            if (tiles[0].x < tiles[2].x || tiles[0].y < tiles[2].y)
                std::swap(tiles[0], tiles[2]);
            ScreenReaderSpeak(
                std::string("Park entrance positioned, outside facing ") + OutsideFacing() + ". It covers "
                + SpokenTileCoordsText(tiles[0]) + " to " + SpokenTileCoordsText(tiles[2])
                + ". Enter builds it, Backspace moves it, Escape cancels.");
            return;
        }

        // Second press: build at the frozen spot, not wherever inspecting it took the cursor.
        auto loc = EntranceLocation(_previewTile);
        if (!loc.has_value())
            return;
        auto action = MakePlaceAction(*loc);
        // Raised from the UI, the action runs a tick later (trap 12.2): finish when it lands. On a
        // failure the error window speaks and the preview stays, to Backspace and try elsewhere.
        action.SetCallback([](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return;
            Audio::Play3D(Audio::SoundId::placeItem, result->position);
            _active = false;
            _previewing = false;
            ScreenReaderSpeak("Park entrance built. Next, build a path from its outside edge to the edge of the map.");
        });
        GameActions::Execute(&action, getGameState());
    }

    std::optional<TileCoordsXY> EditorPlacementPickup()
    {
        if (!_active || !_previewing)
            return std::nullopt;
        _previewing = false;
        return _previewTile;
    }

    void EditorPlacementCancel()
    {
        if (!_active)
            return;
        _active = false;
        _previewing = false;
        // Each guest start is placed as it goes, so leaving keeps them; an entrance not yet built is
        // abandoned.
        ScreenReaderSpeak(_kind == Kind::guestStart ? "Finished placing guest starts" : "Placement cancelled");
    }

    void EditorPlacementReset()
    {
        _active = false;
        _previewing = false;
    }

    std::string EditorPlacementHelp()
    {
        namespace Help = Graph::Vocab::Help;
        return _kind == Kind::guestStart ? Help::kPlacingGuestStarts : Help::kPlacingParkEntrance;
    }

    std::optional<std::string> EditorPlacementPreviewLabel(const TileCoordsXY& tile)
    {
        if (!_active || !_previewing)
            return std::nullopt;
        for (const auto& covered : EntranceTiles(_previewTile))
        {
            if (covered == tile)
                return "Park entrance preview";
        }
        return std::nullopt;
    }
} // namespace OpenRCT2::Ui::Accessibility
