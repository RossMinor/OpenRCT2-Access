/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessFollow.h"

#include "MapNavigation.h"
#include "ScreenReader.h"

#include <openrct2/GameState.h>
#include <openrct2/entity/Peep.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/StringIds.h>
#include <openrct2/world/Location.hpp>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    static bool _following = false;
    static std::string _followName;
    static EntityId _followId = EntityId::GetNull();

    // Event-driven read-outs (no timer): announce the tile only when its type changes, and the
    // action only when it changes - mirroring how the map cursor announces as it moves.
    static std::string _lastTileDesc;
    static std::string _lastAction;

    // The tile the followed NPC was last on, so we only re-centre the camera (and move the map
    // cursor) when they actually step to a new tile.
    static TileCoordsXY _lastTile{ -1, -1 };

    void StartFollowingEntity(EntityId id, const std::string& name)
    {
        if (id.IsNull())
            return;

        // Rather than the engine's smart-follow (which snaps back here and leaves 'C' reporting the
        // old cursor tile), drive the existing map cursor onto the NPC each time they move. That way
        // the camera tracks them AND 'C' reports their live position, using one code path.
        _following = true;
        _followName = name;
        _followId = id;
        _lastTileDesc.clear();
        _lastAction.clear();
        _lastTile = TileCoordsXY{ -1, -1 };
        // Leave the toolbar menu so keys (e.g. C for coordinates) go to the map cursor, not the menu.
        LeaveMenuMode();
        ScreenReaderSpeak("Following " + name + ". Press Escape to stop.");
    }

    bool IsFollowingEntity()
    {
        return _following;
    }

    static void EndFollow(const std::string& message)
    {
        _following = false;
        _followName.clear();
        _followId = EntityId::GetNull();
        _lastTileDesc.clear();
        _lastAction.clear();
        _lastTile = TileCoordsXY{ -1, -1 };
        if (!message.empty())
            ScreenReaderSpeak(message);
    }

    void StopFollowingEntity()
    {
        if (!_following)
            return;
        EndFollow(_followName.empty() ? "Stopped following." : ("Stopped following " + _followName + "."));
    }

    void TickFollowEntity()
    {
        if (!_following)
            return;

        auto* peep = getGameState().entities.TryGetEntity<Peep>(_followId);
        if (peep == nullptr)
        {
            const std::string name = _followName;
            EndFollow(name.empty() ? "No longer following." : (name + " is no longer here."));
            return;
        }

        // Read the tile they are on, only when its type changes - just like moving the map cursor.
        // While on a ride the peep is hidden (x is null), so skip the tile then; the action covers it.
        if (peep->x != kLocationNull)
        {
            const TileCoordsXY tile{ peep->x / kCoordsXYStep, peep->y / kCoordsXYStep };

            // Move the map cursor (and camera) to keep up, only when they change tile.
            if (tile.x != _lastTile.x || tile.y != _lastTile.y)
            {
                _lastTile = tile;
                FollowCursorTo(tile);
            }

            std::string tileDesc = DescribeTile(tile);
            if (tileDesc != _lastTileDesc)
            {
                _lastTileDesc = tileDesc;
                if (!tileDesc.empty())
                    ScreenReaderSpeak(tileDesc);
            }
        }

        // Read what they are doing when it changes (buying, going on a ride, being sick, etc.).
        Formatter ft;
        peep->FormatActionTo(ft);
        std::string action = OpenRCT2::FormatStringIDLegacy(STR_STRINGID, ft.Data());
        if (action != _lastAction)
        {
            _lastAction = action;
            if (!action.empty())
                ScreenReaderSpeak(action);
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
