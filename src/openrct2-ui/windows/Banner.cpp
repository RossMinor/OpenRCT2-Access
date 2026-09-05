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
#include <openrct2/actions/scenery/BannerRemoveAction.h>
#include <openrct2/actions/scenery/BannerSetNameAction.h>
#include <openrct2/actions/scenery/BannerSetStyleAction.h>
#include <algorithm>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Colour.h>
#include <openrct2/drawing/TextColour.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/object/BannerSceneryEntry.h>
#include <openrct2/object/ObjectEntryManager.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/tile_element/BannerElement.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    static constexpr StringId kWindowTitle = STR_BANNER_WINDOW_TITLE;
    static constexpr ScreenSize kWindowSize = { 113, 96 };

    enum WindowBannerWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_VIEWPORT,
        WIDX_BANNER_TEXT,
        WIDX_BANNER_NO_ENTRY,
        WIDX_BANNER_DEMOLISH,
        WIDX_MAIN_COLOUR,
        WIDX_TEXT_COLOUR_DROPDOWN,
        WIDX_TEXT_COLOUR_DROPDOWN_BUTTON
    };

    // clang-format off
    static constexpr StringId kBannerColouredTextFormats[] = {
        STR_TEXT_COLOUR_BLACK,        // TextColour::black
        STR_TEXT_COLOUR_GREY,         // TextColour::grey
        STR_TEXT_COLOUR_WHITE,        // TextColour::white
        STR_TEXT_COLOUR_RED,          // TextColour::red
        STR_TEXT_COLOUR_GREEN,        // TextColour::green
        STR_TEXT_COLOUR_YELLOW,       // TextColour::yellow
        STR_TEXT_COLOUR_TOPAZ,        // TextColour::topaz
        STR_TEXT_COLOUR_CELADON,      // TextColour::celadon
        STR_TEXT_COLOUR_BABYBLUE,     // TextColour::babyBlue
        STR_TEXT_COLOUR_PALELAVENDER, // TextColour::paleLavender
        STR_TEXT_COLOUR_PALEGOLD,     // TextColour::paleGold
        STR_TEXT_COLOUR_LIGHTPINK,    // TextColour::lightPink
        STR_TEXT_COLOUR_PEARLAQUA,    // TextColour::pearlAqua
        STR_TEXT_COLOUR_PALESILVER,   // TextColour::paleSilver
    };

    static constexpr auto window_banner_widgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({                     3,                      17}, {85, 60}, WidgetType::viewport,     WindowColour::secondary, 0x0FFFFFFFE                                           ), // tab content panel
        makeWidget({kWindowSize.width - 25,                      19}, {24, 24}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_RENAME),   STR_CHANGE_BANNER_TEXT_TIP     ), // change banner button
        makeWidget({kWindowSize.width - 25,                      43}, {24, 24}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_NO_ENTRY), STR_SET_AS_NO_ENTRY_BANNER_TIP ), // no entry button
        makeWidget({kWindowSize.width - 25,                      67}, {24, 24}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_DEMOLISH), STR_DEMOLISH_BANNER_TIP        ), // demolish button
        makeWidget({                     5, kWindowSize.height - 16}, {12, 12}, WidgetType::colourBtn,    WindowColour::secondary, 0xFFFFFFFF,            STR_SELECT_MAIN_SIGN_COLOUR_TIP), // main colour
        makeWidget({                    43, kWindowSize.height - 16}, {39, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                                        ), // text colour
        makeWidget({                    70, kWindowSize.height - 15}, {11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH,    STR_SELECT_TEXT_COLOUR_TIP     )  // text colour
    );
    // clang-format on

    class BannerWindow final : public Window
    {
    private:
        CoordsXYZ _bannerViewPos;

        void createViewport()
        {
            const auto& viewportWidget = widgets[WIDX_VIEWPORT];
            ViewportCreate(
                *this, windowPos + ScreenCoordsXY{ viewportWidget.left + 1, viewportWidget.top + 1 },
                viewportWidget.width() - 2, viewportWidget.height() - 2, Focus(_bannerViewPos));

            if (viewport != nullptr)
                viewport->flags = Config::Get().general.alwaysShowGridlines ? VIEWPORT_FLAG_GRIDLINES : VIEWPORT_FLAG_NONE;
            invalidate();
        }

        BannerIndex GetBannerIndex() const
        {
            return BannerIndex::FromUnderlying(number);
        }

        BannerElement* GetBannerElement()
        {
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                return nullptr;
            }

            for (auto* bannerElement : TileElementsView<BannerElement>(banner->position))
            {
                if (bannerElement->GetIndex() == GetBannerIndex())
                {
                    return bannerElement;
                }
            }

            return nullptr;
        }

    public:
        void onOpen() override
        {
            setWidgets(window_banner_widgets);
            WindowInitScrollWidgets(*this);
        }

        void initialise(WindowNumber _number)
        {
            number = _number;
            auto* banner = GetBanner(BannerIndex::FromUnderlying(number));

            auto* bannerElement = GetBannerElement();
            if (bannerElement == nullptr)
                return;

            _bannerViewPos = CoordsXYZ{ banner->position.ToCoordsXY().ToTileCentre(), bannerElement->getBaseZ() };
            createViewport();
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            Widget* widget = &widgets[widgetIndex];
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                close();
                return;
            }
            switch (widgetIndex)
            {
                case WIDX_MAIN_COLOUR:
                    WindowDropdownShowColour(this, widget, colours[1].withFlag(ColourFlag::translucent, true), banner->colour);
                    break;
                case WIDX_TEXT_COLOUR_DROPDOWN_BUTTON:

                    auto numItems = std::size(kBannerColouredTextFormats) - 1;
                    for (size_t i = 0; i < numItems; ++i)
                    {
                        gDropdown.items[i] = Dropdown::MenuLabel(kBannerColouredTextFormats[i + 1]);
                    }

                    // Switch to the dropdown box widget.
                    widget--;

                    WindowDropdownShowTextCustomWidth(
                        { widget->left + windowPos.x, widget->top + windowPos.y }, widget->height(), colours[1], 0,
                        Dropdown::Flag::StayOpen, numItems, widget->width() - 1 + 3);

                    gDropdown.items[EnumValue(banner->textColour) - 1].setChecked(true);
                    break;
            }
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
                case WIDX_BANNER_DEMOLISH:
                {
                    auto* bannerElement = GetBannerElement();
                    if (bannerElement == nullptr)
                        break;

                    auto bannerRemoveAction = GameActions::BannerRemoveAction(
                        { banner->position.ToCoordsXY(), bannerElement->getBaseZ(), bannerElement->GetPosition() });
                    GameActions::Execute(&bannerRemoveAction, gameState);
                    break;
                }
                case WIDX_BANNER_TEXT:
                    WindowTextInputRawOpen(
                        this, WIDX_BANNER_TEXT, STR_BANNER_TEXT, STR_ENTER_BANNER_TEXT, {}, banner->getText().c_str(), 32);
                    break;
                case WIDX_BANNER_NO_ENTRY:
                {
                    TextinputCancel();
                    auto bannerSetStyle = GameActions::BannerSetStyleAction(
                        GameActions::BannerSetStyleType::noEntry, GetBannerIndex(), !banner->flags.has(BannerFlag::noEntry));
                    GameActions::Execute(&bannerSetStyle, gameState);
                    break;
                }
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
                        break;

                    auto bannerSetStyle = GameActions::BannerSetStyleAction(
                        GameActions::BannerSetStyleType::primaryColour, GetBannerIndex(),
                        EnumValue(ColourDropDownIndexToColour(dropdownIndex)));
                    GameActions::Execute(&bannerSetStyle, gameState);
                    break;
                }
                case WIDX_TEXT_COLOUR_DROPDOWN_BUTTON:
                {
                    if (dropdownIndex == -1)
                        break;
                    auto bannerSetStyle = GameActions::BannerSetStyleAction(
                        GameActions::BannerSetStyleType::textColour, GetBannerIndex(), dropdownIndex + 1);
                    GameActions::Execute(&bannerSetStyle, gameState);
                    break;
                }
            }
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            if (widgetIndex == WIDX_BANNER_TEXT)
            {
                auto bannerSetNameAction = GameActions::BannerSetNameAction(GetBannerIndex(), std::string(text));
                GameActions::Execute(&bannerSetNameAction, getGameState());
            }
        }

        void onViewportRotate() override
        {
            removeViewport();
            createViewport();
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);

            if (viewport != nullptr)
            {
                WindowDrawViewport(rt, *this);
            }
        }

        void onPrepareDraw() override
        {
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
            {
                return;
            }

            Widget& colourBtn = widgets[WIDX_MAIN_COLOUR];
            colourBtn.type = WidgetType::empty;

            auto* bannerEntry = OpenRCT2::ObjectEntryManager::GetObjectEntry<BannerSceneryEntry>(banner->type);
            if (bannerEntry != nullptr && (bannerEntry->flags & BANNER_ENTRY_FLAG_HAS_PRIMARY_COLOUR))
            {
                colourBtn.type = WidgetType::colourBtn;
            }
            const bool noEntry = banner->flags.has(BannerFlag::noEntry);
            setWidgetPressed(WIDX_BANNER_NO_ENTRY, noEntry);
            setWidgetDisabled(WIDX_BANNER_TEXT, noEntry);
            setWidgetDisabled(WIDX_TEXT_COLOUR_DROPDOWN, noEntry);
            setWidgetDisabled(WIDX_TEXT_COLOUR_DROPDOWN_BUTTON, noEntry);
            colourBtn.image = getColourButtonImage(banner->colour);
            Widget& dropDownWidget = widgets[WIDX_TEXT_COLOUR_DROPDOWN];
            dropDownWidget.text = kBannerColouredTextFormats[EnumValue(banner->textColour)];
        }

        // --- Screen-reader keyboard navigation ---------------------------------------------------
        // A single vertical list of the banner's controls (no tabs). Up/Down move; Left/Right cycle
        // the colour on a colour item; Enter changes text, toggles no-entry, or demolishes.
    private:
        enum class AxKind
        {
            text,
            noEntry,
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
            auto* banner = GetBanner(GetBannerIndex());
            if (banner == nullptr)
                return items;
            const bool noEntry = banner->flags.has(BannerFlag::noEntry);
            if (!noEntry)
                items.push_back(AxKind::text);
            items.push_back(AxKind::noEntry);
            if (widgets[WIDX_MAIN_COLOUR].type == WidgetType::colourBtn)
                items.push_back(AxKind::mainColour);
            if (!noEntry)
                items.push_back(AxKind::textColour);
            items.push_back(AxKind::demolish);
            return items;
        }

        std::string axLabel(AxKind kind)
        {
            auto* banner = GetBanner(GetBannerIndex());
            switch (kind)
            {
                case AxKind::text:
                {
                    std::string text = banner != nullptr ? std::string(banner->getText()) : std::string();
                    return "Banner text, " + (text.empty() ? std::string("blank") : text);
                }
                case AxKind::noEntry:
                    return std::string("No entry banner, ")
                        + (banner != nullptr && banner->flags.has(BannerFlag::noEntry) ? "on" : "off");
                case AxKind::mainColour:
                    return "Sign colour, " + (banner != nullptr ? axColourName(banner->colour) : std::string());
                case AxKind::textColour:
                    return "Text colour, "
                        + (banner != nullptr ? OpenRCT2::FormatStringID(kBannerColouredTextFormats[EnumValue(banner->textColour)])
                                             : std::string());
                case AxKind::demolish:
                    return "Demolish banner";
            }
            return {};
        }

        WidgetIndex axWidget(AxKind kind) const
        {
            switch (kind)
            {
                case AxKind::text:
                    return WIDX_BANNER_TEXT;
                case AxKind::noEntry:
                    return WIDX_BANNER_NO_ENTRY;
                case AxKind::mainColour:
                    return WIDX_MAIN_COLOUR;
                case AxKind::textColour:
                    return WIDX_TEXT_COLOUR_DROPDOWN;
                case AxKind::demolish:
                    break;
            }
            return WIDX_BANNER_DEMOLISH;
        }

    public:
        // ---- graph accessibility recipe (shared WindowClass::banner; see accessGraph* virtuals) ----

        // A single vertical list: banner text, the no-entry toggle, the sign/text colours (Left/Right
        // cycle in place), and demolish. Enter edits text, toggles no-entry, or demolishes.
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
                        vt.onActivate = [this]() { onMouseUp(WIDX_BANNER_TEXT); }; // accessible text-input window
                        break;
                    case AxKind::noEntry:
                        vt.onActivate = [this]() { onMouseUp(WIDX_BANNER_NO_ENTRY); };
                        vt.stateText = [this]() {
                            auto* banner = GetBanner(GetBannerIndex());
                            return std::string(banner != nullptr && banner->flags.has(BannerFlag::noEntry) ? "on" : "off");
                        };
                        break;
                    case AxKind::mainColour:
                        vt.onAdjust = [this](int32_t sign, bool) {
                            auto* banner = GetBanner(GetBannerIndex());
                            if (banner == nullptr)
                                return;
                            const int32_t v = (EnumValue(banner->colour) + sign + Drawing::kColourNumNormal)
                                % Drawing::kColourNumNormal;
                            auto action = GameActions::BannerSetStyleAction(
                                GameActions::BannerSetStyleType::primaryColour, GetBannerIndex(), v);
                            GameActions::Execute(&action, getGameState());
                        };
                        vt.stateText = [this]() {
                            auto* banner = GetBanner(GetBannerIndex());
                            return banner != nullptr ? axColourName(banner->colour) : std::string();
                        };
                        break;
                    case AxKind::textColour:
                        vt.onAdjust = [this](int32_t sign, bool) {
                            auto* banner = GetBanner(GetBannerIndex());
                            if (banner == nullptr)
                                return;
                            // Selectable text colours are 1..13 (index 0, black, is not offered).
                            int32_t v = EnumValue(banner->textColour) + sign;
                            if (v < 1)
                                v = 13;
                            else if (v > 13)
                                v = 1;
                            auto action = GameActions::BannerSetStyleAction(
                                GameActions::BannerSetStyleType::textColour, GetBannerIndex(), v);
                            GameActions::Execute(&action, getGameState());
                        };
                        vt.stateText = [this]() {
                            auto* banner = GetBanner(GetBannerIndex());
                            return banner != nullptr
                                ? OpenRCT2::FormatStringID(kBannerColouredTextFormats[EnumValue(banner->textColour)])
                                : std::string();
                        };
                        break;
                    case AxKind::demolish:
                        vt.onActivate = [this]() { onMouseUp(WIDX_BANNER_DEMOLISH); }; // removes banner, closes window
                        break;
                }
                b.AddItem(ControlId::Structural("banner:" + std::to_string(EnumValue(kind))), std::move(vt));
            }
        }
    };

    /**
     *
     *  rct2: 0x006BA305
     */
    WindowBase* BannerOpen(WindowNumber number)
    {
        auto* windowMgr = GetWindowManager();
        auto w = static_cast<BannerWindow*>(windowMgr->BringToFrontByNumber(WindowClass::banner, number));

        if (w != nullptr)
            return w;

        w = windowMgr->Create<BannerWindow>(WindowClass::banner, kWindowSize, {});

        if (w != nullptr)
            w->initialise(number);

        return w;
    }
} // namespace OpenRCT2::Ui::Windows
