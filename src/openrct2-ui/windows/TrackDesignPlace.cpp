/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/UiContext.h>
#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/input/InputManager.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/ViewportInteraction.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Cheats.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/terraform/ClearAction.h>
#include <openrct2/actions/track/TrackDesignAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/paint/VirtualFloor.h>
#include <openrct2/ride/RideConstruction.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/Track.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/TrackDesign.h>
#include <openrct2/ride/TrackDesignRepository.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/tile_element/Slope.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using namespace OpenRCT2::Numerics;
using namespace OpenRCT2::TrackMetadata;
using OpenRCT2::Drawing::PaletteIndex;
using OpenRCT2::GameActions::CommandFlag;
using OpenRCT2::GameActions::CommandFlags;

namespace OpenRCT2::Ui::Windows
{
    static constexpr StringId kWindowTitle = kStringIdNone;
    static constexpr ScreenSize kWindowSize = { 200, 124 };
    static constexpr ScreenSize kTrackMiniPreviewSize = { 168, 78 };

    static constexpr auto kPaletteIndexColourEntrance = PaletteIndex::pi20;         // White
    static constexpr auto kPaletteIndexColourExit = PaletteIndex::pi10;             // Black
    static constexpr auto kPaletteIndexColourTrack = PaletteIndex::primaryRemap5;   // Grey (dark)
    static constexpr auto kPaletteIndexColourStation = PaletteIndex::primaryRemap9; // Grey (light)

    enum WindowTrackDesignPlaceWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_ROTATE,
        WIDX_MIRROR,
        WIDX_SELECT_DIFFERENT_DESIGN,
        WIDX_PRICE,
        WIDX_PREVIEW,
    };

    VALIDATE_GLOBAL_WIDX(WC_TRACK_DESIGN_PLACE, WIDX_ROTATE);

    // clang-format off
    static constexpr auto _trackPlaceWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({173,  83}, { 24, 24},             WidgetType::flatBtn, WindowColour::primary, ImageId(SPR_ROTATE_ARROW),     STR_ROTATE_90_TIP                         ),
        makeWidget({173,  59}, { 24, 24},             WidgetType::flatBtn, WindowColour::primary, ImageId(SPR_MIRROR_ARROW),     STR_MIRROR_IMAGE_TIP                      ),
        makeWidget({  4, 109}, {192, 12},             WidgetType::button,  WindowColour::primary, STR_SELECT_A_DIFFERENT_DESIGN, STR_GO_BACK_TO_DESIGN_SELECTION_WINDOW_TIP),
        makeWidget({ 88,  93}, {  1,  1},             WidgetType::empty,   WindowColour::primary),
        makeWidget({  4,  17}, kTrackMiniPreviewSize, WidgetType::empty,   WindowColour::primary)
    );
    // clang-format on

    static bool _placingTrackDesign = false;

    class TrackDesignPlaceWindow final : public Window
    {
    private:
        std::unique_ptr<TrackDesign> _trackDesign;

        CoordsXYZD _placementLoc;
        RideId _placementGhostRideId;
        bool _hasPlacementGhost;
        money64 _placementCost;
        CoordsXYZD _placementGhostLoc;

        std::vector<PaletteIndex> _miniPreview;

        bool _trackPlaceCtrlState = false;
        int32_t _trackPlaceCtrlZ;

        bool _trackPlaceShiftState = false;
        ScreenCoordsXY _trackPlaceShiftStart;
        int32_t _trackPlaceShiftZ;

        int32_t _trackPlaceZ;
        bool _triggeredUndergroundView = false;

        // Two-stage keyboard placement (accessibility). The first Enter does not build: it freezes the
        // design's footprint at the cursor (_accPreviewing = true) so a blind player can arrow around
        // and hear where every tile of the ride would land before committing. A second Enter builds at
        // that frozen origin; Backspace picks it back up to reposition. Mirrors the shop/flat-ride
        // keyboard placement flow so pre-built coasters can be checked before they are built.
        bool _accPreviewing = false;
        CoordsXY _accPreviewOrigin{};
        std::string _accPreviewName;

    public:
        void onOpen() override
        {
            setWidgets(_trackPlaceWidgets);
            WindowInitScrollWidgets(*this);
            ToolSet(*this, WIDX_PRICE, Tool::crosshair);
            gInputFlags.set(InputFlag::allowRightMouseRemoval);
            WindowPushOthersRight(*this);
            ShowGridlines();
            _miniPreview.resize(kTrackMiniPreviewSize.width * kTrackMiniPreviewSize.height);
            _placementCost = kMoney64Undefined;
            _placementLoc.SetNull();
            _currentTrackPieceDirection = (2 - GetCurrentRotation()) & 3;
        }

        void onClose() override
        {
            _accPreviewing = false;
            clearProvisional();
            ViewportSetVisibility(ViewportVisibility::standard);
            gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
            gMapSelectFlags.unset(MapSelectFlag::enableArrow);
            HideGridlines();
            _miniPreview.clear();
            _miniPreview.shrink_to_fit();
            _trackDesign = nullptr;
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_ROTATE:
                    clearProvisional();
                    _currentTrackPieceDirection = (_currentTrackPieceDirection + 1) & 3;
                    invalidate();
                    _placementLoc.SetNull();
                    DrawMiniPreview(*_trackDesign);
                    break;
                case WIDX_MIRROR:
                    TrackDesignMirror(*_trackDesign);
                    _currentTrackPieceDirection = (0 - _currentTrackPieceDirection) & 3;
                    invalidate();
                    _placementLoc.SetNull();
                    DrawMiniPreview(*_trackDesign);
                    break;
                case WIDX_SELECT_DIFFERENT_DESIGN:
                    close();

                    auto intent = Intent(WindowClass::trackDesignList);
                    intent.PutExtra(INTENT_EXTRA_RIDE_TYPE, _window_track_list_item.Type);
                    intent.PutExtra(INTENT_EXTRA_RIDE_ENTRY_INDEX, _window_track_list_item.EntryIndex);
                    ContextOpenIntent(&intent);
                    break;
            }
        }

        void onUpdate() override
        {
            if (!isToolActive(WindowClass::trackDesignPlace))
                close();
        }

        void onToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            // In the accessibility fork the keyboard map cursor drives placement, not the mouse. While
            // it is active, position the ghost at the cursor's tile and ignore the mouse position - the
            // game calls this every frame from the pointer, and when the pointer sits over the window or
            // off the map that would otherwise clear the ghost each frame, so the keyboard-driven outline
            // never appears.
            if (Accessibility::IsMapCursorActive())
            {
                if (auto cur = Accessibility::GetMapCursorScreenPos(); cur.has_value())
                {
                    const auto cursorTile = ViewportInteractionGetTileStartAtCursor(*cur);
                    if (!cursorTile.IsNull())
                        accUpdateGhost(cursorTile);
                }
                return;
            }

            TrackDesignState tds{};

            gMapSelectFlags.unset(MapSelectFlag::enable);
            gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
            gMapSelectFlags.unset(MapSelectFlag::enableArrow);

            if (_placingTrackDesign)
            {
                return;
            }

            // Take shift modifier into account
            ScreenCoordsXY targetScreenCoords = screenCoords;
            if (_trackPlaceShiftState)
                targetScreenCoords = _trackPlaceShiftStart;

            // Get the tool map position
            CoordsXY mapCoords = ViewportInteractionGetTileStartAtCursor(targetScreenCoords);
            if (mapCoords.IsNull())
            {
                clearProvisional();
                return;
            }

            // Get base Z position
            // NB: always use the actual screenCoords here, not the shifted ones
            auto maybeMapZ = getBaseZ(mapCoords, screenCoords);
            if (!maybeMapZ.has_value())
            {
                clearProvisional();
                return;
            }

            CoordsXYZD trackLoc = { mapCoords, *maybeMapZ, _currentTrackPieceDirection };

            // Check if tool map position has changed since last update
            if (trackLoc == _placementLoc)
            {
                TrackDesignPreviewDrawOutlines(
                    tds, *_trackDesign, RideGetTemporaryForPreview(), { mapCoords, 0, _currentTrackPieceDirection },
                    !gTrackDesignSceneryToggle);
                return;
            }

            money64 cost = kMoney64Undefined;
            if (GameIsNotPaused() || getGameState().cheats.buildInPauseMode)
            {
                clearProvisional();
                CoordsXYZD ghostTrackLoc = trackLoc;
                auto res = findValidTrackDesignPlaceHeight(ghostTrackLoc, { CommandFlag::noSpend, CommandFlag::ghost });

                if (res.error == GameActions::Status::ok)
                {
                    // Valid location found. Place the ghost at the location.
                    auto tdAction = GameActions::TrackDesignAction(
                        ghostTrackLoc, *_trackDesign, !gTrackDesignSceneryToggle,
                        Config::Get().general.defaultInspectionInterval);
                    tdAction.SetFlags({ CommandFlag::noSpend, CommandFlag::ghost });
                    tdAction.SetCallback([&](const GameActions::GameAction*, const GameActions::Result* result) {
                        if (result->error == GameActions::Status::ok)
                        {
                            _placementGhostRideId = result->getData<RideId>();
                            _placementGhostLoc = ghostTrackLoc;
                            _hasPlacementGhost = true;
                        }
                    });
                    res = GameActions::Execute(&tdAction, getGameState());
                    cost = res.error == GameActions::Status::ok ? res.cost : kMoney64Undefined;

                    VirtualFloorSetHeight(ghostTrackLoc.z);
                }
            }

            _placementLoc = trackLoc;
            if (cost != _placementCost)
            {
                _placementCost = cost;
                invalidateWidget(WIDX_PRICE);
            }

            TrackDesignPreviewDrawOutlines(
                tds, *_trackDesign, RideGetTemporaryForPreview(), trackLoc, !gTrackDesignSceneryToggle);
        }

        void onToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            clearProvisional();
            gMapSelectFlags.unset(MapSelectFlag::enable);
            gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
            gMapSelectFlags.unset(MapSelectFlag::enableArrow);

            // Take shift modifier into account
            ScreenCoordsXY targetScreenCoords = screenCoords;
            if (_trackPlaceShiftState)
                targetScreenCoords = _trackPlaceShiftStart;

            // Get the tool map position
            CoordsXY mapCoords = ViewportInteractionGetTileStartAtCursor(targetScreenCoords);
            if (mapCoords.IsNull())
            {
                clearProvisional();
                return;
            }

            // NB: always use the actual screenCoords here, not the shifted ones
            auto maybeMapZ = getBaseZ(mapCoords, screenCoords);
            if (!maybeMapZ.has_value())
            {
                clearProvisional();
                return;
            }

            // Try increasing Z until a feasible placement is found
            CoordsXYZ trackLoc = { mapCoords, maybeMapZ.value() };
            auto res = findValidTrackDesignPlaceHeight(trackLoc, {});
            if (res.error != GameActions::Status::ok)
            {
                // Unable to build track
                Audio::Play3D(Audio::SoundId::error, trackLoc);

                auto windowManager = GetWindowManager();
                windowManager->ShowError(res.getErrorTitle(), res.getErrorMessage());
                return;
            }

            _placingTrackDesign = true;

            auto tdAction = GameActions::TrackDesignAction(
                { trackLoc, _currentTrackPieceDirection }, *_trackDesign, !gTrackDesignSceneryToggle,
                Config::Get().general.defaultInspectionInterval);
            tdAction.SetCallback([&, trackLoc](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error != GameActions::Status::ok)
                {
                    Audio::Play3D(Audio::SoundId::error, result->position);
                    auto* windowMgr = GetWindowManager();
                    windowMgr->ShowError(result->getErrorTitle(), result->getErrorMessage());
                    announcePlacementFailure(result->error, CoordsXY{ trackLoc }, trackLoc.z);
                    _placingTrackDesign = false;
                    return;
                }

                rideId = result->getData<RideId>();
                auto getRide = GetRide(rideId);
                if (getRide != nullptr)
                {
                    auto* windowMgr = GetWindowManager();
                    windowMgr->CloseByClass(WindowClass::error);

                    Audio::Play3D(Audio::SoundId::placeItem, trackLoc);
                    _currentRideIndex = rideId;

                    if (TrackDesignAreEntranceAndExitPlaced())
                    {
                        auto intent = Intent(WindowClass::ride);
                        intent.PutExtra(INTENT_EXTRA_RIDE_ID, rideId.ToUnderlying());
                        ContextOpenIntent(&intent);
                        auto* wnd = windowMgr->FindByClass(WindowClass::trackDesignPlace);
                        windowMgr->Close(*wnd);
                    }
                    else
                    {
                        RideInitialiseConstructionWindow(*getRide);
                        auto* wnd = windowMgr->FindByClass(WindowClass::rideConstruction);
                        wnd->onMouseUp(WC_RIDE_CONSTRUCTION__WIDX_ENTRANCE);
                    }
                }
                _placingTrackDesign = false;
            });
            GameActions::Execute(&tdAction, getGameState());
        }

        void onToolAbort(WidgetIndex widgetIndex) override
        {
            clearProvisional();
        }

        void onViewportRotate() override
        {
            DrawMiniPreview(*_trackDesign);
        }

        void onPrepareDraw() override
        {
            DrawMiniPreview(*_trackDesign);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            if (_trackDesign != nullptr)
                widgets[WIDX_TITLE].setString(_trackDesign->gameStateData.name.c_str());
            else
                widgets[WIDX_TITLE].setString("");

            WindowDrawWidgets(*this, rt);

            // Draw mini tile preview
            Drawing::RenderTarget clippedRT;
            const auto& previewWidget = widgets[WIDX_PREVIEW];
            const auto previewCoords = windowPos + ScreenCoordsXY{ previewWidget.left, previewWidget.top };
            if (ClipRenderTarget(clippedRT, rt, previewCoords, previewWidget.width(), previewWidget.height()))
            {
                G1Element g1temp = {};
                g1temp.offset = reinterpret_cast<uint8_t*>(_miniPreview.data());
                g1temp.width = kTrackMiniPreviewSize.width;
                g1temp.height = kTrackMiniPreviewSize.height;
                GfxSetG1Element(SPR_TEMP_TRACK_PLACE, &g1temp);
                DrawingEngineInvalidateImage(SPR_TEMP_TRACK_PLACE);
                GfxDrawSprite(clippedRT, ImageId(SPR_TEMP_TRACK_PLACE, this->colours[0].colour), { 0, 0 });
            }

            // Price
            if (_placementCost != kMoney64Undefined && !(getGameState().park.flags & PARK_FLAGS_NO_MONEY))
            {
                auto ft = Formatter();
                ft.Add<money64>(_placementCost);
                const auto& priceWidget = widgets[WIDX_PRICE];
                const auto priceCoords = windowPos + ScreenCoordsXY{ priceWidget.left, priceWidget.top };
                drawText(rt, priceCoords, STR_COST_LABEL, ft, { TextAlignment::centre });
            }
        }

        void ClearProvisionalTemporarily()
        {
            if (_hasPlacementGhost)
            {
                auto provRide = GetRide(_placementGhostRideId);
                if (provRide != nullptr)
                {
                    TrackDesignPreviewRemoveGhosts(*_trackDesign, *provRide, _placementGhostLoc);
                }
            }
        }

        void RestoreProvisional()
        {
            if (_hasPlacementGhost)
            {
                auto tdAction = GameActions::TrackDesignAction(
                    { _placementGhostLoc }, *_trackDesign, !gTrackDesignSceneryToggle,
                    Config::Get().general.defaultInspectionInterval);
                tdAction.SetFlags({ CommandFlag::noSpend, CommandFlag::ghost });
                auto res = GameActions::Execute(&tdAction, getGameState());
                if (res.error != GameActions::Status::ok)
                {
                    _hasPlacementGhost = false;
                }
            }
        }

        void init(std::unique_ptr<TrackDesign>&& trackDesign)
        {
            _trackDesign = std::move(trackDesign);

            // Tell a screen-reader user the placement controls (the keyboard map cursor
            // positions the ride; these keys rotate, build, and cancel).
            Accessibility::ScreenReaderSpeak(
                "Placing " + std::string(_trackDesign->gameStateData.name)
                + ". Move the cursor to position the ride, R to rotate, Enter to place a preview, then Enter "
                  "again to build. Backspace repositions, Escape cancels.");
        }

        // ---- Accessibility: driven from the keyboard map cursor via the free functions below.

        void rotateForAccessibility()
        {
            // Rotating changes the footprint, so it would invalidate a frozen preview; ask the player
            // to pick it back up first (matching the shop/flat-ride placement flow).
            if (_accPreviewing)
            {
                Accessibility::ScreenReaderSpeak("Press Backspace to reposition before rotating");
                return;
            }
            clearProvisional();
            _currentTrackPieceDirection = (_currentTrackPieceDirection + 1) & 3;
            invalidate();
            _placementLoc.SetNull();
            DrawMiniPreview(*_trackDesign);

            static constexpr const char* kDirections[] = { "North", "East", "South", "West" };
            Accessibility::ScreenReaderSpeak(
                std::string("Rotated, facing ") + kDirections[(_currentTrackPieceDirection + GetCurrentRotation()) & 3]);
        }

        // Plain-language "why and how to fix" for a placement failure, from the engine's status
        // code. Pre-built rides usually fail because the ground is not flat and clear across the
        // whole footprint, so the generic case spells that out.
        static const char* PlacementAdvice(GameActions::Status status)
        {
            switch (status)
            {
                case GameActions::Status::notOwned:
                    return "Part of the ride would sit on land you do not own. Buy the land first, or "
                           "move the ride fully inside your park.";
                case GameActions::Status::tooHigh:
                    return "The ride will not fit this high. Try lower ground, or flatten and lower the "
                           "land here.";
                case GameActions::Status::tooLow:
                    return "The ride sits too low here. Try higher ground, or raise the land.";
                case GameActions::Status::noClearance:
                    return "Something is in the way over the ride's footprint. Clear the trees, scenery "
                           "or other rides around and above it.";
                case GameActions::Status::itemAlreadyPlaced:
                    return "Something is already built on the ride's footprint. Remove or move it first.";
                case GameActions::Status::noFreeElements:
                    return "This part of the map is too crowded to build more. Remove some scenery or "
                           "rides nearby.";
                case GameActions::Status::insufficientFunds:
                    return "You cannot afford to build this ride right now.";
                default:
                    return "The ride cannot be built here. A pre-built ride needs a large, flat, empty "
                           "area: clear any scenery and level the ground across the whole footprint, or "
                           "rotate it with R, then try again.";
            }
        }

        // Walks the track design's footprint (mirroring TrackDesignPlaceVirtual) from the given
        // placement origin and the current rotation, returning each world-coordinate tile the track
        // would occupy. Deduplicated.
        std::vector<CoordsXY> footprintTiles(const CoordsXY& origin)
        {
            std::vector<CoordsXY> tiles;
            if (_trackDesign == nullptr)
                return tiles;

            std::vector<int32_t> seen; // packed tile keys, to skip tiles the track revisits
            CoordsXYZ newCoords{ origin.x, origin.y, 0 };
            uint8_t rotation = _currentTrackPieceDirection;

            for (const auto& track : _trackDesign->trackElements)
            {
                const auto& ted = GetTrackElementDescriptor(track.type);
                for (uint8_t i = 0; i < ted.sequenceData.numSequences; i++)
                {
                    const auto& clearance = ted.sequenceData.sequences[i].clearance;
                    const CoordsXY tile = CoordsXY{ newCoords } + CoordsXY{ clearance.x, clearance.y }.Rotate(rotation);
                    if (!MapIsLocationValid(tile))
                        continue;

                    const TileCoordsXY tc{ tile };
                    const int32_t key = (tc.x << 16) | (tc.y & 0xFFFF);
                    if (std::find(seen.begin(), seen.end(), key) != seen.end())
                        continue;
                    seen.push_back(key);
                    tiles.push_back(tile);
                }

                const auto& coords = ted.coordinates;
                const CoordsXY offset = CoordsXY{ newCoords } + CoordsXY{ coords.x, coords.y }.Rotate(rotation);
                newCoords = { offset, newCoords.z - coords.zBegin + coords.zEnd };
                rotation = (rotation + coords.rotationEnd - coords.rotationBegin) & 3;
                if (coords.rotationEnd & (1 << 2))
                    rotation |= (1 << 2);
                else
                    newCoords += CoordsDirectionDelta[rotation];
            }
            return tiles;
        }

        // World-coordinate tile of each entrance and exit the design will build, paired with whether
        // it is the exit. A path or other blocker on one of these fails placement just as one on the
        // track does, but they sit beyond the track footprint, so they must be checked separately.
        //
        // Follows TrackDesignPlaceEntrances exactly: the entrance goes at
        // rotatedEntranceMapPos + origin, which is what RideEntranceExitPlaceAction is given. The
        // CoordsDirectionDelta step onward from there is the STATION's track tile, which the engine
        // only uses to find the track element and read its station index - stepping to it here would
        // report a tile inside the design's own footprint instead of the entrance.
        std::vector<std::pair<CoordsXY, bool>> entranceExitTiles(const CoordsXY& origin)
        {
            std::vector<std::pair<CoordsXY, bool>> tiles;
            if (_trackDesign == nullptr)
                return tiles;

            const uint8_t baseRotation = _currentTrackPieceDirection & 3;
            for (const auto& entrance : _trackDesign->entranceElements)
            {
                const CoordsXY tile = entrance.location.ToCoordsXY().Rotate(baseRotation) + origin;
                if (MapIsLocationValid(tile))
                    tiles.emplace_back(tile, entrance.isExit);
            }
            return tiles;
        }

        // Every tile the placement validates: the track footprint plus the entrance/exit tiles.
        // Both the hard-blocker check and the obstruction report use this so a path on an exit tile
        // (which is not part of the track footprint) is correctly detected, rather than silently
        // clearing scenery for a placement that a path was always going to block.
        std::vector<CoordsXY> validatedTiles(const CoordsXY& origin)
        {
            auto tiles = footprintTiles(origin);
            for (const auto& [tile, isExit] : entranceExitTiles(origin))
            {
                const TileCoordsXY tc{ tile };
                const bool alreadyListed = std::any_of(tiles.begin(), tiles.end(), [&](const CoordsXY& t) {
                    const TileCoordsXY existing{ t };
                    return existing.x == tc.x && existing.y == tc.y;
                });
                if (!alreadyListed)
                    tiles.push_back(tile);
            }
            return tiles;
        }

        // Names only the un-clearable blockers on a tile - a path, another ride, an entrance/exit or
        // a banner. Scenery and walls/fences are deliberately never named: they are auto-cleared
        // before building, so they are not what is stopping placement. Returns empty if the tile has
        // nothing un-clearable on it.
        std::string describeTileBlockers(const TileCoordsXY& tc)
        {
            std::vector<std::string> parts;
            bool namedRide = false;
            for (TileElement* el = MapGetFirstElementAt(tc); el != nullptr;)
            {
                if (!el->isGhost())
                {
                    switch (el->getType())
                    {
                        case TileElementType::Path:
                            parts.push_back("Path");
                            break;
                        case TileElementType::Track:
                            if (!namedRide) // one mention per tile, however many pieces sit on it
                            {
                                namedRide = true;
                                parts.push_back("Another ride");
                            }
                            break;
                        case TileElementType::Entrance:
                            parts.push_back("Ride entrance or exit");
                            break;
                        case TileElementType::Banner:
                            parts.push_back("Banner");
                            break;
                        default: // Surface, scenery and walls/fences are clearable - not reported.
                            break;
                    }
                }
                if (el->isLastForTile())
                    break;
                el++;
            }

            std::string out;
            for (size_t i = 0; i < parts.size(); i++)
                out += (i == 0 ? "" : ", ") + parts[i];
            return out;
        }

        // "X n, Y n: what's there" for each footprint tile that has an un-clearable blocker on it.
        // Tiles whose only contents are scenery/walls are omitted, since those are auto-cleared and
        // are not the reason a placement failed.
        std::vector<std::string> findFootprintObstructions(const CoordsXY& origin)
        {
            std::vector<std::string> blockers;
            for (const auto& tile : validatedTiles(origin))
            {
                const TileCoordsXY tc{ tile };
                const std::string desc = describeTileBlockers(tc);
                if (!desc.empty())
                    blockers.push_back(Accessibility::SpokenTileCoordsText(tc) + ": " + desc);
            }
            return blockers;
        }

        // True if any footprint tile holds a non-scenery blocker (a path, another ride, an
        // entrance/exit, or a banner) - i.e. something clearing scenery would NOT resolve. Walls and
        // fences are clearable so they don't count. Used to avoid destroying scenery when the ride
        // still couldn't be built anyway.
        bool footprintHasHardBlocker(const CoordsXY& origin)
        {
            for (const auto& tile : validatedTiles(origin))
            {
                if (Accessibility::TileHasNonSceneryBlocker(TileCoordsXY{ tile }))
                    return true;
            }
            return false;
        }

        // "X n, Y n: why" for each footprint tile whose bare ground cannot host the ride no matter
        // what scenery is removed - sloped or missing ground, or ground rising higher than the
        // placement can be raised to bridge. Clearing scenery will not fix these (the land needs
        // levelling), so they are reported as the reason and never cleared.
        std::vector<std::string> findUnlevelTiles(const CoordsXY& origin, int32_t cursorBaseZ)
        {
            std::vector<std::string> out;
            for (const auto& tile : footprintTiles(origin))
            {
                const TileCoordsXY tc{ tile };
                auto* surface = MapGetSurfaceElementAt(tile);
                const char* why = nullptr;
                if (surface == nullptr)
                {
                    why = "off the edge of the map";
                }
                else if (surface->GetSlope() != 0)
                {
                    why = "sloped ground";
                }
                else
                {
                    int32_t top = floor2(surface->getBaseZ(), kCoordsZStep);
                    if (surface->GetWaterHeight() > 0)
                        top = std::max<int32_t>(top, surface->GetWaterHeight());
                    // findValidTrackDesignPlaceHeight only probes 7 steps up from the cursor's ground
                    // height; ground higher than that range cannot be bridged by raising the ride.
                    if ((top - cursorBaseZ) > 6 * static_cast<int32_t>(kCoordsZStep))
                        why = "ground too high";
                }
                if (why != nullptr)
                    out.push_back(Accessibility::SpokenTileCoordsText(tc) + ": " + why);
            }
            return out;
        }

        // True if the ground under the footprint cannot host the ride regardless of scenery, so
        // clearing scenery would be pointless. See findUnlevelTiles for what counts.
        bool footprintTerrainBlocks(const CoordsXY& origin, int32_t cursorBaseZ)
        {
            return !findUnlevelTiles(origin, cursorBaseZ).empty();
        }

        // True if any tile the placement validates is on land the player cannot build on (not owned
        // and without construction rights, outside sandbox mode). The ride could never be built
        // there, so scenery must not be cleared for it.
        bool footprintOnUnbuildableLand(const CoordsXY& origin)
        {
            if (getGameState().cheats.sandboxMode)
                return false;
            for (const auto& tile : validatedTiles(origin))
            {
                if (!MapIsLocationOwnedOrHasRights(tile))
                    return true;
            }
            return false;
        }

        // Removes small scenery, large scenery and walls/fences from exactly the tiles the placement
        // validates (the track footprint plus the entrance/exit tiles). Clearing the same tiles the
        // build is checked against means that, once hard blockers, terrain and ownership have been
        // ruled out, nothing clearable is left to stop it. Track elements are never touched.
        void clearFootprintScenery(const CoordsXY& origin)
        {
            const GameActions::ClearableItems items = GameActions::CLEARABLE_ITEMS::kScenerySmall
                | GameActions::CLEARABLE_ITEMS::kSceneryLarge;
            for (const auto& tile : validatedTiles(origin))
            {
                auto clear = GameActions::ClearAction(MapRange(tile, tile), items);
                GameActions::Execute(&clear, getGameState());
            }
        }

        // After the engine's error window has spoken the reason, queue plain guidance on how to fix
        // it. For "something in the way" or "level the ground" failures, also name the exact footprint
        // tiles at fault - hard obstructions (paths, rides) and un-level ground - so the player knows
        // precisely what to clear or flatten and where.
        void announcePlacementFailure(GameActions::Status status, const CoordsXY& origin, int32_t baseZ)
        {
            std::string spoken = PlacementAdvice(status);

            // Name the exact tiles at fault - hard obstructions (paths, rides) and un-level ground.
            // Both lists are empty when nothing on a tile is wrong, so this is safe for any failure.
            auto blockers = findFootprintObstructions(origin);
            for (auto& tile : findUnlevelTiles(origin, baseZ))
                blockers.push_back(std::move(tile));

            if (!blockers.empty())
            {
                spoken += " Problem tiles: ";
                const size_t limit = std::min<size_t>(blockers.size(), 4);
                for (size_t i = 0; i < limit; i++)
                    spoken += (i == 0 ? "" : "; ") + blockers[i];
                if (blockers.size() > limit)
                    spoken += "; and " + std::to_string(blockers.size() - limit) + " more";
                spoken += ".";
            }

            // interrupt = false so this follows the error message rather than cutting it off.
            Accessibility::ScreenReaderSpeak(spoken, false);
        }

        // Plays the error sound and reports why a placement failed. Queries at the ground height the
        // player actually aimed at (the upward height search otherwise reports a misleading "too
        // high" from the top of its range) so the spoken reason and advice match where they tried.
        void reportPlacementFailure(const CoordsXY& mapCoords, int32_t baseZ)
        {
            CoordsXYZD groundLoc{ mapCoords.x, mapCoords.y, baseZ, _currentTrackPieceDirection };
            auto groundAction = GameActions::TrackDesignAction(
                groundLoc, *_trackDesign, !gTrackDesignSceneryToggle, Config::Get().general.defaultInspectionInterval);
            auto diag = GameActions::Query(&groundAction, getGameState());

            Audio::Play3D(Audio::SoundId::error, { mapCoords, baseZ });
            GetWindowManager()->ShowError(diag.getErrorTitle(), diag.getErrorMessage());
            announcePlacementFailure(diag.error, mapCoords, baseZ);
        }

        // Every world-coordinate tile the design's footprint (plus entrances/exits) would occupy at
        // the given origin and current rotation, as tile coordinates. Used to read out the frozen
        // preview so the player can trace the ride's shape by arrowing over it.
        std::vector<TileCoordsXY> previewTiles(const CoordsXY& origin)
        {
            std::vector<TileCoordsXY> tiles;
            for (const auto& t : validatedTiles(origin))
                tiles.push_back(TileCoordsXY{ t });
            return tiles;
        }

        // Drives the game's own placement ghost (the visual outline plus the provisional ride) from
        // the keyboard cursor's tile, so a blind player positioning by keyboard sees exactly the ghost
        // the mouse would show - and it sits precisely where buildAtTile will place the ride, since
        // both use the same base-height formula (getBaseZ, i.e. surface + water + TrackDesignGetZPlacement).
        // Mirrors onToolUpdate but takes a map tile directly instead of a screen position. No-op while a
        // preview is frozen (the ghost must stay put so the footprint can be inspected).
        void accUpdateGhost(const CoordsXY& mapCoords)
        {
            if (_trackDesign == nullptr || _placingTrackDesign || _accPreviewing)
                return;

            gMapSelectFlags.unset(MapSelectFlag::enable);
            gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
            gMapSelectFlags.unset(MapSelectFlag::enableArrow);

            auto* surface = MapGetSurfaceElementAt(mapCoords);
            if (surface == nullptr)
            {
                clearProvisional();
                return;
            }

            int32_t baseZ = floor2(surface->getBaseZ(), kCoordsZStep);
            if (surface->GetWaterHeight() > 0)
                baseZ = std::max<int32_t>(baseZ, surface->GetWaterHeight());
            baseZ += TrackDesignGetZPlacement(
                *_trackDesign, RideGetTemporaryForPreview(), { mapCoords, baseZ, _currentTrackPieceDirection });

            CoordsXYZD trackLoc = { mapCoords, baseZ, _currentTrackPieceDirection };

            TrackDesignState tds{};
            if (trackLoc == _placementLoc)
            {
                TrackDesignPreviewDrawOutlines(
                    tds, *_trackDesign, RideGetTemporaryForPreview(), { mapCoords, 0, _currentTrackPieceDirection },
                    !gTrackDesignSceneryToggle);
                return;
            }

            if (GameIsNotPaused() || getGameState().cheats.buildInPauseMode)
            {
                clearProvisional();
                CoordsXYZD ghostTrackLoc = trackLoc;
                auto res = findValidTrackDesignPlaceHeight(ghostTrackLoc, { CommandFlag::noSpend, CommandFlag::ghost });
                if (res.error == GameActions::Status::ok)
                {
                    auto tdAction = GameActions::TrackDesignAction(
                        ghostTrackLoc, *_trackDesign, !gTrackDesignSceneryToggle,
                        Config::Get().general.defaultInspectionInterval);
                    tdAction.SetFlags({ CommandFlag::noSpend, CommandFlag::ghost });
                    tdAction.SetCallback([&](const GameActions::GameAction*, const GameActions::Result* result) {
                        if (result->error == GameActions::Status::ok)
                        {
                            _placementGhostRideId = result->getData<RideId>();
                            _placementGhostLoc = ghostTrackLoc;
                            _hasPlacementGhost = true;
                        }
                    });
                    GameActions::Execute(&tdAction, getGameState());
                    VirtualFloorSetHeight(ghostTrackLoc.z);
                }
            }

            _placementLoc = trackLoc;
            TrackDesignPreviewDrawOutlines(
                tds, *_trackDesign, RideGetTemporaryForPreview(), trackLoc, !gTrackDesignSceneryToggle);
        }

        // First Enter: freeze the design's footprint at the cursor without building, so the player can
        // arrow around to inspect where it will sit. Announces the footprint size and the controls.
        void freezePreview(const CoordsXY& mapCoords)
        {
            _accPreviewOrigin = mapCoords;
            _accPreviewName = std::string(_trackDesign->gameStateData.name);
            _accPreviewing = true;

            // Footprint extent in tiles, for the size announcement.
            const auto tiles = previewTiles(mapCoords);
            int32_t w = 1, h = 1;
            if (!tiles.empty())
            {
                int32_t minX = tiles.front().x, maxX = tiles.front().x;
                int32_t minY = tiles.front().y, maxY = tiles.front().y;
                for (const auto& t : tiles)
                {
                    minX = std::min(minX, t.x);
                    maxX = std::max(maxX, t.x);
                    minY = std::min(minY, t.y);
                    maxY = std::max(maxY, t.y);
                }
                w = maxX - minX + 1;
                h = maxY - minY + 1;
            }
            Accessibility::ScreenReaderSpeak(
                "Ride positioned, " + std::to_string(w) + " by " + std::to_string(h)
                + " tiles. Arrow around to check the area, Enter to build, Backspace to reposition.");
        }

        // Backspace during a preview: pick the design back up so it can be repositioned.
        void pickupForAccessibility()
        {
            if (!_accPreviewing)
                return;
            _accPreviewing = false;
            Accessibility::ScreenReaderSpeak("Picked back up. Move the cursor and press Enter to position it again.");
        }

        // The world tiles where a frozen preview's entrances and exits will be built, each with the
        // word to speak, so the map cursor can jump between them before the ride is built (the ride
        // does not exist yet, so there are no stations to read them off). Only while the preview is
        // frozen: while the design still follows the cursor, moving to an entrance tile would drag
        // the whole ride along with it.
        std::vector<std::pair<CoordsXY, std::string>> previewEntranceExitTiles()
        {
            std::vector<std::pair<CoordsXY, std::string>> out;
            if (!_accPreviewing)
                return out;

            for (const auto& [tile, isExit] : entranceExitTiles(_accPreviewOrigin))
                out.emplace_back(tile, isExit ? "Ride exit" : "Ride entrance");
            return out;
        }

        // If a preview is frozen and covers the given tile, returns the design's name so the tile
        // reader can announce the ride as though it were already there. Otherwise nullopt.
        std::optional<std::string> previewLabelForTile(const TileCoordsXY& tile)
        {
            if (!_accPreviewing)
                return std::nullopt;
            for (const auto& t : previewTiles(_accPreviewOrigin))
                if (t.x == tile.x && t.y == tile.y)
                    return _accPreviewName;
            return std::nullopt;
        }

        // Handles Enter during keyboard placement: the first freezes a preview, the second builds it
        // at the frozen origin (searching upward for a valid height, clearing scenery as needed).
        void placeAtTile(const CoordsXY& mapCoords)
        {
            if (_trackDesign == nullptr)
                return;
            if (!_accPreviewing)
            {
                freezePreview(mapCoords);
                return;
            }
            buildAtTile(_accPreviewOrigin);
        }

        void buildAtTile(const CoordsXY& mapCoords)
        {
            if (_trackDesign == nullptr)
                return;

            auto* surface = MapGetSurfaceElementAt(mapCoords);
            if (surface == nullptr)
            {
                Accessibility::ScreenReaderSpeak("Cannot build here");
                return;
            }

            clearProvisional();

            int32_t baseZ = floor2(surface->getBaseZ(), kCoordsZStep);
            if (surface->GetWaterHeight() > 0)
                baseZ = std::max<int32_t>(baseZ, surface->GetWaterHeight());
            // Strictly follow the game's own placement height: getBaseZ (which positions the
            // construction ghost, and the mouse tool's real placement) adds this design-specific
            // offset so the design sits where the ghost shows it, instead of dropping its origin to
            // the ground. Without it the built ride lands at a different height than the ghost.
            baseZ += TrackDesignGetZPlacement(
                *_trackDesign, RideGetTemporaryForPreview(), { mapCoords, baseZ, _currentTrackPieceDirection });

            // Decide whether clearing scenery would actually let the ride build BEFORE destroying any.
            // The engine auto-clears small scenery as it builds track, so a probe that still fails is
            // blocked by something else: a hard blocker, large scenery, a wall, terrain, or land we
            // cannot build on. We clear (large scenery and walls) only once everything that clearing
            // could NOT fix has been ruled out, so scenery is never destroyed for a doomed placement.
            CoordsXYZ probeLoc = { mapCoords, baseZ };
            const auto asIs = findValidTrackDesignPlaceHeight(probeLoc, {});
            if (asIs.error != GameActions::Status::ok)
            {
                //  - A path, another ride, or a banner on the footprint or an entrance/exit tile.
                //  - Ground that is sloped or rises too high (needs levelling, not clearing).
                //  - Land that is not owned and has no construction rights.
                // Any of these means the ride can never go here, so report and clear nothing.
                if (footprintHasHardBlocker(mapCoords) || footprintTerrainBlocks(mapCoords, baseZ)
                    || footprintOnUnbuildableLand(mapCoords))
                {
                    reportPlacementFailure(mapCoords, baseZ);
                    return;
                }

                // The only thing left that can be blocking is clearable large scenery or walls.
                clearFootprintScenery(mapCoords);
            }

            CoordsXYZ trackLoc = { mapCoords, baseZ };
            auto res = findValidTrackDesignPlaceHeight(trackLoc, {});
            if (res.error != GameActions::Status::ok)
            {
                reportPlacementFailure(mapCoords, baseZ);
                return;
            }

            _placingTrackDesign = true;
            auto tdAction = GameActions::TrackDesignAction(
                { trackLoc, _currentTrackPieceDirection }, *_trackDesign, !gTrackDesignSceneryToggle,
                Config::Get().general.defaultInspectionInterval);
            tdAction.SetCallback([&, trackLoc](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error != GameActions::Status::ok)
                {
                    Audio::Play3D(Audio::SoundId::error, result->position);
                    auto* windowMgr = GetWindowManager();
                    windowMgr->ShowError(result->getErrorTitle(), result->getErrorMessage());
                    announcePlacementFailure(result->error, CoordsXY{ trackLoc }, trackLoc.z);
                    _placingTrackDesign = false;
                    return;
                }

                rideId = result->getData<RideId>();
                auto getRide = GetRide(rideId);
                if (getRide != nullptr)
                {
                    auto* windowMgr = GetWindowManager();
                    windowMgr->CloseByClass(WindowClass::error);
                    Audio::Play3D(Audio::SoundId::placeItem, trackLoc);
                    _currentRideIndex = rideId;
                    // Sweep any scenery or walls still sharing the ride's tiles so nothing is left
                    // under it. This runs only after a successful build, so it can never destroy
                    // scenery for a placement that failed.
                    clearFootprintScenery(CoordsXY{ trackLoc });
                    _accPreviewing = false; // the ride is down; leave preview mode
                    Accessibility::ScreenReaderSpeak("Ride placed");

                    if (TrackDesignAreEntranceAndExitPlaced())
                    {
                        auto intent = Intent(WindowClass::ride);
                        intent.PutExtra(INTENT_EXTRA_RIDE_ID, rideId.ToUnderlying());
                        ContextOpenIntent(&intent);
                        auto* wnd = windowMgr->FindByClass(WindowClass::trackDesignPlace);
                        if (wnd != nullptr)
                            windowMgr->Close(*wnd);
                    }
                    else
                    {
                        RideInitialiseConstructionWindow(*getRide);
                        auto* wnd = windowMgr->FindByClass(WindowClass::rideConstruction);
                        if (wnd != nullptr)
                            wnd->onMouseUp(WC_RIDE_CONSTRUCTION__WIDX_ENTRANCE);
                    }
                }
                _placingTrackDesign = false;
            });
            GameActions::Execute(&tdAction, getGameState());
        }

        void DrawMiniPreview(const TrackDesign& td)
        {
            ClearMiniPreview();

            // First pass is used to determine the width and height of the image so it can centre it
            CoordsXY min = { 0, 0 };
            CoordsXY max = { 0, 0 };
            for (int32_t pass = 0; pass < 2; pass++)
            {
                CoordsXY origin = { 0, 0 };
                if (pass == 1)
                {
                    origin.x -= ((max.x + min.x) >> 6) * kCoordsXYStep;
                    origin.y -= ((max.y + min.y) >> 6) * kCoordsXYStep;
                }

                const auto& rtd = GetRideTypeDescriptor(td.trackAndVehicle.rtdIndex);
                if (rtd.specialType == RtdSpecialType::maze)
                {
                    drawMiniPreviewMaze(td, pass, origin, min, max);
                }
                else
                {
                    drawMiniPreviewTrack(td, pass, origin, min, max);
                }
            }
        }

        void ClearMiniPreview()
        {
            // Fill with transparent colour.
            std::fill(_miniPreview.begin(), _miniPreview.end(), PaletteIndex::transparent);
        }

    private:
        void clearProvisional()
        {
            if (_hasPlacementGhost)
            {
                auto newRide = GetRide(_placementGhostRideId);
                if (newRide != nullptr)
                {
                    TrackDesignPreviewRemoveGhosts(*_trackDesign, *newRide, _placementGhostLoc);
                    _hasPlacementGhost = false;
                }

                VirtualFloorSetHeight(0);
            }
        }

        std::optional<int32_t> getBaseZ([[maybe_unused]] const CoordsXY& loc, const ScreenCoordsXY& screenCoords)
        {
            CoordsXY mapCoords = ViewportInteractionGetTileStartAtCursor(screenCoords);
            auto surfaceElement = MapGetSurfaceElementAt(mapCoords);
            if (surfaceElement == nullptr)
                return std::nullopt;

            auto& im = GetInputManager();

            if (!_trackPlaceCtrlState && im.isModifierKeyPressed(ModifierKey::ctrl))
            {
                constexpr auto interactionFlags = EnumsToFlags(
                    ViewportInteractionItem::terrain, ViewportInteractionItem::ride, ViewportInteractionItem::scenery,
                    ViewportInteractionItem::footpath, ViewportInteractionItem::wall, ViewportInteractionItem::largeScenery);

                auto info = GetMapCoordinatesFromPos(screenCoords, interactionFlags);
                if (info.interactionType == ViewportInteractionItem::terrain)
                {
                    _trackPlaceCtrlZ = floor2(surfaceElement->getBaseZ(), kCoordsZStep);

                    // Increase Z above water
                    if (surfaceElement->GetWaterHeight() > 0)
                        _trackPlaceCtrlZ = std::max(_trackPlaceCtrlZ, surfaceElement->GetWaterHeight());
                }
                else
                {
                    _trackPlaceCtrlZ = floor2(info.Element->getBaseZ(), kCoordsZStep);
                }

                _trackPlaceCtrlState = true;
            }
            else if (!im.isModifierKeyPressed(ModifierKey::ctrl))
            {
                _trackPlaceCtrlState = false;
                _trackPlaceCtrlZ = 0;
            }

            if (!_trackPlaceShiftState && im.isModifierKeyPressed(ModifierKey::shift))
            {
                _trackPlaceShiftState = true;
                _trackPlaceShiftStart = screenCoords;
                _trackPlaceShiftZ = 0;
            }
            else if (im.isModifierKeyPressed(ModifierKey::shift))
            {
                uint16_t newMaxHeight = ZoomLevel::max().ApplyTo(
                    std::numeric_limits<decltype(TileElement::baseHeight)>::max() - 32);

                _trackPlaceShiftZ = _trackPlaceShiftStart.y - screenCoords.y + 4;

                // Scale delta by zoom to match mouse position.
                auto* mainWnd = WindowGetMain();
                if (mainWnd != nullptr && mainWnd->viewport != nullptr)
                    _trackPlaceShiftZ = mainWnd->viewport->zoom.ApplyTo(_trackPlaceShiftZ);

                // Floor to closest kCoordsZStep
                _trackPlaceShiftZ = floor2(_trackPlaceShiftZ, kCoordsZStep);

                // Clamp to maximum possible value of BaseHeight can offer.
                _trackPlaceShiftZ = std::min<int16_t>(_trackPlaceShiftZ, newMaxHeight);
            }
            else if (_trackPlaceShiftState)
            {
                _trackPlaceShiftState = false;
                _trackPlaceShiftZ = 0;
            }

            if (!_trackPlaceCtrlState)
            {
                _trackPlaceZ = floor2(surfaceElement->getBaseZ(), kCoordsZStep);

                // Increase Z above water
                if (surfaceElement->GetWaterHeight() > 0)
                    _trackPlaceZ = std::max(_trackPlaceZ, surfaceElement->GetWaterHeight());

                if (_trackPlaceShiftState)
                {
                    _trackPlaceZ += _trackPlaceShiftZ;
                    _trackPlaceZ = std::max<int16_t>(16, _trackPlaceZ);
                }
            }
            else
            {
                _trackPlaceZ = _trackPlaceCtrlZ;
                if (_trackPlaceShiftState)
                    _trackPlaceZ += _trackPlaceShiftZ;

                _trackPlaceZ = std::max<int32_t>(16, _trackPlaceZ);
            }

            if (mapCoords.x == kLocationNull)
                return std::nullopt;

            // Trigger underground view?
            auto* mainWnd = WindowGetMain();
            if (mainWnd != nullptr && mainWnd->viewport != nullptr)
            {
                if (_trackPlaceZ < surfaceElement->getBaseZ() && !_triggeredUndergroundView)
                {
                    mainWnd->viewport->flags |= VIEWPORT_FLAG_UNDERGROUND_INSIDE;
                    _triggeredUndergroundView = true;
                }
                else if (_trackPlaceZ >= surfaceElement->getBaseZ() && _triggeredUndergroundView)
                {
                    mainWnd->viewport->flags &= ~VIEWPORT_FLAG_UNDERGROUND_INSIDE;
                    _triggeredUndergroundView = false;
                }
            }

            // Force placement at the designated position if modifiers are used
            if (_trackPlaceShiftState || _trackPlaceCtrlState)
                return _trackPlaceZ;

            // Figure out a good position to place the design, taking other elements and surface height into account
            return _trackPlaceZ
                + TrackDesignGetZPlacement(
                       *_trackDesign, RideGetTemporaryForPreview(), { mapCoords, _trackPlaceZ, _currentTrackPieceDirection });
        }

        void drawMiniPreviewEntrances(
            const TrackDesign& td, int32_t pass, const CoordsXY& origin, CoordsXY& min, CoordsXY& max, Direction rotation)
        {
            for (const auto& entrance : td.entranceElements)
            {
                auto rotatedAndOffsetEntrance = origin + entrance.location.ToCoordsXY().Rotate(rotation);

                if (pass == 0)
                {
                    min.x = std::min(min.x, rotatedAndOffsetEntrance.x);
                    max.x = std::max(max.x, rotatedAndOffsetEntrance.x);
                    min.y = std::min(min.y, rotatedAndOffsetEntrance.y);
                    max.y = std::max(max.y, rotatedAndOffsetEntrance.y);
                }
                else
                {
                    auto pixelPosition = drawMiniPreviewGetPixelPosition(rotatedAndOffsetEntrance);
                    if (drawMiniPreviewIsPixelInBounds(pixelPosition))
                    {
                        PaletteIndex* pixel = drawMiniPreviewGetPixelPtr(pixelPosition);
                        auto colour = entrance.isExit ? kPaletteIndexColourExit : kPaletteIndexColourEntrance;
                        for (int32_t i = 0; i < 4; i++)
                        {
                            pixel[338 + i] = colour; // x + 2, y + 2
                            pixel[168 + i] = colour; //        y + 1
                            pixel[2 + i] = colour;   // x + 2
                            pixel[172 + i] = colour; // x + 4, y + 1
                        }
                    }
                }
            }
        }

        void drawMiniPreviewTrack(const TrackDesign& td, int32_t pass, const CoordsXY& origin, CoordsXY& min, CoordsXY& max)
        {
            const uint8_t rotation = (_currentTrackPieceDirection + GetCurrentRotation()) & 3;

            CoordsXY curTrackStart = origin;
            uint8_t curTrackRotation = rotation;
            for (const auto& trackElement : td.trackElements)
            {
                // Follow a single track piece shape
                const auto& ted = GetTrackElementDescriptor(trackElement.type);
                for (size_t sequenceIndex = 0; sequenceIndex < ted.sequenceData.numSequences; sequenceIndex++)
                {
                    const auto& trackBlock = ted.sequenceData.sequences[sequenceIndex].clearance;
                    auto rotatedAndOffsetTrackBlock = curTrackStart
                        + CoordsXY{ trackBlock.x, trackBlock.y }.Rotate(curTrackRotation);

                    if (pass == 0)
                    {
                        min.x = std::min(min.x, rotatedAndOffsetTrackBlock.x);
                        max.x = std::max(max.x, rotatedAndOffsetTrackBlock.x);
                        min.y = std::min(min.y, rotatedAndOffsetTrackBlock.y);
                        max.y = std::max(max.y, rotatedAndOffsetTrackBlock.y);
                    }
                    else
                    {
                        auto pixelPosition = drawMiniPreviewGetPixelPosition(rotatedAndOffsetTrackBlock);
                        if (drawMiniPreviewIsPixelInBounds(pixelPosition))
                        {
                            PaletteIndex* pixel = drawMiniPreviewGetPixelPtr(pixelPosition);

                            auto bits = trackBlock.quarterTile.Rotate(curTrackRotation & 3).GetBaseQuarterOccupied();

                            // Station track is a lighter colour
                            auto colour = ted.sequenceData.sequences[0].flags.has(SequenceFlag::trackOrigin)
                                ? kPaletteIndexColourStation
                                : kPaletteIndexColourTrack;

                            for (int32_t i = 0; i < 4; i++)
                            {
                                if (bits & 1)
                                    pixel[338 + i] = colour; // x + 2, y + 2
                                if (bits & 2)
                                    pixel[168 + i] = colour; //        y + 1
                                if (bits & 4)
                                    pixel[2 + i] = colour; // x + 2
                                if (bits & 8)
                                    pixel[172 + i] = colour; // x + 4, y + 1
                            }
                        }
                    }
                }

                // Change rotation and next position based on track curvature
                curTrackRotation &= 3;

                const TrackCoordinates* track_coordinate = &ted.coordinates;

                curTrackStart += CoordsXY{ track_coordinate->x, track_coordinate->y }.Rotate(curTrackRotation);
                curTrackRotation += track_coordinate->rotationEnd - track_coordinate->rotationBegin;
                curTrackRotation &= 3;
                if (track_coordinate->rotationEnd & 4)
                {
                    curTrackRotation |= 4;
                }
                if (!(curTrackRotation & 4))
                {
                    curTrackStart += CoordsDirectionDelta[curTrackRotation];
                }
            }

            drawMiniPreviewEntrances(td, pass, origin, min, max, rotation);
        }

        void drawMiniPreviewMaze(const TrackDesign& td, int32_t pass, const CoordsXY& origin, CoordsXY& min, CoordsXY& max)
        {
            uint8_t rotation = (_currentTrackPieceDirection + GetCurrentRotation()) & 3;
            for (const auto& mazeElement : td.mazeElements)
            {
                auto rotatedMazeCoords = origin + mazeElement.location.ToCoordsXY().Rotate(rotation);

                if (pass == 0)
                {
                    min.x = std::min(min.x, rotatedMazeCoords.x);
                    max.x = std::max(max.x, rotatedMazeCoords.x);
                    min.y = std::min(min.y, rotatedMazeCoords.y);
                    max.y = std::max(max.y, rotatedMazeCoords.y);
                }
                else
                {
                    auto pixelPosition = drawMiniPreviewGetPixelPosition(rotatedMazeCoords);
                    if (drawMiniPreviewIsPixelInBounds(pixelPosition))
                    {
                        auto* pixel = drawMiniPreviewGetPixelPtr(pixelPosition);

                        auto colour = kPaletteIndexColourTrack;
                        for (int32_t i = 0; i < 4; i++)
                        {
                            pixel[338 + i] = colour; // x + 2, y + 2
                            pixel[168 + i] = colour; //        y + 1
                            pixel[2 + i] = colour;   // x + 2
                            pixel[172 + i] = colour; // x + 4, y + 1
                        }
                    }
                }
            }

            drawMiniPreviewEntrances(td, pass, origin, min, max, rotation);
        }

        ScreenCoordsXY drawMiniPreviewGetPixelPosition(const CoordsXY& location)
        {
            auto tilePos = TileCoordsXY(location);
            return { (80 + (tilePos.y - tilePos.x) * 4), (38 + (tilePos.y + tilePos.x) * 2) };
        }

        bool drawMiniPreviewIsPixelInBounds(const ScreenCoordsXY& pixel)
        {
            return pixel.x >= 0 && pixel.y >= 0 && pixel.x <= 160 && pixel.y <= 75;
        }

        PaletteIndex* drawMiniPreviewGetPixelPtr(const ScreenCoordsXY& pixel)
        {
            return &_miniPreview[pixel.y * kTrackMiniPreviewSize.width + pixel.x];
        }

        GameActions::Result findValidTrackDesignPlaceHeight(CoordsXYZ& loc, CommandFlags newFlags)
        {
            GameActions::Result res;
            for (int32_t i = 0; i < 7; i++, loc.z += kCoordsZStep)
            {
                auto tdAction = GameActions::TrackDesignAction(
                    CoordsXYZD{ loc.x, loc.y, loc.z, _currentTrackPieceDirection }, *_trackDesign, !gTrackDesignSceneryToggle,
                    Config::Get().general.defaultInspectionInterval);
                tdAction.SetFlags(newFlags);
                res = GameActions::Query(&tdAction, getGameState());

                // If successful don't keep trying.
                // If failure due to no money then increasing height only makes problem worse
                if (res.error == GameActions::Status::ok || res.error == GameActions::Status::insufficientFunds)
                {
                    return res;
                }
            }
            return res;
        }
    };

    WindowBase* TrackPlaceOpen(const TrackDesignFileRef* tdFileRef)
    {
        std::unique_ptr<TrackDesign> openTrackDesign = TrackDesignImport(tdFileRef->path.c_str());

        if (openTrackDesign == nullptr)
        {
            return nullptr;
        }

        auto* windowMgr = GetWindowManager();
        windowMgr->CloseConstructionWindows();

        auto* window = windowMgr->FocusOrCreate<TrackDesignPlaceWindow>(WindowClass::trackDesignPlace, kWindowSize, {});
        if (window != nullptr)
        {
            window->init(std::move(openTrackDesign));
        }
        return window;
    }

    void TrackPlaceClearProvisionalTemporarily()
    {
        auto* windowMgr = GetWindowManager();
        auto* trackPlaceWnd = static_cast<TrackDesignPlaceWindow*>(windowMgr->FindByClass(WindowClass::trackDesignPlace));
        if (trackPlaceWnd != nullptr)
        {
            trackPlaceWnd->ClearProvisionalTemporarily();
        }
    }

    void TrackPlaceRestoreProvisional()
    {
        auto* windowMgr = GetWindowManager();
        auto* trackPlaceWnd = static_cast<TrackDesignPlaceWindow*>(windowMgr->FindByClass(WindowClass::trackDesignPlace));
        if (trackPlaceWnd != nullptr)
        {
            trackPlaceWnd->RestoreProvisional();
        }
    }

    static TrackDesignPlaceWindow* GetTrackPlaceWindow()
    {
        auto* windowMgr = GetWindowManager();
        return static_cast<TrackDesignPlaceWindow*>(windowMgr->FindByClass(WindowClass::trackDesignPlace));
    }

    bool WindowTrackPlaceIsActive()
    {
        return GetTrackPlaceWindow() != nullptr;
    }

    void WindowTrackPlaceRotate()
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            w->rotateForAccessibility();
    }

    void WindowTrackPlaceAtTile(const CoordsXY& mapCoords)
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            w->placeAtTile(mapCoords);
    }

    void WindowTrackPlaceUpdateGhost(const CoordsXY& mapCoords)
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            w->accUpdateGhost(mapCoords);
    }

    void WindowTrackPlaceCancel()
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            w->close();
    }

    void WindowTrackPlacePickup()
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            w->pickupForAccessibility();
    }

    std::optional<std::string> WindowTrackPlacePreviewLabel(const TileCoordsXY& tile)
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            return w->previewLabelForTile(tile);
        return std::nullopt;
    }

    std::vector<std::pair<CoordsXY, std::string>> WindowTrackPlaceEntranceExitTiles()
    {
        if (auto* w = GetTrackPlaceWindow(); w != nullptr)
            return w->previewEntranceExitTiles();
        return {};
    }
} // namespace OpenRCT2::Ui::Windows
