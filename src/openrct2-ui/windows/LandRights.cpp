/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/accessibility/graph/GraphVocab.h>
#include <openrct2-ui/interface/LandTool.h>
#include <openrct2-ui/interface/Theme.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/park/LandBuyRightsAction.h>
#include <openrct2/actions/park/LandSetRightsAction.h>
#include <openrct2/core/String.hpp>
#include <openrct2/drawing/Text.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    namespace Vocab = Accessibility::Graph::Vocab;

    static constexpr ScreenSize kInGameSize = { 94, 94 };
    static constexpr ScreenSize kEditorSize = { 280, 104 };

    static constexpr ScreenSize kWindowSize = kInGameSize;
    static constexpr StringId kWindowTitle = STR_LAND_RIGHTS;

    enum WindowLandRightsWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PREVIEW,
        WIDX_DECREMENT,
        WIDX_INCREMENT,

        // In-game widgets
        WIDX_BUY_LAND_RIGHTS,
        WIDX_BUY_CONSTRUCTION_RIGHTS,

        // Editor/sandbox widgets
        WIDX_LAND_OWNED_CHECKBOX,
        WIDX_LAND_SALE_CHECKBOX,
        WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX,
        WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX,
        WIDX_UNOWNED_LAND_CHECKBOX,
    };

    // clang-format off
    static constexpr auto window_land_rights_widgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget     ({ 27, 17}, { 44, 32}, WidgetType::imgBtn,   WindowColour::primary, ImageId(SPR_LAND_TOOL_SIZE_0)                                                   ), // preview box
        makeRemapWidget({ 28, 18}, { 16, 16}, WidgetType::trnBtn,   WindowColour::primary, SPR_LAND_TOOL_DECREASE,          STR_ADJUST_SMALLER_LAND_RIGHTS_TIP             ), // decrement size
        makeRemapWidget({ 54, 32}, { 16, 16}, WidgetType::trnBtn,   WindowColour::primary, SPR_LAND_TOOL_INCREASE,          STR_ADJUST_LARGER_LAND_RIGHTS_TIP              ), // increment size
        makeRemapWidget({ 22, 53}, { 24, 24}, WidgetType::imgBtn,   WindowColour::primary, SPR_BUY_LAND_RIGHTS,             STR_BUY_LAND_RIGHTS_TIP                        ), // land rights
        makeRemapWidget({ 52, 53}, { 24, 24}, WidgetType::imgBtn,   WindowColour::primary, SPR_BUY_CONSTRUCTION_RIGHTS,     STR_BUY_CONSTRUCTION_RIGHTS_TIP                ), // construction rights
        makeWidget     ({100, 22}, {170, 12}, WidgetType::checkbox, WindowColour::primary, STR_LAND_OWNED,                  STR_SET_LAND_TO_BE_OWNED_TIP                   ),
        makeWidget     ({100, 38}, {170, 12}, WidgetType::checkbox, WindowColour::primary, STR_LAND_SALE,                   STR_SET_LAND_TO_BE_AVAILABLE_TIP               ),
        makeWidget     ({100, 54}, {170, 12}, WidgetType::checkbox, WindowColour::primary, STR_CONSTRUCTION_RIGHTS_OWNED,   STR_SET_CONSTRUCTION_RIGHTS_TO_BE_OWNED_TIP    ),
        makeWidget     ({100, 70}, {170, 12}, WidgetType::checkbox, WindowColour::primary, STR_CONSTRUCTION_RIGHTS_SALE,    STR_SET_CONSTRUCTION_RIGHTS_TO_BE_AVAILABLE_TIP),
        makeWidget     ({100, 86}, {170, 12}, WidgetType::checkbox, WindowColour::primary, STR_LAND_NOT_OWNED,              STR_SET_LAND_TO_BE_NOT_OWNED_TIP               )
    );
    // clang-format on

    enum class LandRightsMode : uint8_t
    {
        // In-game
        buyLand,
        buyConstructionRights,

        // Sandbox/editor mode
        setLandOwned,
        setLandForSale,
        setConstructionRightsOwned,
        setConstructionRightsForSale,
        setLandUnowned,
    };

    static const bool kLandRightsVisibleByMode[] = { true, false, true, true, false, false, true };
    static const bool kConstructionRightsVisibleByMode[] = { false, true, false, false, true, true, true };

    class LandRightsWindow final : public Window
    {
    private:
        LandRightsMode _landRightsMode;
        money64 _landRightsCost;

        bool IsOwnershipMode() const
        {
            return isInEditorMode() != 0 || getGameState().cheats.sandboxMode;
        }

        void SwitchToMode(LandRightsMode mode)
        {
            auto widgetIndex = WIDX_BUY_LAND_RIGHTS + EnumValue(mode);
            widgetSetPressedExclusive(
                *this,
                { WIDX_BUY_LAND_RIGHTS, WIDX_BUY_CONSTRUCTION_RIGHTS, WIDX_LAND_OWNED_CHECKBOX, WIDX_LAND_SALE_CHECKBOX,
                  WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX, WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX, WIDX_UNOWNED_LAND_CHECKBOX },
                widgetIndex);
            _landRightsMode = mode;

            ToolSet(*this, widgetIndex, Tool::upArrow);

            if (kLandRightsVisibleByMode[EnumValue(mode)])
                ShowLandRights();
            else
                HideLandRights();

            if (kConstructionRightsVisibleByMode[EnumValue(mode)])
                ShowConstructionRights();
            else
                HideConstructionRights();

            invalidate();
        }

    public:
        void onOpen() override
        {
            setWidgets(window_land_rights_widgets);

            widgetsSetHoldable(*this, { WIDX_INCREMENT, WIDX_DECREMENT });
            WindowInitScrollWidgets(*this);
            WindowPushOthersBelow(*this);

            gLandToolSize = kLandToolMinimumSize;
            ShowGridlines();

            if (!IsOwnershipMode())
            {
                if (gLandRemainingOwnershipSales > 0)
                    SwitchToMode(LandRightsMode::buyLand);
                else
                    SwitchToMode(LandRightsMode::buyConstructionRights);
            }
            else
            {
                SwitchToMode(LandRightsMode::setLandOwned);
            }
        }

        void onClose() override
        {
            HideGridlines();
            HideConstructionRights();

            if (isToolActive(WindowClass::landRights))
                ToolCancel();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_PREVIEW:
                    InputSize();
                    break;

                // In-game mode widgets
                case WIDX_BUY_LAND_RIGHTS:
                    if (_landRightsMode != LandRightsMode::buyLand)
                        SwitchToMode(LandRightsMode::buyLand);
                    break;
                case WIDX_BUY_CONSTRUCTION_RIGHTS:
                    if (_landRightsMode != LandRightsMode::buyConstructionRights)
                        SwitchToMode(LandRightsMode::buyConstructionRights);
                    break;

                // Sandbox/editor mode widgets
                case WIDX_UNOWNED_LAND_CHECKBOX:
                    if (_landRightsMode != LandRightsMode::setLandUnowned)
                        SwitchToMode(LandRightsMode::setLandUnowned);
                    break;

                case WIDX_LAND_SALE_CHECKBOX:
                    if (_landRightsMode != LandRightsMode::setLandForSale)
                        SwitchToMode(LandRightsMode::setLandForSale);
                    break;

                case WIDX_LAND_OWNED_CHECKBOX:
                    if (_landRightsMode != LandRightsMode::setLandOwned)
                        SwitchToMode(LandRightsMode::setLandOwned);
                    break;

                case WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX:
                    if (_landRightsMode != LandRightsMode::setConstructionRightsForSale)
                        SwitchToMode(LandRightsMode::setConstructionRightsForSale);
                    break;

                case WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX:
                    if (_landRightsMode != LandRightsMode::setConstructionRightsOwned)
                        SwitchToMode(LandRightsMode::setConstructionRightsOwned);
                    break;
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_DECREMENT:
                    // Decrement land rights tool size
                    gLandToolSize = std::max<uint16_t>(kLandToolMinimumSize, gLandToolSize - 1);

                    // Invalidate the window
                    invalidate();
                    break;
                case WIDX_INCREMENT:
                    // Decrement land rights tool size
                    gLandToolSize = std::min<uint16_t>(kLandToolMaximumSize, gLandToolSize + 1);

                    // Invalidate the window
                    invalidate();
                    break;
            }
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            if (text.empty())
                return;

            if (widgetIndex != WIDX_PREVIEW)
                return;

            const auto res = String::tryParse<int32_t>(text);
            if (res.has_value())
            {
                uint16_t size;
                size = res.value();
                size = std::max(kLandToolMinimumSize, size);
                size = std::min(kLandToolMaximumSize, size);
                gLandToolSize = size;
                invalidate();
            }
        }

        void onUpdate() override
        {
            currentFrame++;

            // Close window if another tool is open
            if (!isToolActive(WindowClass::landRights))
            {
                close();
                return;
            }

            bool inRightsMode = _landRightsMode == LandRightsMode::buyLand
                || _landRightsMode == LandRightsMode::buyConstructionRights;

            if (!IsOwnershipMode() && !inRightsMode)
            {
                if (gLandRemainingOwnershipSales > 0)
                    SwitchToMode(LandRightsMode::buyLand);
                else
                    SwitchToMode(LandRightsMode::buyConstructionRights);
            }
            else if (IsOwnershipMode() && inRightsMode)
                SwitchToMode(LandRightsMode::setLandUnowned);
        }

        void PrepareDrawInGame()
        {
            if (_landRightsMode == LandRightsMode::buyLand)
            {
                setWidgetPressed(WIDX_BUY_LAND_RIGHTS, true);
                setWidgetPressed(WIDX_BUY_CONSTRUCTION_RIGHTS, false);
            }
            else if (_landRightsMode == LandRightsMode::buyConstructionRights)
            {
                setWidgetPressed(WIDX_BUY_LAND_RIGHTS, false);
                setWidgetPressed(WIDX_BUY_CONSTRUCTION_RIGHTS, true);
            }

            if (gLandRemainingOwnershipSales == 0)
            {
                setWidgetDisabled(WIDX_BUY_LAND_RIGHTS, true);
                widgets[WIDX_BUY_LAND_RIGHTS].tooltip = STR_NO_LAND_RIGHTS_FOR_SALE_TIP;
            }
            else
            {
                setWidgetDisabled(WIDX_BUY_LAND_RIGHTS, false);
                widgets[WIDX_BUY_LAND_RIGHTS].tooltip = STR_BUY_LAND_RIGHTS_TIP;
            }

            if (gLandRemainingConstructionSales == 0)
            {
                setWidgetDisabled(WIDX_BUY_CONSTRUCTION_RIGHTS, true);
                widgets[WIDX_BUY_CONSTRUCTION_RIGHTS].tooltip = STR_NO_CONSTRUCTION_RIGHTS_FOR_SALE_TIP;
            }
            else
            {
                setWidgetDisabled(WIDX_BUY_CONSTRUCTION_RIGHTS, false);
                widgets[WIDX_BUY_CONSTRUCTION_RIGHTS].tooltip = STR_BUY_CONSTRUCTION_RIGHTS_TIP;
            }

            // Position land size tool
            auto contentTop = widgets[WIDX_TITLE].bottom + 3;
            widgets[WIDX_PREVIEW].top = contentTop;
            widgets[WIDX_PREVIEW].bottom = widgets[WIDX_PREVIEW].top + 32;
            widgets[WIDX_DECREMENT].top = contentTop + 1;
            widgets[WIDX_DECREMENT].bottom = widgets[WIDX_DECREMENT].top + 16;
            widgets[WIDX_INCREMENT].top = contentTop + 16;
            widgets[WIDX_INCREMENT].bottom = widgets[WIDX_INCREMENT].top + 16;

            // Show in-game mode widgets
            widgets[WIDX_BUY_LAND_RIGHTS].setVisible();
            widgets[WIDX_BUY_CONSTRUCTION_RIGHTS].setVisible();

            // Hide editor/sandbox mode widgets
            widgets[WIDX_UNOWNED_LAND_CHECKBOX].setHidden();
            widgets[WIDX_LAND_OWNED_CHECKBOX].setHidden();
            widgets[WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX].setHidden();
            widgets[WIDX_LAND_SALE_CHECKBOX].setHidden();
            widgets[WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX].setHidden();
        }

        void PrepareDrawSandbox()
        {
            // Position land size tool
            auto contentTop = widgets[WIDX_TITLE].bottom + 27;
            widgets[WIDX_PREVIEW].top = contentTop;
            widgets[WIDX_PREVIEW].bottom = widgets[WIDX_PREVIEW].top + 32;
            widgets[WIDX_DECREMENT].top = contentTop + 1;
            widgets[WIDX_DECREMENT].bottom = widgets[WIDX_DECREMENT].top + 16;
            widgets[WIDX_INCREMENT].top = contentTop + 16;
            widgets[WIDX_INCREMENT].bottom = widgets[WIDX_INCREMENT].top + 16;

            // Hide in-game mode widgets
            widgets[WIDX_BUY_LAND_RIGHTS].setHidden();
            widgets[WIDX_BUY_CONSTRUCTION_RIGHTS].setHidden();

            // Show editor/sandbox mode widgets
            widgets[WIDX_UNOWNED_LAND_CHECKBOX].setVisible();
            widgets[WIDX_LAND_OWNED_CHECKBOX].setVisible();
            widgets[WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX].setVisible();
            widgets[WIDX_LAND_SALE_CHECKBOX].setVisible();
            widgets[WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX].setVisible();
        }

        ScreenSize GetModeDimensions() const
        {
            if (IsOwnershipMode())
                return kEditorSize;
            else
                return kInGameSize;
        }

        void onPrepareDraw() override
        {
            setWidgetPressed(WIDX_PREVIEW, true);
            widgets[WIDX_PREVIEW].image = ImageId(LandTool::SizeToSpriteIndex(gLandToolSize));

            if (width != GetModeDimensions().width)
                onResize();

            if (IsOwnershipMode())
            {
                PrepareDrawSandbox();
                ColourSchemeUpdateByClass(this, WindowClass::map);
            }
            else
            {
                PrepareDrawInGame();
                ColourSchemeUpdateByClass(this, WindowClass::landRights);
            }
        }

        void onResize() override
        {
            invalidate();

            auto dimensions = GetModeDimensions();
            WindowSetResize(*this, dimensions, dimensions);

            if (windowPos.x + width > ContextGetWidth())
                windowPos.x = ContextGetWidth() - width;

            invalidate();
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            auto screenCoords = ScreenCoordsXY{ windowPos.x + widgets[WIDX_PREVIEW].midX(),
                                                windowPos.y + widgets[WIDX_PREVIEW].midY() };

            drawWidgets(rt);
            // Draw number for tool sizes bigger than 7
            if (gLandToolSize > kLandToolMaximumSizeWithSprite)
            {
                auto ft = Formatter();
                ft.Add<uint16_t>(gLandToolSize);
                drawText(rt, screenCoords - ScreenCoordsXY{ 0, 2 }, STR_LAND_TOOL_SIZE_VALUE, ft, { TextAlignment::centre });
            }

            // Draw cost amount
            if (_landRightsCost != kMoney64Undefined && _landRightsCost != 0
                && !getGameState().park.flags.has(ParkFlag::noMoney))
            {
                auto ft = Formatter();
                ft.Add<money64>(_landRightsCost);

                auto offset = widgets[WIDX_BUY_LAND_RIGHTS].isVisible() ? 32 : 8;

                screenCoords = { widgets[WIDX_PREVIEW].midX() + windowPos.x,
                                 widgets[WIDX_PREVIEW].bottom + windowPos.y + offset };
                drawText(rt, screenCoords, STR_COST_AMOUNT, ft, { TextAlignment::centre });
            }
        }

        GameActions::LandBuyRightsAction GetLandBuyAction()
        {
            auto mode = (_landRightsMode == LandRightsMode::buyLand) ? GameActions::LandBuyRightSetting::buyLand
                                                                     : GameActions::LandBuyRightSetting::buyConstructionRights;

            return GameActions::LandBuyRightsAction(
                { gMapSelectPositionA.x, gMapSelectPositionA.y, gMapSelectPositionB.x, gMapSelectPositionB.y }, mode);
        }

        OwnershipFlags GetDesiredOwnership()
        {
            switch (_landRightsMode)
            {
                case LandRightsMode::setLandForSale:
                    return { OwnershipFlag::landForSale };
                case LandRightsMode::setLandOwned:
                    return { OwnershipFlag::landOwned };
                case LandRightsMode::setConstructionRightsForSale:
                    return { OwnershipFlag::constructionRightsForSale };
                case LandRightsMode::setConstructionRightsOwned:
                    return { OwnershipFlag::constructionRightsOwned };
                case LandRightsMode::setLandUnowned:
                default:
                    return kUnowned;
            }
        }

        GameActions::LandSetRightsAction GetLandSetAction()
        {
            return GameActions::LandSetRightsAction(
                { gMapSelectPositionA.x, gMapSelectPositionA.y, gMapSelectPositionB.x, gMapSelectPositionB.y },
                GameActions::LandSetRightSetting::setOwnershipWithChecks, GetDesiredOwnership());
        }

        void onToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            gMapSelectFlags.unset(MapSelectFlag::enable);

            auto info = GetMapCoordinatesFromPos(
                screenCoords, EnumsToFlags(ViewportInteractionItem::terrain, ViewportInteractionItem::water));
            if (info.interactionType == ViewportInteractionItem::none)
            {
                if (_landRightsCost != kMoney64Undefined)
                {
                    _landRightsCost = kMoney64Undefined;

                    auto* windowMgr = GetWindowManager();
                    windowMgr->InvalidateByClass(WindowClass::landRights);
                }
                return;
            }
            auto mapTile = info.Loc;

            uint8_t state_changed = 0;

            if (!gMapSelectFlags.has(MapSelectFlag::enable))
            {
                gMapSelectFlags.set(MapSelectFlag::enable);
                state_changed++;
            }

            if (gMapSelectType != MapSelectType::fullLandRights)
            {
                gMapSelectType = MapSelectType::fullLandRights;
                state_changed++;
            }

            int16_t tool_size = gLandToolSize;
            if (tool_size == 0)
                tool_size = 1;

            int16_t tool_length = (tool_size - 1) * 32;

            // Move to tool bottom left
            mapTile.x -= (tool_size - 1) * 16;
            mapTile.y -= (tool_size - 1) * 16;
            mapTile = mapTile.ToTileStart();

            if (gMapSelectPositionA.x != mapTile.x)
            {
                gMapSelectPositionA.x = mapTile.x;
                state_changed++;
            }

            if (gMapSelectPositionA.y != mapTile.y)
            {
                gMapSelectPositionA.y = mapTile.y;
                state_changed++;
            }

            mapTile.x += tool_length;
            mapTile.y += tool_length;

            if (gMapSelectPositionB.x != mapTile.x)
            {
                gMapSelectPositionB.x = mapTile.x;
                state_changed++;
            }

            if (gMapSelectPositionB.y != mapTile.y)
            {
                gMapSelectPositionB.y = mapTile.y;
                state_changed++;
            }

            if (!state_changed)
                return;

            auto& gameState = getGameState();
            if (IsOwnershipMode())
            {
                auto landSetRightsAction = GetLandSetAction();
                auto res = GameActions::Query(&landSetRightsAction, gameState);
                _landRightsCost = res.error == GameActions::Status::ok ? res.cost : kMoney64Undefined;
            }
            else
            {
                auto landBuyRightsAction = GetLandBuyAction();
                auto res = GameActions::Query(&landBuyRightsAction, gameState);
                _landRightsCost = res.error == GameActions::Status::ok ? res.cost : kMoney64Undefined;
            }
        }

        void onToolAbort(WidgetIndex widgetIndex) override
        {
            if (kLandRightsVisibleByMode[EnumValue(_landRightsMode)])
                HideLandRights();

            if (kConstructionRightsVisibleByMode[EnumValue(_landRightsMode)])
                HideConstructionRights();
        }

        void onToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (screenCoords.x == kLocationNull)
                return;

            auto& gameState = getGameState();
            if (IsOwnershipMode())
            {
                auto landSetRightsAction = GetLandSetAction();
                GameActions::Execute(&landSetRightsAction, gameState);
            }
            else
            {
                auto landBuyRightsAction = GetLandBuyAction();
                GameActions::Execute(&landBuyRightsAction, gameState);
            }
        }

        void onToolDrag(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            onToolDown(widgetIndex, screenCoords);
        }

    private:
        void InputSize()
        {
            Formatter ft;
            ft.Add<uint16_t>(kLandToolMinimumSize);
            ft.Add<uint16_t>(kLandToolMaximumSize);
            WindowTextInputOpen(
                this, WIDX_PREVIEW, STR_SELECTION_SIZE, STR_ENTER_SELECTION_SIZE, ft, kStringIdNone, kStringIdNone, 3);
        }

    public:
        // ---- graph accessibility recipe ----
        //
        // A list that acts on the map cursor's area - its brush, or the marked rectangle - then
        // closes the window. The list's heading says which tiles that is, so choosing is the
        // confirmation. The window's tool size is left out: the map cursor's brush (B) is the area.
        //
        // In the scenario editor and sandbox mode: the five ownership settings, opening on the one
        // the window has chosen, applied with the window's own land rights action, the one its mouse
        // tool runs over a dragged area. In a park: the window's two buying options, each with what
        // it would cost there.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            onPrepareDraw(); // the pressed setting and which settings show

            b.PushContext(Vocab::LandRightsAppliesTo(Accessibility::DescribeKeyboardToolArea()), Vocab::kRoleList);
            if (!IsOwnershipMode())
            {
                for (const WidgetIndex widx : { WIDX_BUY_LAND_RIGHTS, WIDX_BUY_CONSTRUCTION_RIGHTS })
                    AddBuyItem(b, widx);
                b.PopContext();
                return;
            }
            for (const WidgetIndex widx : { WIDX_LAND_OWNED_CHECKBOX, WIDX_LAND_SALE_CHECKBOX,
                                            WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX, WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX,
                                            WIDX_UNOWNED_LAND_CHECKBOX })
            {
                if (!widgets[widx].isVisible())
                    continue;
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(FormatStringID(widgets[widx].text)));
                vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleCheckbox, AnnouncementKinds::kRole));
                vt.announcements.emplace_back(
                    NodeAnnouncement::Static(isWidgetPressed(widx) ? Vocab::kChecked : "", AnnouncementKinds::kSelected));
                vt.onActivate = [this, widx]() { axApply(widx); };
                vt.focusRect = [this, widx]() -> std::optional<GraphRect> {
                    const auto& wd = widgets[widx];
                    return GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1, wd.height() + 1 };
                };
                b.AddItem(ControlId::Structural("rights:" + std::to_string(widx)), std::move(vt));
            }
            b.PopContext();
        }

    private:
        // A buying option: its name, what it would cost over the area (asked of the game with the
        // action Enter runs), and "unavailable" when the park has none of it left for sale, which is
        // when the game greys the option out.
        void AddBuyItem(Accessibility::Graph::GraphBuilder& b, WidgetIndex widx)
        {
            using namespace Accessibility::Graph;
            const bool rights = widx == WIDX_BUY_CONSTRUCTION_RIGHTS;
            const bool unavailable = isWidgetDisabled(widx);

            std::string price;
            if (!unavailable && !getGameState().park.flags.has(ParkFlag::noMoney))
            {
                auto action = GameActions::LandBuyRightsAction(
                    Accessibility::KeyboardToolArea(),
                    rights ? GameActions::LandBuyRightSetting::buyConstructionRights
                           : GameActions::LandBuyRightSetting::buyLand);
                const auto res = GameActions::Query(&action, getGameState());
                if (res.error == GameActions::Status::ok)
                    price = res.cost != 0 ? Accessibility::SpokenMoney(res.cost) : Vocab::kNothingForSaleHere;
            }

            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(rights ? Vocab::kBuyConstructionRights : Vocab::kBuyLand));
            vt.announcements.emplace_back(NodeAnnouncement::Static(price, AnnouncementKinds::kValue));
            vt.announcements.emplace_back(
                NodeAnnouncement::Static(unavailable ? Vocab::kUnavailable : "", AnnouncementKinds::kEnabled));
            vt.onActivate = [this, widx, rights]() {
                if (isWidgetDisabled(widx))
                {
                    // The game's own reason, the tooltip the greyed-out option shows.
                    Accessibility::ScreenReaderSpeak(FormatStringID(widgets[widx].tooltip));
                    return;
                }
                onMouseUp(widx); // the window's own switch to that option
                Accessibility::BuyLandRightsInKeyboardArea(rights);
                close();
            };
            vt.focusRect = [this, widx]() -> std::optional<GraphRect> {
                const auto& wd = widgets[widx];
                return GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1, wd.height() + 1 };
            };
            b.AddItem(ControlId::Structural("buy:" + std::to_string(widx)), std::move(vt));
        }

        void axApply(WidgetIndex widx)
        {
            onMouseUp(widx); // the window's own switch to that setting

            const std::string setting = FormatStringID(widgets[widx].text);
            const std::string area = Accessibility::DescribeKeyboardToolArea();
            auto action = GameActions::LandSetRightsAction(
                Accessibility::KeyboardToolArea(), GameActions::LandSetRightSetting::setOwnershipWithChecks,
                GetDesiredOwnership());
            // A game action raised from the UI runs a tick later (trap 12.2): say it when it lands.
            // Queued, so it follows whatever the closing window hands focus back to says.
            action.SetCallback([setting, area](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error != GameActions::Status::ok)
                    return; // the error window speaks
                Accessibility::ScreenReaderSpeak(Vocab::LandRightsApplied(setting, area), false);
            });
            GameActions::Execute(&action, getGameState());
            close();
        }
    };

    void RegisterLandRightsGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::landRights;
        screen.helpText = [](WindowBase&) {
            namespace Help = Accessibility::Graph::Vocab::Help;
            const bool ownership = isInEditorMode() || getGameState().cheats.sandboxMode;
            return std::string(ownership ? Help::kLandRights : Help::kLandRights_Park);
        };
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<LandRightsWindow&>(w).BuildAccessGraph(b); };
        screen.screenName = []() { return FormatStringID(kWindowTitle); };
        RegisterGraphScreen(std::move(screen));
    }

    WindowBase* LandRightsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<LandRightsWindow>(
            WindowClass::landRights, ScreenCoordsXY(ContextGetWidth() - kWindowSize.width, 29), kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows
