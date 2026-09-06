/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "SceneryPlacement.h"

#include "AccessSounds.h"
#include "Direction.h"
#include "ScreenReader.h"

#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathAdditionPlaceAction.h>
#include <openrct2/actions/scenery/BannerPlaceAction.h>
#include <openrct2/actions/scenery/LargeSceneryPlaceAction.h>
#include <openrct2/actions/scenery/SmallSceneryPlaceAction.h>
#include <openrct2/actions/scenery/WallPlaceAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/core/Numerics.hpp>
#include <openrct2/drawing/Colour.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <string>

using namespace OpenRCT2;
using namespace OpenRCT2::Numerics;
using OpenRCT2::Drawing::Colour;

namespace OpenRCT2::Ui::Accessibility
{
    // Placement session. Stays active until the player presses Escape, so several copies can be
    // placed in a row by moving the cursor and pressing Enter again.
    static bool _active = false;
    static ScenerySelection _selection{};
    static std::string _name;
    static Direction _rotation = 0; // small/large scenery and banner facing
    static Direction _edge = 0;     // wall/banner world edge, set by Shift+WASD
    static uint8_t _quadrant = 0;   // small scenery world quadrant (corner), set by Shift+QEZC

    // Default colours for placed scenery. Colour is cosmetic for a screen-reader user, so a fixed
    // default keeps the controls simple.
    static constexpr Colour kColour = Colour::bordeauxRed;

    static bool IsWallOrBanner()
    {
        return _selection.SceneryType == SCENERY_TYPE_WALL || _selection.SceneryType == SCENERY_TYPE_BANNER;
    }

    void BeginAccessibleSceneryPlacement(const ScenerySelection& selection, const std::string& name)
    {
        if (selection.IsUndefined())
            return;

        _selection = selection;
        _name = name;
        _rotation = 0;
        _edge = 0;
        _quadrant = 0;
        _active = true;

        std::string help = "Placing " + name + ". Move the cursor, ";
        switch (selection.SceneryType)
        {
            case SCENERY_TYPE_WALL:
            case SCENERY_TYPE_BANNER:
                help += "hold Shift and press W, A, S or D to choose the edge, ";
                break;
            case SCENERY_TYPE_SMALL:
                help += "R to rotate, hold Shift and press Q, E, Z or C to choose the corner, ";
                break;
            case SCENERY_TYPE_PATH_ITEM:
                help += "onto a path, ";
                break;
            default:
                help += "R to rotate, ";
                break;
        }
        help += "Enter to place, Escape to finish.";
        ScreenReaderSpeak(help);
    }

    bool IsAccessibleSceneryPlacementActive()
    {
        return _active;
    }

    void AccessibleSceneryPlacementRotate()
    {
        if (!_active)
            return;
        if (IsWallOrBanner())
        {
            ScreenReaderSpeak("Hold Shift and press W, A, S or D to choose the edge");
            return;
        }
        _rotation = (_rotation + 1) & 3;
        // _rotation is placed directly as the object's world direction (see the placement actions
        // below), so name it in the absolute world frame - NOT camera-relative, unlike _edge/_quadrant
        // whose input is screen-relative - so the spoken facing matches what actually gets built.
        ScreenReaderSpeak(std::string("Rotated, facing ") + GetWorldDirectionName(_rotation));
    }

    // Convert a screen-relative edge (0 = top, 1 = right, 2 = bottom, 3 = left, as the player sees
    // the tile) to a world Direction, using the SAME screen delta and camera rotation the map
    // cursor's arrow keys use (see MoveScreen/Move in MapNavigation). This keeps "left" here meaning
    // the same world edge the Left arrow moves toward, so fences follow the mod's direction logic
    // instead of ending up rotated - and lets the edge be named with the mod's absolute compass.
    static Direction ScreenEdgeToWorldDirection(int32_t screenEdge)
    {
        // Screen deltas matching the arrow keys: top like Up (0,-1), right like Right (-1,0),
        // bottom like Down (0,1), left like Left (1,0).
        static constexpr int32_t kDx[4] = { 0, -1, 0, 1 };
        static constexpr int32_t kDy[4] = { -1, 0, 1, 0 };
        int32_t dx = kDx[screenEdge & 3];
        int32_t dy = kDy[screenEdge & 3];
        // Rotate the screen delta by the camera rotation (90-degree step (x,y) -> (y,-x)), as MoveScreen does.
        for (int32_t i = 0, steps = GetCurrentRotation() & 3; i < steps; i++)
        {
            const int32_t nx = dy;
            const int32_t ny = -dx;
            dx = nx;
            dy = ny;
        }
        // World delta to Direction, matching Move(): +x = West, -x = East, +y = South, -y = North.
        if (dx > 0)
            return 2; // West
        if (dx < 0)
            return 0; // East
        if (dy > 0)
            return 1; // South
        return 3;     // North
    }

    void AccessibleSceneryPlacementSetEdge(int32_t screenEdge, const char* /*label*/)
    {
        if (!_active)
            return;
        _edge = ScreenEdgeToWorldDirection(screenEdge);
        // Announce the absolute world edge (e.g. "West edge"), matching how the mod names the
        // direction the Left arrow moves, rather than the screen-relative "Left edge".
        ScreenReaderSpeak(std::string(GetWorldDirectionName(_edge)) + " edge");
    }

    // Absolute compass name of a small-scenery quadrant (the engine's SceneryQuadrantOffsets, in the
    // mod's world frame where -x = East, +x = West, -y = North, +y = South): 0 = NE, 1 = SE, 2 = SW,
    // 3 = NW.
    static const char* WorldQuadrantName(uint8_t quadrant)
    {
        switch (quadrant & 3)
        {
            case 0:
                return "Northeast corner";
            case 1:
                return "Southeast corner";
            case 2:
                return "Southwest corner";
            default:
                return "Northwest corner";
        }
    }

    // Convert a screen-relative corner (0 = top-left, 1 = top-right, 2 = bottom-right, 3 = bottom-left,
    // as the player sees the tile) to a world quadrant, using the SAME screen deltas and camera
    // rotation the edge/arrow-key logic uses (each corner is the sum of its two edge deltas). This
    // keeps corners consistent with the mod's direction logic instead of landing rotated.
    static uint8_t ScreenCornerToWorldQuadrant(int32_t screenCorner)
    {
        // Diagonal screen deltas = the two edge deltas combined (top/right/bottom/left from the edge
        // helper): top-left = top+left, top-right = top+right, bottom-right = bottom+right, etc.
        static constexpr int32_t kDx[4] = { 1, -1, -1, 1 };  // TL, TR, BR, BL
        static constexpr int32_t kDy[4] = { -1, -1, 1, 1 };
        int32_t dx = kDx[screenCorner & 3];
        int32_t dy = kDy[screenCorner & 3];
        for (int32_t i = 0, steps = GetCurrentRotation() & 3; i < steps; i++)
        {
            const int32_t nx = dy;
            const int32_t ny = -dx;
            dx = nx;
            dy = ny;
        }
        // World diagonal to quadrant (SceneryQuadrantOffsets): -x,-y = 0 (NE); -x,+y = 1 (SE);
        // +x,+y = 2 (SW); +x,-y = 3 (NW).
        if (dx < 0)
            return dy < 0 ? 0 : 1;
        return dy > 0 ? 2 : 3;
    }

    void AccessibleSceneryPlacementSetCorner(int32_t screenCorner, const char* /*label*/)
    {
        if (!_active)
            return;
        _quadrant = ScreenCornerToWorldQuadrant(screenCorner);
        // Announce the absolute world corner (e.g. "Northwest corner"), matching the mod's compass.
        ScreenReaderSpeak(WorldQuadrantName(_quadrant));
    }

    static int32_t SurfaceBaseZ(const CoordsXY& tile)
    {
        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface == nullptr)
            return -1;
        int32_t z = floor2(surface->getBaseZ(), kCoordsZStep);
        if (surface->getWaterHeight() > 0)
            z = std::max<int32_t>(z, surface->getWaterHeight());
        return z;
    }

    static int32_t PathBaseZ(const CoordsXY& tile)
    {
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (el->asPath() != nullptr)
                return el->getBaseZ();
            if (el->isLastForTile())
                break;
            el++;
        }
        return -1;
    }

    // Runs a place action, announcing the error on failure. On success plays the place sound and
    // says "Placed"; placement stays active for the next item.
    static void ExecutePlace(GameActions::GameAction* action, const CoordsXYZ& loc)
    {
        action->SetCallback([](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
            {
                PlayCue(Audio::SoundId::error, result->position);
                auto* windowMgr = GetWindowManager();
                windowMgr->ShowError(result->getErrorTitle(), result->getErrorMessage());
                return;
            }
            PlayAccessSound(AccessSound::place);
            ScreenReaderSpeak("Placed");
        });
        GameActions::Execute(action, getGameState());
    }

    void AccessibleSceneryPlacementAtTile(const CoordsXY& mapCoords)
    {
        if (!_active)
            return;

        const int32_t entry = _selection.EntryIndex;

        switch (_selection.SceneryType)
        {
            case SCENERY_TYPE_SMALL:
            {
                int32_t z = SurfaceBaseZ(mapCoords);
                if (z < 0)
                {
                    ScreenReaderSpeak("Cannot place here");
                    return;
                }
                // Search upward for a height where it fits (slopes, obstacles).
                for (int32_t i = 0; i < 7; i++, z += kCoordsZStep)
                {
                    const CoordsXYZD loc = { mapCoords.x, mapCoords.y, z, _rotation };
                    auto query = GameActions::SmallSceneryPlaceAction(loc, _quadrant, entry, kColour, kColour, kColour);
                    if (GameActions::Query(&query, getGameState()).error == GameActions::Status::ok)
                    {
                        auto place = GameActions::SmallSceneryPlaceAction(loc, _quadrant, entry, kColour, kColour, kColour);
                        ExecutePlace(&place, { mapCoords, z });
                        return;
                    }
                }
                const int32_t base = SurfaceBaseZ(mapCoords);
                auto fail = GameActions::SmallSceneryPlaceAction(
                    { mapCoords.x, mapCoords.y, base, _rotation }, _quadrant, entry, kColour, kColour, kColour);
                ExecutePlace(&fail, { mapCoords, base });
                break;
            }

            case SCENERY_TYPE_LARGE:
            {
                int32_t z = SurfaceBaseZ(mapCoords);
                if (z < 0)
                {
                    ScreenReaderSpeak("Cannot place here");
                    return;
                }
                for (int32_t i = 0; i < 7; i++, z += kCoordsZStep)
                {
                    const CoordsXYZD loc = { mapCoords.x, mapCoords.y, z, _rotation };
                    auto query = GameActions::LargeSceneryPlaceAction(loc, entry, kColour, kColour, kColour);
                    if (GameActions::Query(&query, getGameState()).error == GameActions::Status::ok)
                    {
                        auto place = GameActions::LargeSceneryPlaceAction(loc, entry, kColour, kColour, kColour);
                        ExecutePlace(&place, { mapCoords, z });
                        return;
                    }
                }
                const int32_t base = SurfaceBaseZ(mapCoords);
                auto fail = GameActions::LargeSceneryPlaceAction(
                    { mapCoords.x, mapCoords.y, base, _rotation }, entry, kColour, kColour, kColour);
                ExecutePlace(&fail, { mapCoords, base });
                break;
            }

            case SCENERY_TYPE_WALL:
            {
                // Place with z = 0, which WallPlaceAction treats as "auto": it derives the base height
                // from the surface and edge slope, exactly as the game's own wall tool does (which
                // passes gSceneryPlaceZ = 0). This puts the fence on the ground/path it is placed on.
                // Passing an explicit surface height and searching upward (as small/large scenery do)
                // instead floated the fence one elevation step above path tiles.
                const CoordsXYZ loc = { mapCoords.x, mapCoords.y, 0 };
                auto place = GameActions::WallPlaceAction(entry, loc, _edge, kColour, kColour, kColour);
                ExecutePlace(&place, loc);
                break;
            }

            case SCENERY_TYPE_BANNER:
            {
                const int32_t z = PathBaseZ(mapCoords);
                if (z < 0)
                {
                    ScreenReaderSpeak("Banners must be placed on a path");
                    return;
                }
                auto place = GameActions::BannerPlaceAction({ mapCoords.x, mapCoords.y, z, _edge }, entry, kColour);
                ExecutePlace(&place, { mapCoords, z });
                break;
            }

            case SCENERY_TYPE_PATH_ITEM:
            {
                const int32_t z = PathBaseZ(mapCoords);
                if (z < 0)
                {
                    ScreenReaderSpeak("Path items must be placed on a path");
                    return;
                }
                auto place = GameActions::FootpathAdditionPlaceAction({ mapCoords.x, mapCoords.y, z }, entry);
                ExecutePlace(&place, { mapCoords, z });
                break;
            }
        }
    }

    void AccessibleSceneryPlacementCancel()
    {
        if (!_active)
            return;
        _active = false;
        _selection = {};
        ScreenReaderSpeak("Finished placing scenery");
    }
} // namespace OpenRCT2::Ui::Accessibility
