/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphVocab.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/input/MouseInput.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/interface/Cursors.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/management/Research.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/scenes/editor/EditorController.h>
#include <openrct2/ui/WindowManager.h>
#include <optional>
#include <string>
#include <unordered_set>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    namespace Vocab = Accessibility::Graph::Vocab;

#pragma region Widgets

    static constexpr ScreenSize kWindowSize = { 600, 400 };
    static constexpr StringId kWindowTitle = STR_INVENTION_LIST;

    enum WindowEditorInventionsListWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_RESIZE,
        WIDX_TAB_1,
        WIDX_PRE_RESEARCHED_SCROLL,
        WIDX_RESEARCH_ORDER_SCROLL,
        WIDX_PREVIEW,
        WIDX_MOVE_ITEMS_TO_TOP,
        WIDX_MOVE_ITEMS_TO_BOTTOM,
        WIDX_RANDOM_SHUFFLE
    };

    // clang-format off
    static constexpr auto _inventionListWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({  0,  43}, {600, 357}, WidgetType::resize,  WindowColour::secondary                                             ),
        makeTab   ({  3,  17}                                                                                                       ),
        makeWidget({  4,  56}, {368, 161}, WidgetType::scroll,  WindowColour::secondary, SCROLL_VERTICAL                            ),
        makeWidget({  4, 231}, {368, 157}, WidgetType::scroll,  WindowColour::secondary, SCROLL_VERTICAL                            ),
        makeWidget({431, 106}, {114, 114}, WidgetType::flatBtn, WindowColour::secondary                                             ),
        makeWidget({375, 343}, {220,  14}, WidgetType::button,  WindowColour::secondary, STR_MOVE_ALL_TOP                           ),
        makeWidget({375, 358}, {220,  14}, WidgetType::button,  WindowColour::secondary, STR_MOVE_ALL_BOTTOM                        ),
        makeWidget({375, 373}, {220,  14}, WidgetType::button,  WindowColour::secondary, STR_RANDOM_SHUFFLE,  STR_RANDOM_SHUFFLE_TIP)
    );

    static constexpr auto _inventionListDragWidgets = makeWidgets(
        makeWidget({0, 0}, {150, 14}, WidgetType::imgBtn, WindowColour::primary)
    );
    // clang-format on

#pragma endregion

    static void WindowEditorInventionsListDragOpen(
        ResearchItem* researchItem, const ScreenCoordsXY& editorPos, int objectSelectionScrollWidth);
    static const ResearchItem* WindowEditorInventionsListDragGetItem();

    /**
     *
     *  rct2: 0x0068596F
     * Sets rides that are in use to be always researched
     */
    static void ResearchRidesSetup()
    {
        // Reset all objects to not required
        for (auto objectType : getTransientObjectTypes())
        {
            auto maxObjects = getObjectEntryGroupCount(objectType);
            for (auto i = 0u; i < maxObjects; i++)
            {
                Editor::ClearSelectedObject(objectType, i);
            }
        }

        // Set research required for rides in use
        const auto& gameState = getGameState();
        for (const auto& ride : RideManager(gameState))
        {
            Editor::SetSelectedObject(
                ObjectType::ride, ride.subtype, { ObjectSelectionFlag::selected, ObjectSelectionFlag::inUse });
        }
    }

    static void DrawResearchItem(
        RenderTarget& rt, const ResearchItem& researchItem, const int16_t& width, const ScreenCoordsXY& screenCoords,
        StringId format, TextPaint textPaint)
    {
        const StringId itemNameId = researchItem.getName();
        int16_t columnSplitOffset = width / 2;

        if (researchItem.type == Research::EntryType::ride
            && !GetRideTypeDescriptor(researchItem.baseRideType).flags.has(RtdFlag::listVehiclesSeparately))
        {
            const StringId rideTypeName = GetRideNaming(researchItem.baseRideType, GetRideEntryByIndex(researchItem.entryIndex))
                                              .Name;

            // Draw group name
            auto ft = Formatter();
            ft.Add<StringId>(rideTypeName);
            drawTextEllipsised(rt, screenCoords, columnSplitOffset - 11, format, ft, textPaint);

            // Draw vehicle name
            ft = Formatter();
            ft.Add<StringId>(itemNameId);
            drawTextEllipsised(
                rt, { screenCoords + ScreenCoordsXY{ columnSplitOffset, 0 } }, columnSplitOffset - 11, format, ft, textPaint);
        }
        else
        {
            // Scenery group, flat ride or shopdis
            auto ft = Formatter();
            ft.Add<StringId>(itemNameId);
            drawTextEllipsised(rt, screenCoords, width, format, ft, textPaint);
        }
    }

#pragma region Invention List Window
    struct InventionListItem
    {
        ResearchItem* research = nullptr;
        bool isInvented = true;
    };

    class InventionListWindow final : public Window
    {
        ResearchItem* _selectedResearchItem;

    public:
        void onOpen() override
        {
            ResearchRidesSetup();

            setWidgets(_inventionListWidgets);
            widgetSetPressed(*this, WIDX_PREVIEW, true);
            widgetSetPressed(*this, WIDX_TAB_1, true);
            initScrollWidgets();
            selectedTab = 0;
            _selectedResearchItem = nullptr;

            WindowSetResize(*this, kWindowSize, { kWindowSize.width * 2, kWindowSize.height * 2 });
        }

        void onClose() override
        {
            ResearchRemoveFlags();

            // When used in-game (as a cheat)
            if (!isInEditorMode())
            {
                gSilentResearch = true;
                ResearchResetCurrentItem();
                gSilentResearch = false;
            }
        }

        void onMouseUp(WidgetIndex widx) override
        {
            switch (widx)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_RANDOM_SHUFFLE:
                    ResearchItemsShuffle();
                    invalidate();
                    break;
                case WIDX_MOVE_ITEMS_TO_TOP:
                    ResearchItemsMakeAllResearched();
                    initScrollWidgets();
                    invalidate();
                    break;
                case WIDX_MOVE_ITEMS_TO_BOTTOM:
                    ResearchItemsMakeAllUnresearched();
                    initScrollWidgets();
                    invalidate();
                    break;
            }
        }

        void onResize() override
        {
            if (width < minWidth)
            {
                invalidate();
                width = minWidth;
            }
            if (height < minHeight)
            {
                invalidate();
                height = minHeight;
            }
        }

        void onUpdate() override
        {
            currentFrame++;
            onPrepareDraw();

            invalidateWidget(WIDX_TAB_1);

            if (WindowEditorInventionsListDragGetItem() != nullptr)
                return;
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            const auto& gameState = getGameState();
            ScreenSize size{};
            if (scrollIndex == 0)
            {
                size.height = static_cast<int32_t>(gameState.researchItemsInvented.size()) * kScrollableRowHeight;
            }
            else
            {
                size.height = static_cast<int32_t>(gameState.researchItemsUninvented.size()) * kScrollableRowHeight;
            }
            return size;
        }

        void onScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            auto* researchItem = GetItemFromScrollY(scrollIndex == 0, screenCoords.y);
            if (researchItem != _selectedResearchItem)
            {
                _selectedResearchItem = researchItem;
                invalidate();

                // Prevent always-researched items from being highlighted when hovered over
                if (researchItem != nullptr && researchItem->isAlwaysResearched())
                {
                    _selectedResearchItem = nullptr;
                }
            }
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            auto* researchItem = GetItemFromScrollY(scrollIndex == 0, screenCoords.y);
            if (researchItem == nullptr)
                return;

            // Disallow picking up always-researched items
            if (researchItem->isAlwaysResearched())
                return;

            invalidate();

            WindowEditorInventionsListDragOpen(researchItem, windowPos, widgets[WIDX_PRE_RESEARCHED_SCROLL].right);
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            const auto& gameState = getGameState();

            // Draw background
            auto paletteIndex = getColourMap(colours[1].colour).midLight;
            GfxClear(rt, paletteIndex);

            int16_t boxWidth = widgets[WIDX_RESEARCH_ORDER_SCROLL].width() - 1;
            int32_t itemY = -kScrollableRowHeight;
            auto* dragItem = WindowEditorInventionsListDragGetItem();

            const auto& researchList = scrollIndex == 0 ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            for (const auto& researchItem : researchList)
            {
                itemY += kScrollableRowHeight;
                if (itemY + kScrollableRowHeight < rt.y || itemY >= rt.y + rt.height)
                    continue;

                if (_selectedResearchItem == &researchItem)
                {
                    int32_t top, bottom;
                    if (dragItem == nullptr)
                    {
                        // Highlight
                        top = itemY;
                        bottom = itemY + kScrollableRowHeight - 1;
                    }
                    else
                    {
                        // Drop horizontal rule
                        top = itemY - 1;
                        bottom = itemY;
                    }

                    Rectangle::filter(rt, { 0, top, boxWidth, bottom }, FilterPaletteID::paletteDarken1);
                }

                if (dragItem != nullptr && researchItem == *dragItem)
                    continue;

                // TODO: this parameter by itself produces very light text.
                // It needs a {BLACK} token in the string to work properly.
                ColourWithFlags colour = { Drawing::Colour::black };
                FontStyle fontStyle = FontStyle::medium;
                auto darkness = TextDarkness::regular;

                if (researchItem.isAlwaysResearched())
                {
                    if (_selectedResearchItem == &researchItem && dragItem == nullptr)
                        darkness = TextDarkness::extraDark;
                    else
                        darkness = TextDarkness::dark;
                    colour = colours[1].withFlag(ColourFlag::inset, true);
                }

                DrawResearchItem(rt, researchItem, boxWidth, { 1, itemY }, STR_BLACK_STRING, { colour, fontStyle, darkness });
            }
        }

        CursorID onCursor(WidgetIndex widx, const ScreenCoordsXY& screenCoords, CursorID fallback) override
        {
            bool isInvented = false;

            switch (widx)
            {
                case WIDX_PRE_RESEARCHED_SCROLL:
                    isInvented = true;
                    break;
                case WIDX_RESEARCH_ORDER_SCROLL:
                    isInvented = false;
                    break;
                default:
                    return fallback;
            }

            // Use the open hand as cursor for items that can be picked up
            auto* researchItem = GetItemFromScrollY(isInvented, screenCoords.y);
            if (researchItem != nullptr && !researchItem->isAlwaysResearched())
            {
                return CursorID::handOpen;
            }
            return fallback;
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);

            // Tab image
            auto screenPos = windowPos + ScreenCoordsXY{ widgets[WIDX_TAB_1].left, widgets[WIDX_TAB_1].top };
            GfxDrawSprite(rt, ImageId(SPR_TAB_FINANCES_RESEARCH_0 + (currentFrame / 2) % 8), screenPos);

            // Pre-researched items label
            screenPos = windowPos
                + ScreenCoordsXY{ widgets[WIDX_PRE_RESEARCHED_SCROLL].left, widgets[WIDX_PRE_RESEARCHED_SCROLL].top - 11 };
            drawText(rt, screenPos - ScreenCoordsXY{ 0, 1 }, STR_INVENTION_PREINVENTED_ITEMS);

            // Research order label
            screenPos = windowPos
                + ScreenCoordsXY{ widgets[WIDX_RESEARCH_ORDER_SCROLL].left, widgets[WIDX_RESEARCH_ORDER_SCROLL].top - 11 };
            drawText(rt, screenPos - ScreenCoordsXY{ 0, 1 }, STR_INVENTION_TO_BE_INVENTED_ITEMS);

            // Preview background
            auto& bkWidget = widgets[WIDX_PREVIEW];
            Rectangle::fill(
                rt,
                { windowPos + ScreenCoordsXY{ bkWidget.left + 1, bkWidget.top + 1 },
                  windowPos + ScreenCoordsXY{ bkWidget.right - 1, bkWidget.bottom - 1 } },
                getColourMap(colours[1].colour).darkest);

            auto* researchItem = WindowEditorInventionsListDragGetItem();
            if (researchItem == nullptr || researchItem->isNull())
                researchItem = _selectedResearchItem;
            // If the research item is null or a list separator.
            if (researchItem == nullptr || researchItem->isNull())
                return;

            // Preview image
            ObjectType objectEntryType = ObjectType::sceneryGroup;
            if (researchItem->type == Research::EntryType::ride)
                objectEntryType = ObjectType::ride;

            auto chunk = ObjectEntryGetChunk(objectEntryType, researchItem->entryIndex);
            if (chunk == nullptr)
                return;

            // Draw preview
            const auto* object = ObjectEntryGetObject(objectEntryType, researchItem->entryIndex);
            if (object != nullptr)
            {
                RenderTarget clipRT;
                screenPos = windowPos + ScreenCoordsXY{ bkWidget.left + 1, bkWidget.top + 1 };
                const auto clipWidth = bkWidget.width() - 2;
                const auto clipHeight = bkWidget.height() - 2;
                if (ClipRenderTarget(clipRT, rt, screenPos, clipWidth, clipHeight))
                {
                    object->DrawPreview(clipRT, clipWidth, clipHeight);
                }
            }

            // Item name
            screenPos = windowPos + ScreenCoordsXY{ bkWidget.midX() + 1, bkWidget.bottom + 3 };
            const auto itemWidth = width - widgets[WIDX_RESEARCH_ORDER_SCROLL].right - 6;

            StringId drawString = STR_WINDOW_COLOUR_2_STRINGID;
            StringId stringId = researchItem->getName();
            auto ft = Formatter();

            if (researchItem->type == Research::EntryType::ride
                && !GetRideTypeDescriptor(researchItem->baseRideType).flags.has(RtdFlag::listVehiclesSeparately))
            {
                drawString = STR_WINDOW_COLOUR_2_STRINGID_STRINGID;
                StringId rideTypeName = GetRideNaming(researchItem->baseRideType, GetRideEntryByIndex(researchItem->entryIndex))
                                            .Name;
                ft.Add<StringId>(rideTypeName);
                ft.Add<StringId>(stringId);
            }
            else
            {
                ft.Add<StringId>(stringId);
            }

            drawTextEllipsised(rt, screenPos, itemWidth, drawString, ft, { TextAlignment::centre });
            screenPos.y += 15;

            // Item category
            screenPos.x = windowPos.x + widgets[WIDX_RESEARCH_ORDER_SCROLL].right + 4;
            ft = Formatter();
            ft.Add<StringId>(researchItem->getCategoryInventionString());
            drawText(rt, screenPos, STR_INVENTION_RESEARCH_GROUP, ft);
        }

        void onPrepareDraw() override
        {
            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);

            int16_t scrollListHeight = (height - 88) / 2;

            widgets[WIDX_PRE_RESEARCHED_SCROLL].bottom = 60 + scrollListHeight;
            widgets[WIDX_PRE_RESEARCHED_SCROLL].right = width - 229;

            widgets[WIDX_RESEARCH_ORDER_SCROLL].top = widgets[WIDX_PRE_RESEARCHED_SCROLL].bottom + 15;
            widgets[WIDX_RESEARCH_ORDER_SCROLL].bottom = widgets[WIDX_RESEARCH_ORDER_SCROLL].top + scrollListHeight;
            widgets[WIDX_RESEARCH_ORDER_SCROLL].right = width - 229;

            widgets[WIDX_PREVIEW].left = width - 169;
            widgets[WIDX_PREVIEW].right = width - 56;

            widgets[WIDX_MOVE_ITEMS_TO_TOP].top = height - 57;
            widgets[WIDX_MOVE_ITEMS_TO_TOP].bottom = height - 44;
            widgets[WIDX_MOVE_ITEMS_TO_TOP].left = width - 225;
            widgets[WIDX_MOVE_ITEMS_TO_TOP].right = width - 6;

            widgets[WIDX_MOVE_ITEMS_TO_BOTTOM].top = height - 42;
            widgets[WIDX_MOVE_ITEMS_TO_BOTTOM].bottom = height - 29;
            widgets[WIDX_MOVE_ITEMS_TO_BOTTOM].left = width - 225;
            widgets[WIDX_MOVE_ITEMS_TO_BOTTOM].right = width - 6;

            widgets[WIDX_RANDOM_SHUFFLE].top = height - 27;
            widgets[WIDX_RANDOM_SHUFFLE].bottom = height - 14;
            widgets[WIDX_RANDOM_SHUFFLE].left = width - 225;
            widgets[WIDX_RANDOM_SHUFFLE].right = width - 6;
        }

        // Get Research Item and Scroll Id (scroll id represents invented(0)/not invented(1)/failure(-1)
        std::optional<InventionListItem> GetResearchItemAt(const ScreenCoordsXY& screenCoords)
        {
            if (windowPos.x <= screenCoords.x && windowPos.y < screenCoords.y && windowPos.x + width > screenCoords.x
                && windowPos.y + height > screenCoords.y)
            {
                auto* windowMgr = GetWindowManager();
                WidgetIndex widgetIndex = windowMgr->FindWidgetFromPoint(*this, screenCoords);

                auto& widget = widgets[widgetIndex];
                if (widgetIndex == WIDX_PRE_RESEARCHED_SCROLL || widgetIndex == WIDX_RESEARCH_ORDER_SCROLL)
                {
                    gPressedWidget.widgetIndex = widgetIndex;
                    int32_t outScrollArea{};
                    ScreenCoordsXY outScrollCoords{};
                    int32_t outScrollId{};
                    widgetScrollGetPart(*this, &widget, screenCoords, outScrollCoords, &outScrollArea, &outScrollId);
                    if (outScrollArea == SCROLL_PART_VIEW)
                    {
                        const auto isInvented = outScrollId == 0;
                        int32_t scrollY = outScrollCoords.y + 6;
                        return InventionListItem{ GetItemFromScrollYIncludeSeps(isInvented, scrollY), isInvented };
                    }
                }
            }

            return std::nullopt;
        }

        bool IsResearchItemSelected(ResearchItem* item) const
        {
            return item == _selectedResearchItem;
        }

        // hack to fix #17544: onScrollMouseOver never gets called while dragging
        void SetSelectedResearchItem(ResearchItem* item)
        {
            _selectedResearchItem = item;
        }

        void MoveResearchItem(const ResearchItem& item, ResearchItem* beforeItem, bool isInvented)
        {
            auto& gameState = getGameState();
            _selectedResearchItem = nullptr;
            invalidate();

            uint32_t beforeItemRawValue = 0;
            if (beforeItem != nullptr)
                beforeItemRawValue = beforeItem->rawValue;

            if (item.rawValue == beforeItemRawValue)
                return;

            ResearchRemove(item);

            auto& researchList = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            if (beforeItem != nullptr)
            {
                for (size_t i = 0; i < researchList.size(); i++)
                {
                    if (researchList[i].rawValue == beforeItemRawValue)
                    {
                        researchList.insert((researchList.begin() + i), item);
                        return;
                    }
                }
            }

            // Still not found? Append to end of list.
            researchList.push_back(item);
        }

        // ---- graph accessibility recipe ----
        //
        // Each list, the buttons and (in the scenario editor) the Editor steps tab are their own
        // Tab-stops: the arrows stay inside one, and the navigator cycles them with Tab, returning to
        // where each was left. Moving an item is the mouse's drag and drop made into two presses: Enter picks an
        // item up, Enter on another item drops it in front of that one, in either list, and each list
        // offers an "End of list" spot while something is carried. Shift+Enter sends an item straight
        // to the end of the other list. Every move goes through MoveResearchItem - the drop the mouse
        // uses - and none of it is a game action, so the result is there to read when the key returns.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            onPrepareDraw(); // lay the widgets out for the current window size before boxing them

            // The carried item can leave the lists behind our back (Move all, a mouse drag): let go.
            if (_axCarried.has_value() && !axFind(_axCarried->rawValue).has_value())
                _axCarried.reset();

            std::unordered_set<uint32_t> seen;
            axBuildList(b, true, seen);
            axBuildList(b, false, seen);
            b.BeginStop("buttons");
            axAddButton(b, WIDX_MOVE_ITEMS_TO_TOP, Vocab::kInvAllToPreInvented);
            axAddButton(b, WIDX_MOVE_ITEMS_TO_BOTTOM, Vocab::kInvAllToInvent);
            axAddButton(b, WIDX_RANDOM_SHUFFLE, Vocab::kInvShuffled);

            // In the scenario editor only (never when opened from the Cheats menu).
            if (HasEditorStepsTab())
            {
                b.BeginStop("steps");
                BuildEditorStepsTab(b);
            }
        }

        // Escape puts a carried item back. Otherwise it closes the window only where the window can
        // be closed: in the scenario editor this is a step of the editor, with no close button, and
        // closing it would strand the player between steps.
        bool AccessEscape()
        {
            if (_axCarried.has_value())
            {
                _axCarried.reset();
                axNote(Vocab::kInvPutBack);
                return true;
            }
            return gLegacyScene == LegacyScene::scenarioEditor;
        }

    private:
        // The item being moved (Enter to pick up, Enter again to drop). Held by value: the lists are
        // vectors, and every move reshuffles them under any pointer.
        std::optional<ResearchItem> _axCarried;

        void axNote(std::string text)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::editorInventionList).nextLandingNote = std::move(text);
        }

        void axFocusOn(uint32_t rawValue)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::editorInventionList).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural("item:" + std::to_string(rawValue));
        }

        static const char* axListName(bool isInvented)
        {
            return isInvented ? Vocab::kInvPreInvented : Vocab::kInvToInvent;
        }

        // Which list an item is in, and where.
        struct AxPlace
        {
            bool isInvented;
            size_t index;
        };

        std::optional<AxPlace> axFind(uint32_t rawValue) const
        {
            const auto& gameState = getGameState();
            for (bool isInvented : { true, false })
            {
                const auto& list = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
                for (size_t i = 0; i < list.size(); i++)
                    if (list[i].rawValue == rawValue)
                        return AxPlace{ isInvented, i };
            }
            return std::nullopt;
        }

        // Read the way the list draws it: a ride listed by type shows the type, then the vehicle.
        static std::string axItemLabel(const ResearchItem& item)
        {
            std::string name = FormatStringID(item.getName());
            if (item.type == Research::EntryType::ride
                && !GetRideTypeDescriptor(item.baseRideType).flags.has(RtdFlag::listVehiclesSeparately))
            {
                const auto typeName = FormatStringID(
                    GetRideNaming(item.baseRideType, GetRideEntryByIndex(item.entryIndex)).Name);
                if (!typeName.empty() && typeName != name)
                    return typeName + ", " + name;
            }
            return name;
        }

        // `seen` guards node identity: a research list holding the same item twice (a damaged park)
        // would give two nodes one key, and the builder throws on that - leaving the window silent.
        void axBuildList(Accessibility::Graph::GraphBuilder& b, bool isInvented, std::unordered_set<uint32_t>& seen)
        {
            using namespace Accessibility::Graph;
            const auto& gameState = getGameState();
            const auto& list = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            const std::string side = isInvented ? "pre" : "todo";

            // Each list is its own Tab-stop, so the arrows stay inside it.
            b.BeginStop(side);
            b.PushContext(axListName(isInvented), Vocab::kRoleList);
            for (size_t i = 0; i < list.size(); i++)
            {
                const ResearchItem item = list[i]; // a copy: the build is thrown away before anything moves
                const uint32_t raw = item.rawValue;
                if (!seen.insert(raw).second)
                    continue;

                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(axItemLabel(item)));
                vt.announcements.emplace_back(NodeAnnouncement::Static(FormatStringID(item.getCategoryInventionString())));
                if (item.isAlwaysResearched())
                    vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kInvAlwaysInvented));
                vt.announcements.emplace_back(
                    [this, raw]() { return _axCarried.has_value() && _axCarried->rawValue == raw ? Vocab::kInvCarried : ""; },
                    true);
                vt.onActivate = [this, raw]() { axActivate(raw); };
                vt.onActivateShift = [this, raw]() { axSendAcross(raw); };
                vt.focusRect = [this, isInvented, i]() { return axRowRect(isInvented, static_cast<int32_t>(i)); };
                vt.onFocus = [this, isInvented, i]() { axScrollTo(isInvented, static_cast<int32_t>(i)); };
                b.AddItem(ControlId::Structural("item:" + std::to_string(raw)), std::move(vt));
            }

            // Somewhere to drop after the last item - which is also the only way into an empty list.
            if (_axCarried.has_value())
            {
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kInvEndOfList));
                vt.onActivate = [this, isInvented]() { axDrop(std::nullopt, isInvented); };
                vt.excludeFromSearch = true;
                b.AddItem(ControlId::Structural("end:" + side), std::move(vt));
            }
            else if (list.empty())
            {
                // An empty list still needs a node, or its heading is never heard.
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kInvEmpty));
                vt.excludeFromSearch = true;
                b.AddItem(ControlId::Structural("empty:" + side), std::move(vt));
            }
            b.PopContext();
        }

        void axAddButton(Accessibility::Graph::GraphBuilder& b, WidgetIndex widx, const char* done)
        {
            using namespace Accessibility::Graph;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(FormatStringID(widgets[widx].text)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleButton, AnnouncementKinds::kRole));
            // The window's own handler, so the buttons do exactly what a click does.
            vt.onActivate = [this, widx, done]() {
                _axCarried.reset();
                onMouseUp(widx);
                axNote(done);
            };
            vt.focusRect = [this, widx]() -> std::optional<GraphRect> {
                const auto& wd = widgets[widx];
                return GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1, wd.height() + 1 };
            };
            b.AddItem(ControlId::Structural("btn:" + std::to_string(widx)), std::move(vt));
        }

        // Enter on an item: pick it up, or drop what is carried in front of it.
        void axActivate(uint32_t raw)
        {
            const auto place = axFind(raw);
            if (!place.has_value())
                return;
            if (_axCarried.has_value())
            {
                if (_axCarried->rawValue == raw)
                {
                    _axCarried.reset();
                    axNote(Vocab::kInvPutBack);
                    return;
                }
                axDrop(place->index, place->isInvented);
                return;
            }

            const auto& gameState = getGameState();
            const auto& list = place->isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            if (list[place->index].isAlwaysResearched())
            {
                axNote(Vocab::kInvCannotMove);
                return;
            }
            _axCarried = list[place->index];
            axNote(Vocab::kInvPickedUp);
        }

        // Drop the carried item in front of the item at `beforeIndex` in a list, or at its end.
        void axDrop(std::optional<size_t> beforeIndex, bool isInvented)
        {
            if (!_axCarried.has_value())
                return;
            const ResearchItem carried = *_axCarried;
            _axCarried.reset();

            auto& gameState = getGameState();
            auto& list = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            // Like the mouse drop, never land among the always-invented items: slide past them.
            ResearchItem* before = nullptr;
            if (beforeIndex.has_value())
            {
                for (size_t i = *beforeIndex; i < list.size(); i++)
                {
                    if (!list[i].isAlwaysResearched())
                    {
                        before = &list[i];
                        break;
                    }
                }
            }
            MoveResearchItem(carried, before, isInvented);
            axFocusOn(carried.rawValue);
            axNote(Vocab::kInvMoved);
        }

        // Shift+Enter: straight to the end of the other list, keeping the player's place.
        void axSendAcross(uint32_t raw)
        {
            const auto place = axFind(raw);
            if (!place.has_value())
                return;
            auto& gameState = getGameState();
            auto& list = place->isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            const ResearchItem item = list[place->index];
            if (item.isAlwaysResearched())
            {
                axNote(Vocab::kInvCannotMove);
                return;
            }
            if (_axCarried.has_value() && _axCarried->rawValue == raw)
                _axCarried.reset();

            // Stay where the item was: on the one after it, or before it at the end of the list.
            std::optional<uint32_t> stay;
            if (place->index + 1 < list.size())
                stay = list[place->index + 1].rawValue;
            else if (place->index > 0)
                stay = list[place->index - 1].rawValue;

            MoveResearchItem(item, nullptr, !place->isInvented);
            if (stay.has_value())
                axFocusOn(*stay);
            axNote(Vocab::InvMovedTo(axItemLabel(item), axListName(!place->isInvented)));
        }

        Accessibility::Graph::GraphRect axRowRect(bool isInvented, int32_t row) const
        {
            const auto& lw = widgets[isInvented ? WIDX_PRE_RESEARCHED_SCROLL : WIDX_RESEARCH_ORDER_SCROLL];
            const int32_t viewTop = windowPos.y + lw.top;
            const int32_t viewBottom = windowPos.y + lw.bottom;
            const int32_t left = windowPos.x + lw.left;
            const int32_t right = windowPos.x + lw.right;

            const int32_t rowTop = viewTop + row * kScrollableRowHeight - scrolls[isInvented ? 0 : 1].contentOffsetY;
            int32_t top = std::max(rowTop, viewTop);
            int32_t bottom = std::min(rowTop + kScrollableRowHeight, viewBottom);
            if (bottom <= top) // scrolled out of view: box the whole list
            {
                top = viewTop;
                bottom = viewBottom;
            }
            return { left, top, right - left, bottom - top };
        }

        // Keep the list scrolled to the keyboard's row, so the focus box lands on something visible.
        void axScrollTo(bool isInvented, int32_t row)
        {
            const WidgetIndex widx = isInvented ? WIDX_PRE_RESEARCHED_SCROLL : WIDX_RESEARCH_ORDER_SCROLL;
            auto& scroll = scrolls[isInvented ? 0 : 1];
            const int32_t viewHeight = widgets[widx].height() - 1;
            const int32_t rowTop = row * kScrollableRowHeight;
            const int32_t rowBottom = rowTop + kScrollableRowHeight;

            int32_t offset = scroll.contentOffsetY;
            if (rowTop < offset)
                offset = rowTop;
            else if (rowBottom > offset + viewHeight)
                offset = rowBottom - viewHeight;
            else
                return; // already visible; don't fight the player's own scrolling

            const auto& gameState = getGameState();
            const size_t count = isInvented ? gameState.researchItemsInvented.size() : gameState.researchItemsUninvented.size();
            const int32_t maxOffset = std::max(0, static_cast<int32_t>(count) * kScrollableRowHeight - viewHeight);
            scroll.contentOffsetY = std::clamp(offset, 0, maxOffset);
            invalidateWidget(widx);
        }

        ResearchItem* GetItemFromScrollY(bool isInvented, int32_t y) const
        {
            auto& gameState = getGameState();
            auto& researchList = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            for (auto& researchItem : researchList)
            {
                y -= kScrollableRowHeight;
                if (y < 0)
                {
                    return &researchItem;
                }
            }

            return nullptr;
        }

        ResearchItem* GetItemFromScrollYIncludeSeps(bool isInvented, int32_t y) const
        {
            auto& gameState = getGameState();
            auto& researchList = isInvented ? gameState.researchItemsInvented : gameState.researchItemsUninvented;
            for (auto& researchItem : researchList)
            {
                y -= kScrollableRowHeight;
                if (y < 0)
                {
                    return &researchItem;
                }
            }
            return nullptr;
        }
    };

    /**
     *
     *  rct2: 0x00684E04
     */
    WindowBase* EditorInventionsListOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<InventionListWindow>(
            WindowClass::editorInventionList, kWindowSize,
            { WindowFlag::noScrolling, WindowFlag::resizable, WindowFlag::centreScreen });
    }

    void RegisterEditorInventionsListGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::editorInventionList;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<InventionListWindow&>(w).BuildAccessGraph(b); };
        screen.onEscape = [](WindowBase& w) { return static_cast<InventionListWindow&>(w).AccessEscape(); };
        // Lists only: Left/Right have no meaning on a row, and there are no pages to change to.
        screen.sideArrowsChangePage = false;
        RegisterGraphScreen(std::move(screen));
    }
#pragma endregion

#pragma region Drag item

    class InventionDragWindow final : public Window
    {
        ResearchItem _draggedItem;

    public:
        void onOpen() override
        {
            setWidgets(_inventionListDragWidgets);
            colours[1] = Drawing::Colour::white;
        }

        CursorID onCursor(const WidgetIndex widx, const ScreenCoordsXY& screenCoords, const CursorID defaultCursor) override
        {
            auto* windowMgr = GetWindowManager();
            auto* inventionListWindow = static_cast<InventionListWindow*>(
                windowMgr->FindByClass(WindowClass::editorInventionList));
            if (inventionListWindow != nullptr)
            {
                auto res = inventionListWindow->GetResearchItemAt(screenCoords);
                auto* research = res.has_value() ? res->research : nullptr;
                if (!inventionListWindow->IsResearchItemSelected(research))
                {
                    inventionListWindow->SetSelectedResearchItem(research);
                    inventionListWindow->invalidate();
                }
            }

            return CursorID::handClosed;
        }

        void onMoved(const ScreenCoordsXY& screenCoords) override
        {
            auto* windowMgr = GetWindowManager();
            auto* inventionListWindow = static_cast<InventionListWindow*>(
                windowMgr->FindByClass(WindowClass::editorInventionList));
            if (inventionListWindow == nullptr)
            {
                close();
                return;
            }
            std::optional<InventionListItem> res;
            // Skip always researched items, so that the dragged item gets placed underneath them
            auto newScreenCoords = screenCoords;
            do
            {
                res = inventionListWindow->GetResearchItemAt(newScreenCoords);
                newScreenCoords.y += kListRowHeight;
            } while (res.has_value() && res->research != nullptr && res->research->isAlwaysResearched());

            if (res.has_value())
            {
                inventionListWindow->MoveResearchItem(_draggedItem, res->research, res->isInvented);
            }

            windowMgr->InvalidateByClass(WindowClass::editorInventionList);
            close();
        }

        void onDraw(RenderTarget& rt) override
        {
            auto screenCoords = windowPos + ScreenCoordsXY{ 0, 2 };

            DrawResearchItem(
                rt, _draggedItem, width, screenCoords, STR_WINDOW_COLOUR_2_STRINGID,
                { ColourWithFlags{ Drawing::Colour::black }.withFlag(ColourFlag::withOutline, true) });
        }

        void init(ResearchItem& researchItem, const ScreenCoordsXY& editorPos, int objectSelectionScrollWidth)
        {
            _draggedItem = researchItem;

            widgets[0].right = objectSelectionScrollWidth;
            invalidate();
            windowPos = ScreenCoordsXY{ editorPos.x, gTooltipCursor.y - 7 };
            width = objectSelectionScrollWidth;
            invalidate();

            InputWindowPositionBegin(*this, 0, gTooltipCursor);
        }

        const ResearchItem& GetItem() const
        {
            return _draggedItem;
        }
    };

    static void WindowEditorInventionsListDragOpen(
        ResearchItem* researchItem, const ScreenCoordsXY& editorPos, int objectSelectionScrollWidth)
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->CloseByClass(WindowClass::editorInventionListDrag);
        auto* wnd = windowMgr->Create<InventionDragWindow>(
            WindowClass::editorInventionListDrag, { 10, 14 },
            { WindowFlag::stickToFront, WindowFlag::transparent, WindowFlag::noSnapping });
        if (wnd != nullptr)
        {
            wnd->init(*researchItem, editorPos, objectSelectionScrollWidth);
        }
    }

    static const ResearchItem* WindowEditorInventionsListDragGetItem()
    {
        auto* windowMgr = GetWindowManager();
        auto* wnd = static_cast<InventionDragWindow*>(windowMgr->FindByClass(WindowClass::editorInventionListDrag));
        if (wnd == nullptr)
        {
            return nullptr;
        }
        return &wnd->GetItem();
    }

#pragma endregion
} // namespace OpenRCT2::Ui::Windows
