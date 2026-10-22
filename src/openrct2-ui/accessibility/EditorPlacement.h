/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/object/ObjectTypes.h>
#include <openrct2/world/Location.hpp>
#include <optional>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // Keyboard placement of what the scenario editor's landscape step puts on the map, driven by the
    // map cursor. The park entrance works like stall placement: the arrow keys move it, R rotates
    // it, the first Enter (or Space) says where it would go or why it cannot, the second builds it,
    // Backspace picks a preview back up and Escape cancels. Guest starting positions are placed one
    // Enter at a time on path tiles outside the park, R turning the way guests walk; Enter on a
    // start already there removes it (the game's own toggle), and Escape finishes.

    // Starts placing a park entrance of this style (an entrance object's entry index).
    void BeginParkEntrancePlacement(ObjectEntryIndex entranceType);

    // Starts placing guest starting positions.
    void BeginGuestStartPlacement();

    // True while a placement is waiting to be positioned and built. The map cursor routes R, Enter,
    // Space, Backspace and Escape to it then, and no window takes the keyboard from it.
    bool IsEditorPlacementActive();

    void EditorPlacementRotate();

    // Enter or Space with the cursor on this tile: the first press previews, the second builds.
    void EditorPlacementAtTile(const TileCoordsXY& tile);

    // Backspace: unfreezes a preview and returns the tile it was frozen at, for the cursor to go back
    // to. Nullopt, and nothing else, unless a preview is frozen.
    std::optional<TileCoordsXY> EditorPlacementPickup();

    // Escape: stops placing, saying so.
    void EditorPlacementCancel();

    // Stops placing silently - the cursor session ended (a step change, leaving the editor).
    void EditorPlacementReset();

    // F1 help for the placement under way.
    std::string EditorPlacementHelp();

    // While a preview is frozen, what the tile readout calls a tile it covers, so the player can
    // arrow over the preview and hear its extent. Nullopt elsewhere.
    std::optional<std::string> EditorPlacementPreviewLabel(const TileCoordsXY& tile);
} // namespace OpenRCT2::Ui::Accessibility
