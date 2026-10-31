/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "PatrolAreaPainting.h"

#include "ScreenReader.h"

#include <cctype>
#include <openrct2-ui/UiStringIds.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/peep/StaffSetPatrolAreaAction.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/entity/PatrolArea.h>
#include <openrct2/entity/Staff.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/interface/WidgetIndexGlobals.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/tile_element/PathElement.h>

namespace OpenRCT2::Ui::Accessibility
{
    enum class PatrolMode : uint8_t
    {
        none,
        setting, // one staff member's area (the staff window's tool)
        showing, // every area of one staff type (the staff list's tool)
    };

    static PatrolMode _mode = PatrolMode::none;
    static EntityId _staffId = EntityId::GetNull();
    static StaffType _staffType = StaffType::handyman;

    static Staff* SettingStaff()
    {
        return getGameState().entities.getEntity<Staff>(_staffId);
    }

    static std::string Lowered(std::string text)
    {
        for (auto& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    // The staff list's names for a staff type ("handyman" / "handymen"), lower-cased to sit mid-sentence.
    static std::string StaffTypeText(StaffType type, bool plural)
    {
        switch (type)
        {
            case StaffType::mechanic:
                return Lowered(OpenRCT2::FormatStringID(plural ? STR_MECHANIC_PLURAL : STR_MECHANIC_SINGULAR));
            case StaffType::security:
                return Lowered(OpenRCT2::FormatStringID(plural ? STR_SECURITY_GUARD_PLURAL : STR_SECURITY_GUARD_SINGULAR));
            case StaffType::entertainer:
                return Lowered(OpenRCT2::FormatStringID(plural ? STR_ENTERTAINER_PLURAL : STR_ENTERTAINER_SINGULAR));
            default:
                return Lowered(OpenRCT2::FormatStringID(plural ? STR_HANDYMAN_PLURAL : STR_HANDYMAN_SINGULAR));
        }
    }

    static std::string TileCountText(int32_t count)
    {
        return std::to_string(count) + (count == 1 ? " tile" : " tiles");
    }

    // The staff type the map is showing: the staff member's own type while setting their area.
    static StaffType ShownStaffType()
    {
        if (_mode == PatrolMode::setting)
        {
            if (auto* staff = SettingStaff(); staff != nullptr)
                return staff->assignedStaffType;
        }
        return _staffType;
    }

    // What the game's own tools do while open (PatrolAreaWindow::EnableTool, the staff list's
    // show-patrol-area tool, and their onToolAbort): grid lines on, the patrol areas painted onto
    // the map.
    static void StartShowing()
    {
        if (_mode == PatrolMode::none)
            ShowGridlines();
    }

    static void StopShowing()
    {
        HideGridlines();
        ClearPatrolAreaToRender();
        GfxInvalidateScreen();
    }

    void BeginPatrolAreaMode(EntityId staffId)
    {
        if (getGameState().entities.getEntity<Staff>(staffId) == nullptr)
            return;
        StartShowing();
        SetPatrolAreaToRender(staffId);
        GfxInvalidateScreen();
        _mode = PatrolMode::setting;
        _staffId = staffId;
        ScreenReaderSpeak("Entering patrol mode. Press space to mark the area you would like to be patrolled.");
    }

    void BeginPatrolAreaView(StaffType staffType)
    {
        StartShowing();
        SetPatrolAreaToRender(staffType);
        GfxInvalidateScreen();
        _mode = PatrolMode::showing;
        _staffType = staffType;
        _staffId = EntityId::GetNull();
        ScreenReaderSpeak(
            "Showing where " + StaffTypeText(staffType, true) + " patrol. Press space on a path to set the patrol area of the nearest "
            + StaffTypeText(staffType, false) + " who patrols it.");
    }

    bool IsPatrolAreaModeActive()
    {
        // The staff member can leave the park while their area is being set (fired from another
        // window); the mode goes with them.
        if (_mode == PatrolMode::setting && SettingStaff() == nullptr)
            PatrolAreaModeReset();
        return _mode != PatrolMode::none;
    }

    bool IsPatrolAreaViewActive()
    {
        return _mode == PatrolMode::showing;
    }

    // The game's rule (PatrolAreaWindow::onToolDown): the tile clicked first decides - already
    // patrolled removes, otherwise adds.
    bool PatrolAreaModeWouldRemove(const TileCoordsXY& decidingTile)
    {
        auto* staff = _mode == PatrolMode::setting ? SettingStaff() : nullptr;
        return staff != nullptr && staff->isPatrolAreaSet(decidingTile.ToCoordsXY());
    }

    void PatrolAreaModeApply(const MapRange& area, const TileCoordsXY& decidingTile)
    {
        auto* staff = (IsPatrolAreaModeActive() && _mode == PatrolMode::setting) ? SettingStaff() : nullptr;
        if (staff == nullptr)
            return;

        // Count the tiles the change will actually flip, so the announcement says what happened
        // rather than the size of the brush.
        const bool remove = PatrolAreaModeWouldRemove(decidingTile);
        const auto range = ClampRangeWithinMap(area);
        int32_t changed = 0;
        for (int32_t y = range.GetY1(); y <= range.GetY2(); y += kCoordsXYStep)
            for (int32_t x = range.GetX1(); x <= range.GetX2(); x += kCoordsXYStep)
                if (staff->isPatrolAreaSet({ x, y }) == remove)
                    changed++;

        // The action is queued, so the change is announced from its callback once it has landed.
        auto action = GameActions::StaffSetPatrolAreaAction(
            _staffId, range, remove ? GameActions::StaffSetPatrolAreaMode::unset : GameActions::StaffSetPatrolAreaMode::set);
        action.SetCallback([remove, changed](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // the failure is spoken by the error window
            ScreenReaderSpeak(
                remove ? "Removed " + TileCountText(changed) + " from the patrol area"
                       : "Added " + TileCountText(changed) + " to the patrol area");
        });
        GameActions::Execute(&action, getGameState());
    }

    // The staff list tool's click (StaffListWindow::onToolDown): on a path, the nearest staff member
    // of the type - among those who patrol it, if any do - and straight into setting their area.
    // Off a path the game's click finds nobody and shows its "No handymen in park yet!" message,
    // which misleads; the keyboard says "Not on a path" instead (Ross's choice).
    void PatrolAreaViewLocate(const TileCoordsXY& tile)
    {
        if (_mode != PatrolMode::showing)
            return;
        const auto type = _staffType;

        bool onPath = false;
        const auto coords = tile.ToCoordsXY();
        for (auto* path : TileElementsView<PathElement>(coords))
        {
            if (!path->isGhost())
            {
                onPath = true;
                break;
            }
        }
        if (!onPath)
        {
            ScreenReaderSpeak("Not on a path");
            return;
        }

        Peep* closest = Windows::StaffListClosestStaffMemberAt(type, coords);
        if (closest == nullptr)
        {
            auto ft = Formatter();
            switch (type)
            {
                case StaffType::mechanic:
                    ft.Add<StringId>(STR_MECHANIC_PLURAL);
                    break;
                case StaffType::security:
                    ft.Add<StringId>(STR_SECURITY_GUARD_PLURAL);
                    break;
                case StaffType::entertainer:
                    ft.Add<StringId>(STR_ENTERTAINER_PLURAL);
                    break;
                default:
                    ft.Add<StringId>(STR_HANDYMAN_PLURAL);
                    break;
            }
            ContextShowError(STR_NO_THING_IN_PARK_YET, kStringIdNone, ft);
            return;
        }

        // As the game does: open their window and choose Set Patrol Area there, which (from the
        // keyboard) closes it again and starts setting their area on the map.
        PatrolAreaModeReset();
        if (auto* staffWindow = Windows::StaffOpen(closest); staffWindow != nullptr)
            staffWindow->onDropdown(WC_PEEP__WIDX_PATROL, 0);
    }

    void PatrolAreaModeFinish()
    {
        switch (_mode)
        {
            case PatrolMode::setting:
            {
                // Back to the staff window it was started from.
                auto* staff = SettingStaff();
                PatrolAreaModeReset();
                ScreenReaderSpeak("Finished setting the patrol area");
                if (staff != nullptr)
                    Windows::StaffOpen(staff);
                break;
            }
            case PatrolMode::showing:
            {
                // Back to the staff list, on the same staff type.
                const auto type = _staffType;
                PatrolAreaModeReset();
                ScreenReaderSpeak("Stopped showing patrol areas");
                Windows::StaffListOpenOn(type);
                break;
            }
            default:
                break;
        }
    }

    void PatrolAreaModeReset()
    {
        if (_mode == PatrolMode::none)
            return;
        _mode = PatrolMode::none;
        _staffId = EntityId::GetNull();
        StopShowing();
    }

    // How the game shades a tile while a patrol tool is open (GetPatrolAreaTileColour): light blue
    // for the staff member being set, grey for any other area of their staff type - and while showing
    // a whole staff type, grey for all of its areas. The grey says only that the tile is patrolled,
    // not by whom, so the reading says no more (spec A6).
    enum class PatrolShade : uint8_t
    {
        none,
        own,   // light blue
        other, // grey
    };

    static PatrolShade ShadeAt(const CoordsXY& pos)
    {
        if (_mode == PatrolMode::setting)
        {
            auto* staff = SettingStaff();
            if (staff == nullptr)
                return PatrolShade::none;
            if (staff->isPatrolAreaSet(pos))
                return PatrolShade::own;
        }
        return IsPatrolAreaSetForStaffType(ShownStaffType(), pos) ? PatrolShade::other : PatrolShade::none;
    }

    // "by Handyman 3" for the staff member being set; "by other handymen" for the rest of their type
    // while setting, "by handymen" while showing a whole type.
    static std::string ShadeOwnerText(PatrolShade shade)
    {
        if (shade == PatrolShade::own)
        {
            auto* staff = SettingStaff();
            return staff != nullptr ? "by " + staff->getName() : std::string();
        }
        return std::string(_mode == PatrolMode::setting ? "by other " : "by ") + StaffTypeText(ShownStaffType(), true);
    }

    std::string PatrolAreaTileText(const TileCoordsXY& tile)
    {
        if (!IsPatrolAreaModeActive())
            return {};
        const auto shade = ShadeAt(tile.ToCoordsXY());
        if (shade == PatrolShade::none)
            return "not patrolled";
        return "patrolled " + ShadeOwnerText(shade);
    }

    // A brush of several tiles: how much of it carries each shade.
    std::string PatrolAreaRangeText(const MapRange& area)
    {
        if (!IsPatrolAreaModeActive())
            return {};
        const auto range = ClampRangeWithinMap(area);
        int32_t total = 0;
        int32_t own = 0;
        int32_t other = 0;
        for (int32_t y = range.GetY1(); y <= range.GetY2(); y += kCoordsXYStep)
        {
            for (int32_t x = range.GetX1(); x <= range.GetX2(); x += kCoordsXYStep)
            {
                total++;
                const auto shade = ShadeAt({ x, y });
                if (shade == PatrolShade::own)
                    own++;
                else if (shade == PatrolShade::other)
                    other++;
            }
        }
        const auto share = [total](int32_t count, PatrolShade shade) {
            return (count == total ? std::string("all patrolled ")
                                   : std::to_string(count) + " of " + TileCountText(total) + " patrolled ")
                + ShadeOwnerText(shade);
        };
        std::string text;
        if (own > 0)
            text = share(own, PatrolShade::own);
        if (other > 0)
            text += (text.empty() ? "" : ", ") + share(other, PatrolShade::other);
        return text.empty() ? std::string("not patrolled") : text;
    }
} // namespace OpenRCT2::Ui::Accessibility
