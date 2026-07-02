/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "SceneryPlacement.h"

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

    static constexpr const char* kDirectionNames[] = { "North", "East", "South", "West" };

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
        ScreenReaderSpeak(std::string("Rotated, facing ") + kDirectionNames[(_rotation + GetCurrentRotation()) & 3]);
    }

    void AccessibleSceneryPlacementSetEdge(int32_t screenEdge, const char* label)
    {
        if (!_active)
            return;
        // The screen-relative edge maps to a world edge through the current view rotation, so
        // "top" is always the edge the player sees at the top whichever way the map is turned.
        _edge = static_cast<Direction>((screenEdge + GetCurrentRotation()) & 3);
        ScreenReaderSpeak(label);
    }

    void AccessibleSceneryPlacementSetCorner(int32_t screenCorner, const char* label)
    {
        if (!_active)
            return;
        _quadrant = static_cast<uint8_t>((screenCorner + GetCurrentRotation()) & 3);
        ScreenReaderSpeak(label);
    }

    static int32_t SurfaceBaseZ(const CoordsXY& tile)
    {
        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface == nullptr)
            return -1;
        int32_t z = floor2(surface->getBaseZ(), kCoordsZStep);
        if (surface->GetWaterHeight() > 0)
            z = std::max<int32_t>(z, surface->GetWaterHeight());
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
            PlayCue(Audio::SoundId::placeItem, result->position);
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
                int32_t z = SurfaceBaseZ(mapCoords);
                if (z < 0)
                {
                    ScreenReaderSpeak("Cannot place here");
                    return;
                }
                for (int32_t i = 0; i < 7; i++, z += kCoordsZStep)
                {
                    const CoordsXYZ loc = { mapCoords.x, mapCoords.y, z };
                    auto query = GameActions::WallPlaceAction(entry, loc, _edge, kColour, kColour, kColour);
                    if (GameActions::Query(&query, getGameState()).error == GameActions::Status::ok)
                    {
                        auto place = GameActions::WallPlaceAction(entry, loc, _edge, kColour, kColour, kColour);
                        ExecutePlace(&place, loc);
                        return;
                    }
                }
                const int32_t base = SurfaceBaseZ(mapCoords);
                auto fail = GameActions::WallPlaceAction(
                    entry, { mapCoords.x, mapCoords.y, base }, _edge, kColour, kColour, kColour);
                ExecutePlace(&fail, { mapCoords, base });
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
