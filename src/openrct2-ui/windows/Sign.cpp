/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../UiStringIds.h"

#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/scenery/LargeSceneryRemoveAction.h>
#include <openrct2/actions/scenery/SignSetNameAction.h>
#include <openrct2/actions/scenery/SignSetStyleAction.h>
#include <openrct2/actions/scenery/WallRemoveAction.h>
#include <algorithm>
#include <openrct2/config/Config.h>
#include <openrct2/core/EnumUtils.hpp>
#include <openrct2/drawing/Colour.h>
#include <openrct2/object/LargeSceneryEntry.h>
#include <openrct2/object/ObjectEntryManager.h>
#include <openrct2/object/WallSceneryEntry.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/tile_element/LargeSceneryElement.h>
#include <openrct2/world/tile_element/WallElement.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    static constexpr StringId kWindowTitle = STR_SIGN;
    static constexpr ScreenSize kWindowSize = { 113, 96 };

    enum WindowSignWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_VIEWPORT,
        WIDX_SIGN_TEXT,
        WIDX_SIGN_DEMOLISH,
        WIDX_MAIN_COLOUR,
        WIDX_TEXT_COLOUR
    };

    // clang-format off
    // 0x9AEE00
    static constexpr auto _signWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({                     3,                      17}, {85, 60}, WidgetType::viewport,  WindowColour::secondary                                                        ), // Viewport
        makeWidget({kWindowSize.width - 25,                      19}, {24, 24}, WidgetType::flatBtn,   WindowColour::secondary, ImageId(SPR_RENAME),   STR_CHANGE_SIGN_TEXT_TIP       ), // change sign button
        makeWidget({kWindowSize.width - 25,                      67}, {24, 24}, WidgetType::flatBtn,   WindowColour::secondary, ImageId(SPR_DEMOLISH), STR_DEMOLISH_SIGN_TIP          ), // demolish button
        makeWidget({                     5, kWindowSize.height - 16}, {12, 12}, WidgetType::colourBtn, WindowColour::secondary, kWidgetContentEmpty,   STR_SELECT_MAIN_SIGN_COLOUR_TIP), // Main colour
        makeWidget({                    17, kWindowSize.height - 16}, {12, 12}, WidgetType::colourBtn, WindowColour::secondary, kWidgetContentEmpty,   STR_SELECT_TEXT_COLOUR_TIP     )  // Text colour
    );
    // clang-format on

    class SignWindow final : public Window
    {
    private:
        bool _isSmall = false;
        ObjectEntryIndex _sceneryEntry = kObjectEntryIndexNull;
        Drawing::Colour _mainColour = {};
        Drawing::Colour _textColour = {};

        BannerIndex GetBannerIndex() const
        {
            return BannerIndex::FromUnderlying(number);
        }

        void ShowTextInput()
        {
            auto* banner = GetBanner(GetBannerIndex());
            if (banner != nullptr)
            {
                auto bannerText = banner->getText();
                WindowTextInputRawOpen(
                    this, WIDX_SIGN_TEXT, STR_SIGN_TEXT_TITLE, STR_SIGN_TEXT_PROMPT, {}, bannerText.c_str(), 32);
            }
        }

    public:
        void onOpen() override
        {
            setWidgets(_signWidgets);
            WindowInitScrollWidgets(*this);
        }

        bool initialise(WindowNumber windowNumber, const bool isSmall)
        {
            number = windowNumber;
            _isSmall = isSmall;
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                return false;
            }

            auto signViewPosition = banner->position.ToCoordsXY().ToTileCentre();
            auto* tileElement = BannerGetTileElement(GetBannerIndex());
            if (tileElement == nullptr)
                return false;

            int32_t viewZ = tileElement->getBaseZ();
            currentFrame = viewZ;

            if (_isSmall)
            {
                auto* wallElement = tileElement->asWall();
                if (wallElement == nullptr)
                {
                    return false;
                }
                _mainColour = wallElement->GetPrimaryColour();
                _textColour = wallElement->GetSecondaryColour();
                _sceneryEntry = wallElement->GetEntryIndex();
            }
            else
            {
                auto* sceneryElement = tileElement->asLargeScenery();
                if (sceneryElement == nullptr)
                {
                    return false;
                }
                _mainColour = sceneryElement->GetPrimaryColour();
                _textColour = sceneryElement->GetSecondaryColour();
                _sceneryEntry = sceneryElement->GetEntryIndex();
            }

            // Create viewport
            Widget& viewportWidget = widgets[WIDX_VIEWPORT];
            ViewportCreate(
                *this, windowPos + ScreenCoordsXY{ viewportWidget.left + 1, viewportWidget.top + 1 },
                viewportWidget.width() - 2, viewportWidget.height() - 2, Focus(CoordsXYZ{ signViewPosition, viewZ }));

            viewport->flags = Config::Get().general.alwaysShowGridlines ? VIEWPORT_FLAG_GRIDLINES : VIEWPORT_FLAG_NONE;
            invalidate();

            return true;
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                close();
                return;
            }

            auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_SIGN_DEMOLISH:
                {
                    auto* tileElement = BannerGetTileElement(GetBannerIndex());
                    if (tileElement == nullptr)
                    {
                        close();
                        return;
                    }
                    auto bannerCoords = banner->position.ToCoordsXY();

                    if (_isSmall)
                    {
                        CoordsXYZD wallLocation = { bannerCoords, tileElement->getBaseZ(), tileElement->getDirection() };
                        auto wallRemoveAction = GameActions::WallRemoveAction(wallLocation);
                        GameActions::Execute(&wallRemoveAction, gameState);
                    }
                    else
                    {
                        auto sceneryRemoveAction = GameActions::LargeSceneryRemoveAction(
                            { bannerCoords, tileElement->getBaseZ(), tileElement->getDirection() },
                            tileElement->asLargeScenery()->GetSequenceIndex());
                        GameActions::Execute(&sceneryRemoveAction, gameState);
                    }
                    break;
                }
                case WIDX_SIGN_TEXT:
                    ShowTextInput();
                    break;
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            Widget* widget = &widgets[widgetIndex];
            switch (widgetIndex)
            {
                case WIDX_MAIN_COLOUR:
                    WindowDropdownShowColour(this, widget, colours[1].withFlag(ColourFlag::translucent, true), _mainColour);
                    break;
                case WIDX_TEXT_COLOUR:
                    WindowDropdownShowColour(this, widget, colours[1].withFlag(ColourFlag::translucent, true), _textColour);
                    break;
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex) override
        {
            auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_MAIN_COLOUR:
                {
                    if (dropdownIndex == -1)
                        return;
                    _mainColour = ColourDropDownIndexToColour(dropdownIndex);
                    auto signSetStyleAction = GameActions::SignSetStyleAction(
                        GetBannerIndex(), _mainColour, _textColour, !_isSmall);
                    GameActions::Execute(&signSetStyleAction, gameState);
                    break;
                }
                case WIDX_TEXT_COLOUR:
                {
                    if (dropdownIndex == -1)
                        return;
                    _textColour = ColourDropDownIndexToColour(dropdownIndex);
                    auto signSetStyleAction = GameActions::SignSetStyleAction(
                        GetBannerIndex(), _mainColour, _textColour, !_isSmall);
                    GameActions::Execute(&signSetStyleAction, gameState);
                    break;
                }
                default:
                    return;
            }

            invalidate();
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            if (widgetIndex == WIDX_SIGN_TEXT && !text.empty())
            {
                auto signSetNameAction = GameActions::SignSetNameAction(GetBannerIndex(), std::string(text));
                GameActions::Execute(&signSetNameAction, getGameState());
            }
        }

        void onPrepareDraw() override
        {
            Widget* main_colour_btn = &widgets[WIDX_MAIN_COLOUR];
            Widget* text_colour_btn = &widgets[WIDX_TEXT_COLOUR];

            if (_isSmall)
            {
                auto* wallEntry = OpenRCT2::ObjectEntryManager::GetObjectEntry<WallSceneryEntry>(_sceneryEntry);

                main_colour_btn->type = WidgetType::empty;
                text_colour_btn->type = WidgetType::empty;
                if (wallEntry == nullptr)
                {
                    return;
                }
                if (wallEntry->flags & WALL_SCENERY_HAS_PRIMARY_COLOUR)
                {
                    main_colour_btn->type = WidgetType::colourBtn;
                }
                if (wallEntry->flags & WALL_SCENERY_HAS_SECONDARY_COLOUR)
                {
                    text_colour_btn->type = WidgetType::colourBtn;
                }
            }
            else
            {
                auto* sceneryEntry = OpenRCT2::ObjectEntryManager::GetObjectEntry<LargeSceneryEntry>(_sceneryEntry);

                main_colour_btn->type = WidgetType::empty;
                text_colour_btn->type = WidgetType::empty;
                if (sceneryEntry == nullptr)
                {
                    return;
                }
                if (sceneryEntry->flags.has(LargeSceneryFlag::hasPrimaryColour))
                {
                    main_colour_btn->type = WidgetType::colourBtn;
                }
                if (sceneryEntry->flags.has(LargeSceneryFlag::hasSecondaryColour))
                {
                    text_colour_btn->type = WidgetType::colourBtn;
                }
            }

            main_colour_btn->image = getColourButtonImage(_mainColour);
            text_colour_btn->image = getColourButtonImage(_textColour);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);

            if (viewport != nullptr)
            {
                WindowDrawViewport(rt, *this);
            }
        }

        void onViewportRotate() override
        {
            removeViewport();

            auto banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                return;
            }

            auto signViewPos = CoordsXYZ{ banner->position.ToCoordsXY().ToTileCentre(), currentFrame };

            // Create viewport
            Widget* viewportWidget = &widgets[WIDX_VIEWPORT];
            ViewportCreate(
                *this, windowPos + ScreenCoordsXY{ viewportWidget->left + 1, viewportWidget->top + 1 },
                viewportWidget->width() - 2, viewportWidget->height() - 2, Focus(CoordsXYZ{ signViewPos }));
            if (viewport != nullptr)
                viewport->flags = Config::Get().general.alwaysShowGridlines ? VIEWPORT_FLAG_GRIDLINES : VIEWPORT_FLAG_NONE;
            invalidate();
        }

        // --- Screen-reader keyboard navigation ---------------------------------------------------
        // A single vertical list: change text, main colour, text colour, demolish. Up/Down move;
        // Left/Right cycle the colour on a colour item; Enter changes text or demolishes.
    private:
        enum class AxKind
        {
            text,
            mainColour,
            textColour,
            demolish,
        };

        static std::string axColourName(Drawing::Colour c)
        {
            std::string s = Drawing::colourToString(c);
            for (auto& ch : s)
                if (ch == '_')
                    ch = ' ';
            return s;
        }

        std::vector<AxKind> buildAxItems()
        {
            std::vector<AxKind> items;
            items.push_back(AxKind::text);
            if (widgets[WIDX_MAIN_COLOUR].type == WidgetType::colourBtn)
                items.push_back(AxKind::mainColour);
            if (widgets[WIDX_TEXT_COLOUR].type == WidgetType::colourBtn)
                items.push_back(AxKind::textColour);
            items.push_back(AxKind::demolish);
            return items;
        }

        std::string axLabel(AxKind kind)
        {
            switch (kind)
            {
                case AxKind::text:
                {
                    auto* banner = GetBanner(GetBannerIndex());
                    std::string text = banner != nullptr ? std::string(banner->getText()) : std::string();
                    return "Sign text, " + (text.empty() ? std::string("blank") : text);
                }
                case AxKind::mainColour:
                    return "Main colour, " + axColourName(_mainColour);
                case AxKind::textColour:
                    return "Text colour, " + axColourName(_textColour);
                case AxKind::demolish:
                    return "Demolish sign";
            }
            return {};
        }

        void axApplyColours()
        {
            auto action = GameActions::SignSetStyleAction(GetBannerIndex(), _mainColour, _textColour, !_isSmall);
            GameActions::Execute(&action, getGameState());
            invalidate();
        }

        WidgetIndex axWidget(AxKind kind) const
        {
            switch (kind)
            {
                case AxKind::text:
                    return WIDX_SIGN_TEXT;
                case AxKind::mainColour:
                    return WIDX_MAIN_COLOUR;
                case AxKind::textColour:
                    return WIDX_TEXT_COLOUR;
                case AxKind::demolish:
                    break;
            }
            return WIDX_SIGN_DEMOLISH;
        }

    public:
        // ---- graph accessibility recipe (shared WindowClass::banner; see accessGraph* virtuals) ----

        // A single vertical list: sign text, the two colour pickers (Left/Right cycle in place), and
        // demolish. Enter edits the text or demolishes.
        void accessGraphBuild(Accessibility::Graph::GraphBuilder& b) override
        {
            using namespace Accessibility::Graph;
            for (const AxKind kind : buildAxItems())
            {
                NodeVtable vt;
                vt.announcements.emplace_back([this, kind]() { return axLabel(kind); });
                vt.focusRect = [this, kind]() -> std::optional<Accessibility::Graph::GraphRect> {
                    const WidgetIndex w = axWidget(kind);
                    if (w >= widgets.size() || widgets[w].type == WidgetType::empty)
                        return std::nullopt;
                    const auto& wd = widgets[w];
                    return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1,
                                                            wd.height() + 1 };
                };
                switch (kind)
                {
                    case AxKind::text:
                        vt.onActivate = [this]() { ShowTextInput(); }; // opens the accessible text-input window
                        break;
                    case AxKind::mainColour:
                        vt.onAdjust = [this](int32_t sign, bool) {
                            _mainColour = static_cast<Drawing::Colour>(
                                (EnumValue(_mainColour) + sign + Drawing::kColourNumNormal) % Drawing::kColourNumNormal);
                            axApplyColours();
                        };
                        vt.stateText = [this]() { return axColourName(_mainColour); };
                        break;
                    case AxKind::textColour:
                        vt.onAdjust = [this](int32_t sign, bool) {
                            _textColour = static_cast<Drawing::Colour>(
                                (EnumValue(_textColour) + sign + Drawing::kColourNumNormal) % Drawing::kColourNumNormal);
                            axApplyColours();
                        };
                        vt.stateText = [this]() { return axColourName(_textColour); };
                        break;
                    case AxKind::demolish:
                        vt.onActivate = [this]() { onMouseUp(WIDX_SIGN_DEMOLISH); }; // removes the sign, closes window
                        break;
                }
                b.AddItem(ControlId::Structural("sign:" + std::to_string(EnumValue(kind))), std::move(vt));
            }
        }
    };

    /**
     *
     *  rct2: 0x006BA305
     */
    WindowBase* SignOpen(WindowNumber number)
    {
        auto* windowMgr = GetWindowManager();
        auto* w = static_cast<SignWindow*>(windowMgr->BringToFrontByNumber(WindowClass::banner, number));

        if (w != nullptr)
            return w;

        w = windowMgr->Create<SignWindow>(WindowClass::banner, kWindowSize, {});

        if (w == nullptr)
            return nullptr;

        bool result = w->initialise(number, false);
        if (result != true)
            return nullptr;

        return w;
    }

    /**
     *
     *  rct2: 0x6E5F52
     */
    WindowBase* SignSmallOpen(WindowNumber number)
    {
        auto* windowMgr = GetWindowManager();
        auto* w = static_cast<SignWindow*>(windowMgr->BringToFrontByNumber(WindowClass::banner, number));

        if (w != nullptr)
            return w;

        w = windowMgr->Create<SignWindow>(WindowClass::banner, kWindowSize, {});

        if (w == nullptr)
            return nullptr;

        bool result = w->initialise(number, true);
        if (result != true)
            return nullptr;

        return w;
    }
} // namespace OpenRCT2::Ui::Windows
