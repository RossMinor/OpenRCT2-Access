/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/accessibility/RideVisualDescriptions.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/TrackDesignDescription.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/ride/Construction.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/Editor.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/String.hpp>
#include <openrct2/core/UnitConversion.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/IDrawingEngine.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/ride/RideConstruction.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/TrackDesign.h>
#include <openrct2/ride/TrackDesignRepository.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>
#include <map>
#include <optional>
#include <vector>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    static constexpr StringId kWindowTitle = STR_SELECT_DESIGN;
    static constexpr ScreenSize kWindowSize = { 600, 441 };
    static constexpr int32_t kDebugPathHeight = 12;
    static constexpr int32_t kRotateAndSceneryButtonSize = 24;
    static constexpr int32_t kWindowPadding = 5;

    enum WindowTrackListWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_BACK,
        WIDX_FILTER_STRING,
        WIDX_FILTER_CLEAR,
        WIDX_TRACK_LIST,
        WIDX_TRACK_PREVIEW,
        WIDX_ROTATE,
        WIDX_TOGGLE_SCENERY,
    };

    VALIDATE_GLOBAL_WIDX(WC_TRACK_DESIGN_LIST, WIDX_ROTATE);

    // clang-format off
    static constexpr auto kFlatBtnSize = ScreenSize{kRotateAndSceneryButtonSize, kRotateAndSceneryButtonSize};
    static constexpr auto _trackListWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({  4,  18}, {218,  13},   WidgetType::tableHeader,  WindowColour::primary, STR_SELECT_OTHER_RIDE                                       ),
        makeWidget({  4,  32}, {124,  13},   WidgetType::textBox,      WindowColour::secondary                                                            ),
        makeWidget({130,  32}, { 92,  13},   WidgetType::button,       WindowColour::primary, STR_OBJECT_SEARCH_CLEAR                                     ),
        makeWidget({  4,  46}, {218, 381},   WidgetType::scroll,       WindowColour::primary, SCROLL_VERTICAL,         STR_CLICK_ON_DESIGN_TO_BUILD_IT_TIP),
        makeWidget({224,  18}, {372, 219},   WidgetType::flatBtn,      WindowColour::primary                                                              ),
        makeWidget({572, 405}, kFlatBtnSize, WidgetType::flatBtn,      WindowColour::primary, ImageId(SPR_ROTATE_ARROW),        STR_ROTATE_90_TIP                  ),
        makeWidget({572, 381}, kFlatBtnSize, WidgetType::flatBtn,      WindowColour::primary, ImageId(SPR_SCENERY),             STR_TOGGLE_SCENERY_TIP             )
    );
    // clang-format on

    constexpr uint16_t kTrackDesignIndexUnloaded = UINT16_MAX;

    RideSelection _window_track_list_item;

    class TrackListWindow final : public Window
    {
    private:
        std::vector<TrackDesignFileRef> _trackDesigns;
        utf8 _filterString[kUserStringMaxLength]{};
        std::vector<uint16_t> _filteredTrackIds;
        uint16_t _loadedTrackDesignIndex;
        std::unique_ptr<TrackDesign> _loadedTrackDesign;
        TrackDesignPreviewBuffer _trackDesignPreviewPixels{};
        bool _selectedItemIsBeingUpdated;
        bool _reloadTrackDesigns;
        u8string _windowTitle;

        // Accessibility: reading a design's details means importing the design file from disk, and
        // the graph navigator rebuilds and re-composes the focused line on every operation and once
        // per idle tick - so import each design once and keep whatever we derived from it.
        struct AccessDesignInfo
        {
            std::string detail;             // dimensions, ratings, description, visual summary
            std::vector<std::string> stats; // the detailed statistics, one readable line each
        };
        std::map<uint16_t, AccessDesignInfo> _accessInfo;

        void filterList()
        {
            _filteredTrackIds.clear();

            // Nothing to filter, so fill the list with all indices
            if (String::lengthOf(_filterString) == 0)
            {
                for (uint16_t i = 0; i < _trackDesigns.size(); i++)
                    _filteredTrackIds.push_back(i);

                return;
            }

            // Convert filter to uppercase
            const auto filterStringUpper = String::toUpper(_filterString);

            // Fill the set with indices for tracks that match the filter
            for (uint16_t i = 0; i < _trackDesigns.size(); i++)
            {
                const auto trackNameUpper = String::toUpper(_trackDesigns[i].name);
                if (trackNameUpper.find(filterStringUpper) != std::string::npos)
                {
                    _filteredTrackIds.push_back(i);
                }
            }

            // Ensure that the selected item is still in the list.
            if (static_cast<size_t>(selectedListItem) >= _filteredTrackIds.size())
            {
                selectedListItem = 0;
            }
        }

        void selectFromList(int32_t listIndex)
        {
            Audio::Play(Audio::SoundId::click1, 0, this->windowPos.x + (this->width / 2));
            if (gLegacyScene != LegacyScene::trackDesignsManager)
            {
                if (listIndex == 0)
                {
                    close();
                    RideConstructNew(_window_track_list_item);
                    return;
                }
                listIndex--;
            }

            // Displays a message if the ride can't load, fix #4080
            if (_loadedTrackDesign == nullptr)
            {
                ContextShowError(STR_CANT_BUILD_THIS_HERE, STR_TRACK_LOAD_FAILED_ERROR, {});
                return;
            }

            if (_loadedTrackDesign->gameStateData.hasFlag(TrackDesignGameStateFlag::SceneryUnavailable))
            {
                gTrackDesignSceneryToggle = true;
            }

            uint16_t trackDesignIndex = _filteredTrackIds[listIndex];
            TrackDesignFileRef* tdRef = &_trackDesigns[trackDesignIndex];
            if (gLegacyScene == LegacyScene::trackDesignsManager)
            {
                auto intent = Intent(WindowClass::manageTrackDesign);
                intent.PutExtra(INTENT_EXTRA_TRACK_DESIGN, tdRef);
                ContextOpenIntent(&intent);
            }
            else
            {
                if (_loadedTrackDesignIndex != kTrackDesignIndexUnloaded
                    && (_loadedTrackDesign->gameStateData.hasFlag(TrackDesignGameStateFlag::VehicleUnavailable)))
                {
                    ContextShowError(STR_THIS_DESIGN_WILL_BE_BUILT_WITH_AN_ALTERNATIVE_VEHICLE_TYPE, kStringIdNone, {});
                }

                auto intent = Intent(WindowClass::trackDesignPlace);
                intent.PutExtra(INTENT_EXTRA_TRACK_DESIGN, tdRef);
                ContextOpenIntent(&intent);
            }
        }

        int32_t getListItemFromPosition(const ScreenCoordsXY& screenCoords)
        {
            size_t maxItems = _filteredTrackIds.size();
            if (gLegacyScene != LegacyScene::trackDesignsManager)
            {
                // Extra item: custom design
                maxItems++;
            }

            int32_t index = screenCoords.y / kScrollableRowHeight;
            if (index < 0 || static_cast<uint32_t>(index) >= maxItems)
            {
                index = -1;
            }
            return index;
        }

        void loadDesignsList(RideSelection item)
        {
            auto repo = GetContext()->GetTrackDesignRepository();
            std::string entryName;
            if (item.Type < 0x80)
            {
                if (GetRideTypeDescriptor(item.Type).flags.has(RtdFlag::listVehiclesSeparately))
                {
                    entryName = GetRideEntryName(item.EntryIndex);
                }
            }
            _trackDesigns = repo->GetItemsForObjectEntry(item.Type, entryName);
            _accessInfo.clear(); // the cache is keyed by _trackDesigns index

            filterList();
        }

        bool loadDesignPreview(const u8string& path)
        {
            _loadedTrackDesign = TrackDesignImport(path.c_str());
            if (_loadedTrackDesign != nullptr)
            {
                TrackDesignDrawPreview(*_loadedTrackDesign, _trackDesignPreviewPixels, !gTrackDesignSceneryToggle);
                return true;
            }
            return false;
        }

    public:
        TrackListWindow(const RideSelection item)
        {
            _window_track_list_item = item;
        }

        void onOpen() override
        {
            String::set(_filterString, sizeof(_filterString), "");
            setWidgets(_trackListWidgets);
            widgets[WIDX_FILTER_STRING].string = _filterString;

            loadDesignsList(_window_track_list_item);

            WindowInitScrollWidgets(*this);
            _selectedItemIsBeingUpdated = false;
            _reloadTrackDesigns = false;
            // Start with first track highlighted
            selectedListItem = 0;
            if (!_trackDesigns.empty() && gLegacyScene != LegacyScene::trackDesignsManager)
            {
                selectedListItem = 1;
            }
            gTrackDesignSceneryToggle = false;
            WindowPushOthersRight(*this);
            _currentTrackPieceDirection = 2;
            std::fill(_trackDesignPreviewPixels.begin(), _trackDesignPreviewPixels.end(), PaletteIndex::transparent);

            _loadedTrackDesign = nullptr;
            _loadedTrackDesignIndex = kTrackDesignIndexUnloaded;

            // The graph navigator announces the screen and its landing row when it attaches.
        }

        void reopenTrackManager()
        {
            auto* windowMgr = GetWindowManager();
            windowMgr->CloseByNumber(WindowClass::manageTrackDesign, number);
            windowMgr->CloseByNumber(WindowClass::trackDeletePrompt, number);
            Editor::LoadTrackManager();
        }

        void onClose() override
        {
            // Dispose track design and preview
            _loadedTrackDesign = nullptr;

            // Dispose track list
            _trackDesigns.clear();

            // If gScreenAge is zero, we're already in the process
            // of loading the track manager, so we shouldn't try
            // to do it again. Otherwise, this window will get
            // another close signal from the track manager load function,
            // try to load the track manager again, and an infinite loop will result.
            if ((gLegacyScene == LegacyScene::trackDesignsManager) && gScreenAge != 0)
            {
                reopenTrackManager();
            }
        }

        void onMouseUp(const WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_ROTATE:
                    _currentTrackPieceDirection++;
                    _currentTrackPieceDirection %= 4;
                    invalidate();
                    break;
                case WIDX_TOGGLE_SCENERY:
                    gTrackDesignSceneryToggle = !gTrackDesignSceneryToggle;
                    _loadedTrackDesignIndex = kTrackDesignIndexUnloaded;
                    invalidate();
                    break;
                case WIDX_BACK:
                    close();
                    if (gLegacyScene != LegacyScene::trackDesignsManager)
                    {
                        ContextOpenWindow(WindowClass::constructRide);
                    }
                    else
                    {
                        reopenTrackManager();
                    }
                    break;
                case WIDX_FILTER_STRING:
                    WindowStartTextbox(*this, widgetIndex, _filterString, sizeof(_filterString));
                    break;
                case WIDX_FILTER_CLEAR:
                    // Keep the highlighted item selected
                    if (gLegacyScene == LegacyScene::trackDesignsManager)
                    {
                        if (selectedListItem != -1 && _filteredTrackIds.size() > static_cast<size_t>(selectedListItem))
                            selectedListItem = _filteredTrackIds[selectedListItem];
                        else
                            selectedListItem = -1;
                    }
                    else
                    {
                        if (selectedListItem != 0)
                            selectedListItem = _filteredTrackIds[selectedListItem - 1] + 1;
                    }

                    String::set(_filterString, sizeof(_filterString), "");
                    filterList();
                    invalidate();
                    break;
            }
        }

#pragma region Accessibility

        // Number of selectable rows: the track designs, plus the leading "Build custom design"
        // entry outside the track manager.
        int32_t accessibleItemCount() const
        {
            int32_t count = static_cast<int32_t>(_filteredTrackIds.size());
            if (gLegacyScene != LegacyScene::trackDesignsManager)
                count++;
            return count;
        }

        // Returns the _trackDesigns index behind a list row, or -1 for the "build custom design"
        // row or an out-of-range row.
        int32_t accessTrackIndexForRow(int32_t row) const
        {
            if (row < 0 || row >= accessibleItemCount())
                return -1;
            const bool customFirst = (gLegacyScene != LegacyScene::trackDesignsManager);
            if (customFirst && row == 0)
                return -1;
            const int32_t listIndex = customFirst ? row - 1 : row;
            if (listIndex < 0 || static_cast<size_t>(listIndex) >= _filteredTrackIds.size())
                return -1;
            return _filteredTrackIds[listIndex];
        }

        // Import a design once and keep everything derived from the file.
        const AccessDesignInfo& accessDesignInfo(uint16_t trackIndex)
        {
            auto it = _accessInfo.find(trackIndex);
            if (it != _accessInfo.end())
                return it->second;

            AccessDesignInfo info;
            auto design = TrackDesignImport(_trackDesigns[trackIndex].path.c_str());
            if (design == nullptr)
                return _accessInfo.emplace(trackIndex, std::move(info)).first->second;

            // Read order matches the visual info panel: dimensions, then the three headline
            // ratings, then the ride type's description and a data-derived visual summary.
            const auto& stats = design->statistics;
            const auto& rtd = GetRideTypeDescriptor(design->trackAndVehicle.rtdIndex);
            auto appendStat = [&info](StringId id, Formatter& ft) {
                info.detail += ". " + FormatStringIDLegacy(id, ft.Data());
            };

            if (!stats.spaceRequired.IsNull())
            {
                Formatter ft;
                ft.Add<uint16_t>(stats.spaceRequired.x);
                ft.Add<uint16_t>(stats.spaceRequired.y);
                appendStat(STR_TRACK_LIST_SPACE_REQUIRED, ft);
            }
            {
                Formatter ft;
                ft.Add<fixed32_2dp>(stats.ratings.excitement);
                appendStat(STR_TRACK_LIST_EXCITEMENT_RATING, ft);
            }
            {
                Formatter ft;
                ft.Add<fixed32_2dp>(stats.ratings.intensity);
                appendStat(STR_TRACK_LIST_INTENSITY_RATING, ft);
            }
            {
                Formatter ft;
                ft.Add<fixed32_2dp>(stats.ratings.nausea);
                appendStat(STR_TRACK_LIST_NAUSEA_RATING, ft);
            }

            const StringId descId = rtd.Naming.Description;
            if (descId != kStringIdNone && descId != kStringIdEmpty)
                info.detail += ". " + OpenRCT2::FormatStringID(descId);

            const std::string visual = Accessibility::DescribeTrackDesign(*design);
            if (!visual.empty())
                info.detail += ". " + visual;

            // Everything beyond the headline ratings hangs off the design as an expandable group,
            // so the default read-out stays manageable.
            info.stats = buildExtendedStats(*design);

            return _accessInfo.emplace(trackIndex, std::move(info)).first->second;
        }

        // The spoken line for one design row: name, cost when it is known, then the cached details.
        std::string accessDesignLine(uint16_t trackIndex)
        {
            std::string text = _trackDesigns[trackIndex].name;

            // Cost is not stored in a design file - it is only computed once the design is loaded
            // for preview - so include it only when this design is the loaded one.
            if (_loadedTrackDesign != nullptr && _loadedTrackDesignIndex == trackIndex
                && _loadedTrackDesign->gameStateData.cost != 0)
            {
                Formatter ft;
                ft.Add<uint32_t>(_loadedTrackDesign->gameStateData.cost);
                text += ". " + FormatStringIDLegacy(STR_TRACK_LIST_COST_AROUND, ft.Data());
            }

            return text + accessDesignInfo(trackIndex).detail;
        }

        // Builds the detailed statistics (everything beyond the three headline ratings), one
        // readable line each. Empty for rides without track (a maze has nothing to add).
        std::vector<std::string> buildExtendedStats(const TrackDesign& designRef)
        {
            std::vector<std::string> items;
            const TrackDesign* design = &designRef;

            const auto& stats = design->statistics;
            const auto& rtd = GetRideTypeDescriptor(design->trackAndVehicle.rtdIndex);
            auto add = [&items](StringId id, Formatter& ft) {
                items.push_back(FormatStringIDLegacy(id, ft.Data()));
            };

            if (!rtd.flags.has(RtdFlag::hasTrack))
                return items;

            if (rtd.specialType != RtdSpecialType::maze)
            {
                if (rtd.specialType == RtdSpecialType::miniGolf)
                {
                    Formatter ft;
                    ft.Add<uint16_t>(stats.holes);
                    add(STR_HOLES, ft);
                }
                else
                {
                    Formatter ft;
                    ft.Add<uint16_t>(ToHumanReadableSpeed(stats.maxSpeed << 16));
                    add(STR_MAX_SPEED, ft);

                    Formatter ft2;
                    ft2.Add<uint16_t>(ToHumanReadableSpeed(stats.averageSpeed << 16));
                    add(STR_AVERAGE_SPEED, ft2);
                }

                Formatter ft;
                ft.Add<StringId>(STR_RIDE_LENGTH_ENTRY);
                ft.Add<uint16_t>(stats.rideLength);
                add(STR_TRACK_LIST_RIDE_LENGTH, ft);
            }

            if (rtd.flags.has(RtdFlag::hasGForces))
            {
                {
                    Formatter ft;
                    ft.Add<int32_t>(stats.maxPositiveVerticalG);
                    add(STR_MAX_POSITIVE_VERTICAL_G, ft);
                }
                {
                    Formatter ft;
                    ft.Add<int32_t>(stats.maxNegativeVerticalG);
                    add(STR_MAX_NEGATIVE_VERTICAL_G, ft);
                }
                {
                    Formatter ft;
                    ft.Add<int32_t>(stats.maxLateralG);
                    add(STR_MAX_LATERAL_G, ft);
                }
                if (stats.totalAirTime != 0)
                {
                    Formatter ft;
                    ft.Add<int32_t>(ToHumanReadableAirTime(stats.totalAirTime));
                    add(STR_TOTAL_AIR_TIME, ft);
                }
            }

            if (rtd.flags.has(RtdFlag::hasDrops))
            {
                {
                    Formatter ft;
                    ft.Add<uint16_t>(stats.drops);
                    add(STR_DROPS, ft);
                }
                {
                    Formatter ft;
                    ft.Add<uint16_t>((stats.highestDropHeight * 3) / 4);
                    add(STR_HIGHEST_DROP_HEIGHT, ft);
                }
            }

            if (stats.inversions != 0)
            {
                Formatter ft;
                ft.Add<uint16_t>(stats.inversions);
                add(STR_INVERSIONS, ft);
            }

            return items;
        }

        // ---- graph accessibility recipe ----

        // Keep the window's own highlight (and the preview it lazily loads) in step with the
        // graph's focused row. Focus itself now lives in the graph rather than in selectedListItem,
        // which is what fixes the old drift: onScrollMouseOver rewrites selectedListItem from
        // wherever the mouse happens to be resting, so a keyboard cursor stored there wandered off
        // on its own between key presses.
        void accessSyncSelection(int32_t row)
        {
            if (row >= 0 && row < accessibleItemCount())
                selectedListItem = row;
        }

        std::optional<Accessibility::Graph::GraphRect> accessRowRect(int32_t row)
        {
            const auto& lw = widgets[WIDX_TRACK_LIST];
            const int32_t viewTop = windowPos.y + lw.top;
            const int32_t viewBottom = windowPos.y + lw.bottom;
            const int32_t left = windowPos.x + lw.left;
            const int32_t widthPx = lw.width() + 1;
            const auto wholeList = [&]() {
                return Accessibility::Graph::GraphRect{ left, viewTop, widthPx, lw.height() + 1 };
            };
            if (row < 0 || row >= accessibleItemCount())
                return wholeList();
            const int32_t rowTop = viewTop + row * kScrollableRowHeight - scrolls[0].contentOffsetY;
            const int32_t top = std::max(rowTop, viewTop);
            const int32_t bottom = std::min(rowTop + kScrollableRowHeight, viewBottom);
            if (bottom <= top)
                return wholeList();
            return Accessibility::Graph::GraphRect{ left, top, widthPx, bottom - top };
        }

        // Build (or place) the design on a row. selectFromList() reads _loadedTrackDesign, which is
        // otherwise only filled in by onDraw for whichever row was drawn last - so load the focused
        // design here first, or keyboard selection drops into the "design failed to load" error.
        void accessActivateRow(int32_t row)
        {
            if (_selectedItemIsBeingUpdated || row < 0 || row >= accessibleItemCount())
                return;

            selectedListItem = row;
            const int32_t trackIndex = accessTrackIndexForRow(row);
            if (trackIndex >= 0 && _loadedTrackDesignIndex != static_cast<uint16_t>(trackIndex))
            {
                if (loadDesignPreview(_trackDesigns[trackIndex].path))
                    _loadedTrackDesignIndex = static_cast<uint16_t>(trackIndex);
                else
                    _loadedTrackDesignIndex = kTrackDesignIndexUnloaded;
            }
            invalidate();
            selectFromList(row);
        }

        // Declare the design list: the leading "Build custom design" row outside the track manager,
        // then one node per design. A design is an expandable group - Right opens its detailed
        // statistics as child rows, Left closes them again (this replaces the old I-key sub-mode).
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            const int32_t count = accessibleItemCount();
            const bool customFirst = (gLegacyScene != LegacyScene::trackDesignsManager);
            const int32_t designs = static_cast<int32_t>(_filteredTrackIds.size());

            std::string header = _windowTitle.empty() ? std::string("Select design") : _windowTitle;
            header += (designs == 0) ? ", no designs"
                                     : ", " + std::to_string(designs) + (designs == 1 ? " design" : " designs");
            b.PushContext(header);

            if (count <= 0)
            {
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static("No designs available"));
                vt.excludeFromSearch = true;
                b.AddItem(ControlId::Structural("tl:none"), std::move(vt));
                b.PopContext();
                return;
            }

            int32_t row = 0;
            if (customFirst)
            {
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static("Build custom design"));
                vt.onActivate = [this]() { accessActivateRow(0); };
                vt.focusRect = [this]() {
                    accessSyncSelection(0);
                    return accessRowRect(0);
                };
                b.AddItem(ControlId::Structural("tl:custom"), std::move(vt));
                row = 1;
            }

            for (; row < count; row++)
            {
                const int32_t trackIndex = accessTrackIndexForRow(row);
                if (trackIndex < 0)
                    continue;
                const auto ti = static_cast<uint16_t>(trackIndex);

                // Keyed by design file, not by row, so focus stays on the same design when the
                // filter text changes the list under it.
                const auto& path = _trackDesigns[ti].path;
                const auto id = ControlId::Structural("tl:" + path);

                NodeVtable vt;
                vt.announcements.emplace_back([this, ti]() { return accessDesignLine(ti); });
                vt.searchText = [this, ti]() { return _trackDesigns[ti].name; };
                vt.onActivate = [this, row]() { accessActivateRow(row); };
                vt.focusRect = [this, row]() {
                    accessSyncSelection(row);
                    return accessRowRect(row);
                };

                b.BeginGroup(id, std::move(vt));
                if (b.IsExpanded(id))
                {
                    const auto& stats = accessDesignInfo(ti).stats;
                    for (size_t i = 0; i < stats.size(); i++)
                    {
                        NodeVtable sv;
                        sv.announcements.emplace_back(NodeAnnouncement::Static(stats[i]));
                        sv.excludeFromSearch = true;
                        sv.focusRect = [this, row]() {
                            accessSyncSelection(row);
                            return accessRowRect(row);
                        };
                        b.AddItem(ControlId::Structural("tl:stat:" + path + ":" + std::to_string(i)), std::move(sv));
                    }
                }
                b.EndGroup();
            }

            b.PopContext();
        }

#pragma endregion

        ScreenSize onScrollGetSize(const int32_t scrollIndex) override
        {
            size_t numItems = _filteredTrackIds.size();
            if (gLegacyScene != LegacyScene::trackDesignsManager)
            {
                // Extra item: custom design
                numItems++;
            }
            int32_t scrollHeight = static_cast<int32_t>(numItems * kScrollableRowHeight);

            return { width, scrollHeight };
        }

        void onScrollMouseDown(const int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (!_selectedItemIsBeingUpdated)
            {
                int32_t i = getListItemFromPosition(screenCoords);
                if (i != -1)
                {
                    selectFromList(i);
                }
            }
        }

        void onScrollMouseOver(const int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            // The graph navigator owns selectedListItem while it drives this window; letting an
            // idle mouse pointer rewrite it every frame is what made keyboard movement wander and
            // put the read-out out of step with what Enter would actually build.
            if (Accessibility::Graph::GraphOwnsWindowClass(WindowClass::trackDesignList))
                return;

            if (!_selectedItemIsBeingUpdated)
            {
                int32_t i = getListItemFromPosition(screenCoords);
                if (i != -1 && selectedListItem != i)
                {
                    selectedListItem = i;
                    invalidate();
                }
            }
        }

        void onTextInput(const WidgetIndex widgetIndex, std::string_view text) override
        {
            if (widgetIndex != WIDX_FILTER_STRING)
                return;

            if (String::equals(_filterString, std::string(text).c_str()))
                return;

            String::set(_filterString, sizeof(_filterString), std::string(text).c_str());

            filterList();

            scrolls->contentOffsetY = 0;

            invalidate();
        }

        void onPrepareDraw() override
        {
            StringId stringId = kStringIdNone;
            const auto* entry = GetRideEntryByIndex(_window_track_list_item.EntryIndex);

            if (entry != nullptr)
            {
                RideNaming rideName = GetRideNaming(_window_track_list_item.Type, entry);
                stringId = rideName.Name;
            }

            StringId titleFormat;
            if (gLegacyScene == LegacyScene::trackDesignsManager)
            {
                titleFormat = STR_TRACK_DESIGNS;
                widgets[WIDX_TRACK_LIST].tooltip = STR_CLICK_ON_DESIGN_TO_RENAME_OR_DELETE_IT;
            }
            else
            {
                titleFormat = STR_SELECT_DESIGN;
                widgets[WIDX_TRACK_LIST].tooltip = STR_CLICK_ON_DESIGN_TO_BUILD_IT_TIP;
            }
            _windowTitle = FormatStringID(titleFormat, stringId);
            widgets[WIDX_TITLE].setString(_windowTitle.c_str());

            const bool showPreview = (gLegacyScene == LegacyScene::trackDesignsManager) || selectedListItem != 0;
            setWidgetPressed(WIDX_TRACK_PREVIEW, showPreview);
            setWidgetDisabled(WIDX_TRACK_PREVIEW, !showPreview);
            if (showPreview)
            {
                widgets[WIDX_ROTATE].type = WidgetType::flatBtn;
                widgets[WIDX_TOGGLE_SCENERY].type = WidgetType::flatBtn;
                setWidgetPressed(WIDX_TOGGLE_SCENERY, !gTrackDesignSceneryToggle);
            }
            else
            {
                widgets[WIDX_ROTATE].type = WidgetType::empty;
                widgets[WIDX_TOGGLE_SCENERY].type = WidgetType::empty;
            }

            // When debugging tools are on, shift everything up a bit to make room for displaying the path.
            const int32_t bottomMargin = Config::Get().general.debuggingTools ? (kWindowPadding + kDebugPathHeight)
                                                                              : kWindowPadding;
            widgets[WIDX_TRACK_LIST].bottom = height - bottomMargin;
            widgets[WIDX_ROTATE].bottom = height - bottomMargin;
            widgets[WIDX_ROTATE].top = widgets[WIDX_ROTATE].bottom - kRotateAndSceneryButtonSize;
            widgets[WIDX_TOGGLE_SCENERY].bottom = widgets[WIDX_ROTATE].top;
            widgets[WIDX_TOGGLE_SCENERY].top = widgets[WIDX_TOGGLE_SCENERY].bottom - kRotateAndSceneryButtonSize;
        }

        void onUpdate() override
        {
            if (GetCurrentTextBox().window.classification == classification && GetCurrentTextBox().window.number == number)
            {
                WindowUpdateTextboxCaret();
                invalidateWidget(WIDX_FILTER_STRING); // TODO Check this
            }

            if (_reloadTrackDesigns)
            {
                loadDesignsList(_window_track_list_item);
                selectedListItem = 0;
                invalidate();
                _reloadTrackDesigns = false;
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);

            int32_t listItemIndex = selectedListItem;
            if ((gLegacyScene == LegacyScene::trackDesignsManager) == 0)
            {
                // Because the first item in the list is "Build a custom design", lower the index by one
                listItemIndex--;
            }

            if (_filteredTrackIds.empty() || listItemIndex == -1)
                return;

            int32_t trackIndex = _filteredTrackIds[listItemIndex];

            // Track preview
            auto& tdWidget = widgets[WIDX_TRACK_PREVIEW];
            auto colour = getColourMap(colours[0].colour).darkest;
            u8string path = _trackDesigns[trackIndex].path;

            // Show track file path (in debug mode)
            if (Config::Get().general.debuggingTools)
            {
                const auto shortPath = shortenPath(path, width, FontStyle::medium);
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 0, height - kDebugPathHeight - 3 }, shortPath,
                    { colours[1] }); // TODO Check dpi
            }

            auto screenPos = windowPos + ScreenCoordsXY{ tdWidget.left + 1, tdWidget.top + 1 };
            Rectangle::fill(rt, { screenPos, screenPos + ScreenCoordsXY{ 369, 216 } }, colour); // TODO Check dpi

            if (_loadedTrackDesignIndex != trackIndex)
            {
                if (loadDesignPreview(path))
                {
                    _loadedTrackDesignIndex = trackIndex;
                }
                else
                {
                    _loadedTrackDesignIndex = kTrackDesignIndexUnloaded;
                }
            }

            if (!_loadedTrackDesign)
            {
                return;
            }

            auto trackPreview = screenPos;
            screenPos = windowPos + ScreenCoordsXY{ tdWidget.midX(), tdWidget.midY() };

            G1Element g1temp = {};
            g1temp.offset = reinterpret_cast<uint8_t*>(
                _trackDesignPreviewPixels.data() + (_currentTrackPieceDirection * kTrackPreviewImageSize));
            g1temp.width = 370;
            g1temp.height = 217;
            g1temp.flags = { G1Flag::hasTransparency };
            GfxSetG1Element(SPR_TEMP_TRACK_LIST, &g1temp);
            DrawingEngineInvalidateImage(SPR_TEMP_TRACK_LIST);
            GfxDrawSprite(rt, ImageId(SPR_TEMP_TRACK_LIST), trackPreview);

            screenPos.y = windowPos.y + tdWidget.bottom - 12;

            // Warnings
            if (_loadedTrackDesign->gameStateData.hasFlag(TrackDesignGameStateFlag::VehicleUnavailable)
                && gLegacyScene != LegacyScene::trackDesignsManager)
            {
                // Vehicle design not available
                drawTextEllipsised(rt, screenPos, 368, STR_VEHICLE_DESIGN_UNAVAILABLE, { TextAlignment::centre });
                screenPos.y -= kScrollableRowHeight;
            }

            if (_loadedTrackDesign->gameStateData.hasFlag(TrackDesignGameStateFlag::SceneryUnavailable))
            {
                if (!gTrackDesignSceneryToggle)
                {
                    // Scenery not available
                    drawTextEllipsised(
                        rt, screenPos, 368, STR_DESIGN_INCLUDES_SCENERY_WHICH_IS_UNAVAILABLE, { TextAlignment::centre });
                    screenPos.y -= kScrollableRowHeight;
                }
            }

            // Track design name
            auto ft = Formatter();
            ft.Add<const utf8*>(_trackDesigns[trackIndex].name.c_str());
            drawTextEllipsised(rt, screenPos, 368, STR_TRACK_PREVIEW_NAME_FORMAT, ft, { TextAlignment::centre });

            // Information
            screenPos = windowPos + ScreenCoordsXY{ tdWidget.left + 1, tdWidget.bottom + 2 };

            // Stats
            ft = Formatter();
            ft.Add<fixed32_2dp>(_loadedTrackDesign->statistics.ratings.excitement);
            drawText(rt, screenPos, STR_TRACK_LIST_EXCITEMENT_RATING, ft);
            screenPos.y += kListRowHeight;

            ft = Formatter();
            ft.Add<fixed32_2dp>(_loadedTrackDesign->statistics.ratings.intensity);
            drawText(rt, screenPos, STR_TRACK_LIST_INTENSITY_RATING, ft);
            screenPos.y += kListRowHeight;

            ft = Formatter();
            ft.Add<fixed32_2dp>(_loadedTrackDesign->statistics.ratings.nausea);
            drawText(rt, screenPos, STR_TRACK_LIST_NAUSEA_RATING, ft);
            screenPos.y += kListRowHeight + 4;

            // Information for tracked rides.
            if (GetRideTypeDescriptor(_loadedTrackDesign->trackAndVehicle.rtdIndex).flags.has(RtdFlag::hasTrack))
            {
                const auto& rtd = GetRideTypeDescriptor(_loadedTrackDesign->trackAndVehicle.rtdIndex);
                if (rtd.specialType != RtdSpecialType::maze)
                {
                    if (rtd.specialType == RtdSpecialType::miniGolf)
                    {
                        // Holes
                        ft = Formatter();
                        ft.Add<uint16_t>(_loadedTrackDesign->statistics.holes);
                        drawText(rt, screenPos, STR_HOLES, ft);
                        screenPos.y += kListRowHeight;
                    }
                    else
                    {
                        // Maximum speed
                        ft = Formatter();
                        ft.Add<uint16_t>(ToHumanReadableSpeed(_loadedTrackDesign->statistics.maxSpeed << 16));
                        drawText(rt, screenPos, STR_MAX_SPEED, ft);
                        screenPos.y += kListRowHeight;

                        // Average speed
                        ft = Formatter();
                        ft.Add<uint16_t>(ToHumanReadableSpeed(_loadedTrackDesign->statistics.averageSpeed << 16));
                        drawText(rt, screenPos, STR_AVERAGE_SPEED, ft);
                        screenPos.y += kListRowHeight;
                    }

                    // Ride length
                    ft = Formatter();
                    ft.Add<StringId>(STR_RIDE_LENGTH_ENTRY);
                    ft.Add<uint16_t>(_loadedTrackDesign->statistics.rideLength);
                    drawTextEllipsised(rt, screenPos, 214, STR_TRACK_LIST_RIDE_LENGTH, ft);
                    screenPos.y += kListRowHeight;
                }

                if (GetRideTypeDescriptor(_loadedTrackDesign->trackAndVehicle.rtdIndex).flags.has(RtdFlag::hasGForces))
                {
                    // Maximum positive vertical Gs
                    ft = Formatter();
                    ft.Add<int32_t>(_loadedTrackDesign->statistics.maxPositiveVerticalG);
                    drawText(rt, screenPos, STR_MAX_POSITIVE_VERTICAL_G, ft);
                    screenPos.y += kListRowHeight;

                    // Maximum negative vertical Gs
                    ft = Formatter();
                    ft.Add<int32_t>(_loadedTrackDesign->statistics.maxNegativeVerticalG);
                    drawText(rt, screenPos, STR_MAX_NEGATIVE_VERTICAL_G, ft);
                    screenPos.y += kListRowHeight;

                    // Maximum lateral Gs
                    ft = Formatter();
                    ft.Add<int32_t>(_loadedTrackDesign->statistics.maxLateralG);
                    drawText(rt, screenPos, STR_MAX_LATERAL_G, ft);
                    screenPos.y += kListRowHeight;

                    if (_loadedTrackDesign->statistics.totalAirTime != 0)
                    {
                        // Total air time
                        ft = Formatter();
                        ft.Add<int32_t>(ToHumanReadableAirTime(_loadedTrackDesign->statistics.totalAirTime));
                        drawText(rt, screenPos, STR_TOTAL_AIR_TIME, ft);
                        screenPos.y += kListRowHeight;
                    }
                }

                if (GetRideTypeDescriptor(_loadedTrackDesign->trackAndVehicle.rtdIndex).flags.has(RtdFlag::hasDrops))
                {
                    // Drops
                    ft = Formatter();
                    ft.Add<uint16_t>(_loadedTrackDesign->statistics.drops);
                    drawText(rt, screenPos, STR_DROPS, ft);
                    screenPos.y += kListRowHeight;

                    // Drop height is multiplied by 0.75
                    ft = Formatter();
                    ft.Add<uint16_t>((_loadedTrackDesign->statistics.highestDropHeight * 3) / 4);
                    drawText(rt, screenPos, STR_HIGHEST_DROP_HEIGHT, ft);
                    screenPos.y += kListRowHeight;
                }

                if (_loadedTrackDesign->statistics.inversions != 0)
                {
                    ft = Formatter();
                    ft.Add<uint16_t>(_loadedTrackDesign->statistics.inversions);
                    drawText(rt, screenPos, STR_INVERSIONS, ft);
                    screenPos.y += kListRowHeight;
                }

                screenPos.y += 4;
            }

            if (!_loadedTrackDesign->statistics.spaceRequired.IsNull())
            {
                // Space required
                ft = Formatter();
                ft.Add<uint16_t>(_loadedTrackDesign->statistics.spaceRequired.x);
                ft.Add<uint16_t>(_loadedTrackDesign->statistics.spaceRequired.y);
                drawText(rt, screenPos, STR_TRACK_LIST_SPACE_REQUIRED, ft);
                screenPos.y += kListRowHeight;
            }

            if (_loadedTrackDesign->gameStateData.cost != 0)
            {
                ft = Formatter();
                ft.Add<uint32_t>(_loadedTrackDesign->gameStateData.cost);
                drawText(rt, screenPos, STR_TRACK_LIST_COST_AROUND, ft);
            }
        }

        void onScrollDraw(const int32_t scrollIndex, RenderTarget& rt) override
        {
            auto paletteIndex = getColourMap(colours[0].colour).midLight;
            GfxClear(rt, paletteIndex);

            auto screenCoords = ScreenCoordsXY{ 0, 0 };
            size_t listIndex = 0;
            if (gLegacyScene == LegacyScene::trackDesignsManager)
            {
                if (_trackDesigns.empty())
                {
                    // No track designs
                    drawText(rt, screenCoords - ScreenCoordsXY{ 0, 1 }, STR_NO_TRACK_DESIGNS_OF_THIS_TYPE);
                    return;
                }
            }
            else
            {
                // Build custom track item
                StringId stringId;
                if (listIndex == static_cast<size_t>(selectedListItem))
                {
                    // Highlight
                    Rectangle::filter(
                        rt, { screenCoords, { width, screenCoords.y + kScrollableRowHeight - 1 } },
                        FilterPaletteID::paletteDarken1);
                    stringId = STR_WINDOW_COLOUR_2_STRINGID;
                }
                else
                {
                    stringId = STR_BLACK_STRING;
                }

                auto ft = Formatter();
                ft.Add<StringId>(STR_BUILD_CUSTOM_DESIGN);
                drawText(rt, screenCoords - ScreenCoordsXY{ 0, 1 }, stringId, ft);
                screenCoords.y += kScrollableRowHeight;
                listIndex++;
            }

            for (auto i : _filteredTrackIds)
            {
                if (screenCoords.y + kScrollableRowHeight >= rt.y && screenCoords.y < rt.y + rt.height)
                {
                    StringId stringId;
                    if (listIndex == static_cast<size_t>(selectedListItem))
                    {
                        // Highlight
                        Rectangle::filter(
                            rt, { screenCoords, { width, screenCoords.y + kScrollableRowHeight - 1 } },
                            FilterPaletteID::paletteDarken1);
                        stringId = STR_WINDOW_COLOUR_2_STRINGID;
                    }
                    else
                    {
                        stringId = STR_BLACK_STRING;
                    }

                    // Draw track name
                    auto ft = Formatter();
                    ft.Add<StringId>(STR_TRACK_LIST_NAME_FORMAT);
                    ft.Add<const utf8*>(_trackDesigns[i].name.c_str());
                    drawText(rt, screenCoords - ScreenCoordsXY{ 0, 1 }, stringId, ft);
                }

                screenCoords.y += kScrollableRowHeight;
                listIndex++;
            }
        }

        void setIsBeingUpdated(const bool beingUpdated)
        {
            _selectedItemIsBeingUpdated = beingUpdated;
        }

        void reloadTrackDesigns()
        {
            _reloadTrackDesigns = true;
        }
    };

    WindowBase* TrackListOpen(const RideSelection item)
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->CloseConstructionWindows();

        WindowFlags flags = {};
        ScreenCoordsXY screenPos{};
        if (gLegacyScene == LegacyScene::trackDesignsManager)
        {
            flags = { WindowFlag::autoPosition, WindowFlag::centreScreen };
        }
        else
        {
            screenPos = { 0, kTopToolbarHeight + 2 };
        }

        return windowMgr->Create<TrackListWindow>(WindowClass::trackDesignList, screenPos, kWindowSize, flags, item);
    }

    void WindowTrackDesignListReloadTracks()
    {
        auto* windowMgr = GetWindowManager();
        auto* trackListWindow = static_cast<TrackListWindow*>(windowMgr->FindByClass(WindowClass::trackDesignList));
        if (trackListWindow != nullptr)
        {
            trackListWindow->reloadTrackDesigns();
        }
    }

    void WindowTrackDesignListSetBeingUpdated(const bool beingUpdated)
    {
        auto* windowMgr = GetWindowManager();
        auto* trackListWindow = static_cast<TrackListWindow*>(windowMgr->FindByClass(WindowClass::trackDesignList));
        if (trackListWindow != nullptr)
        {
            trackListWindow->setIsBeingUpdated(beingUpdated);
        }
    }

    // Register the track-design list with the graph accessibility navigator (called once at startup
    // via EnsureGraphScreensRegistered). From here on the graph owns this window class; the legacy
    // accessibility dispatcher stands down for it.
    void RegisterTrackListGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::trackDesignList;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<TrackListWindow&>(w).BuildAccessGraph(b); };
        // Escape goes back to the ride catalog this list was opened from (legacy parity). In the
        // track manager the window reopens the manager itself from onClose.
        screen.onEscape = [](WindowBase&) {
            if (gLegacyScene != LegacyScene::trackDesignsManager)
                ContextOpenWindow(WindowClass::constructRide);
            return false; // let the navigator close this window
        };
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
