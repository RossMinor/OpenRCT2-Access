/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MapNavigation.h"

#include "RidePlacement.h"
#include "SceneryPlacement.h"
#include "ScreenReader.h"

#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <openrct2-ui/UiStringIds.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathPlaceAction.h>
#include <openrct2/actions/footpath/FootpathRemoveAction.h>
#include <openrct2/actions/terraform/ClearAction.h>
#include <openrct2/actions/terraform/LandLowerAction.h>
#include <openrct2/actions/terraform/LandRaiseAction.h>
#include <openrct2/actions/terraform/WaterLowerAction.h>
#include <openrct2/actions/terraform/WaterRaiseAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/Date.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/config/Config.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/interface/WindowClasses.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.Date.h>
#include <openrct2/object/FootpathEntry.h>
#include <openrct2/object/FootpathSurfaceObject.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectLimits.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectTypes.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/tile_element/EntranceElement.h>
#include <openrct2/world/tile_element/LargeSceneryElement.h>
#include <openrct2/world/tile_element/PathElement.h>
#include <openrct2/world/tile_element/SmallSceneryElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TileElement.h>
#include <openrct2/world/tile_element/TrackElement.h>
#include <openrct2/world/tile_element/WallElement.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Cursor state. Coordinates are absolute map tile coordinates; the origin lets us
    // report them relative to the bottom-left usable corner of the whole map.
    static bool _initialised = false;
    static TileCoordsXY _origin{};
    static TileCoordsXY _cursor{};

    // The key consumed on key-down, so its key-up can be swallowed too.
    static uint32_t _lastHandledKey = 0;

    // When true, arrow keys navigate the in-game toolbar menu instead of the map cursor.
    static bool _menuMode = false;

    // Description of the tile the cursor was last over, so we only announce on change.
    static std::string _lastTileDescription;

    // Cache of the last computed ride footprint, to avoid rescanning the map repeatedly.
    static RideId _cachedBoundsRide = RideId::GetNull();
    static TileCoordsXY _cachedBoundsMin{};
    static TileCoordsXY _cachedBoundsMax{};
    static bool _cachedBoundsValid = false;

    // Last mouse position we observed, used to detect genuine mouse movement for the hover
    // read-out. Seeded on the first gameplay frame.
    static ScreenCoordsXY _lastMousePos{};
    static bool _mouseTracking = false;

    // Description of the tile the mouse last hovered, so the hover read-out only speaks on
    // change. Kept separate from _lastTileDescription (the keyboard cursor) so the two never
    // interfere with each other.
    static std::string _lastHoverDescription;

    // When true, the mouse drives the game normally (edge scroll, clicks) and the arrow-key
    // map cursor is disabled. Toggled with Ctrl+Space. Default false = keyboard cursor mode.
    static bool _mouseMode = false;

    // Edge length (in tiles) of the square brush used for clearing scenery and terraforming.
    // Cycles 1, 3, 5, 7. Larger brushes carve out flat pads for big rides in one keypress.
    static int32_t _brushSize = 1;

    // Elevation tone: a short sine beep whose pitch rises with terrain height. It plays only
    // when the cursor moves onto a tile at a different elevation, so scanning flat ground stays
    // silent. The sine sample is synthesised once and cached; pitch is set per play via the
    // mixer's playback rate.
    static int32_t _lastElevation = -1; // surface->baseHeight/2 of the previous tile; -1 = none yet

    // Read-only Z-axis probe. _scanHeight is a tile-element baseHeight; plain Page Up/Down step it
    // to the next element above/below on the cursor's tile to report what is stacked there. Reset
    // to the surface whenever the cursor moves to a new tile.
    static int32_t _scanHeight = 0;

    // Pitch range mapped across elevation. Capped at 1 kHz so the highest terrain never gets
    // piercing; kElevToneRange is how many elevation steps span the full min..max sweep.
    static constexpr double kElevToneMinFreq = 220.0;
    static constexpr double kElevToneMaxFreq = 1000.0;
    static constexpr int32_t kElevToneRange = 50;

    // One synthesised sine source per (clamped) elevation step, generated lazily and cached for
    // the session. Each is rendered at its exact target frequency and played at rate 1.0, which
    // bypasses the mixer's resampler - a crude linear interpolator that adds a faint buzz to any
    // pitch-shifted tone. Index = clamped elevation in [0, kElevToneRange].
    static Audio::IAudioSource* _elevationToneSources[kElevToneRange + 1] = {};

    static bool IsTileOwned(const TileCoordsXY& tile)
    {
        auto* surface = MapGetSurfaceElementAt(tile);
        return surface != nullptr && (surface->GetOwnership() & OWNERSHIP_OWNED) != 0;
    }

    // Returns the ride occupying this tile (via its track/structure), or null if none.
    static RideId GetRideAtTile(const TileCoordsXY& tile)
    {
        TileElement* element = MapGetFirstElementAt(tile);
        if (element == nullptr)
            return RideId::GetNull();

        do
        {
            if (auto* track = element->asTrack(); track != nullptr)
                return track->GetRideIndex();
        } while (!(element++)->isLastForTile());

        return RideId::GetNull();
    }

    // Computes the tile bounding box of a ride's footprint (cached for the last ride).
    static bool ComputeRideBounds(RideId rideId, TileCoordsXY& outMin, TileCoordsXY& outMax)
    {
        if (rideId.IsNull())
            return false;

        if (_cachedBoundsValid && _cachedBoundsRide == rideId)
        {
            outMin = _cachedBoundsMin;
            outMax = _cachedBoundsMax;
            return true;
        }

        bool found = false;
        const auto mapSize = getGameState().mapSize;
        for (int32_t y = 0; y < mapSize.y; y++)
        {
            for (int32_t x = 0; x < mapSize.x; x++)
            {
                if (GetRideAtTile(TileCoordsXY{ x, y }) != rideId)
                    continue;

                if (!found)
                {
                    found = true;
                    outMin = TileCoordsXY{ x, y };
                    outMax = TileCoordsXY{ x, y };
                }
                else
                {
                    outMin.x = std::min(outMin.x, x);
                    outMin.y = std::min(outMin.y, y);
                    outMax.x = std::max(outMax.x, x);
                    outMax.y = std::max(outMax.y, y);
                }
            }
        }

        if (found)
        {
            _cachedBoundsValid = true;
            _cachedBoundsRide = rideId;
            _cachedBoundsMin = outMin;
            _cachedBoundsMax = outMax;
        }
        return found;
    }

    // A ride's name and footprint size, e.g. "Wooden Coaster 1, 4 by 4".
    static std::string GetRideDescription(RideId rideId)
    {
        auto* ride = GetRide(rideId);
        if (ride == nullptr)
            return {};

        std::string text = ride->getName();
        TileCoordsXY mn, mx;
        if (ComputeRideBounds(rideId, mn, mx))
        {
            const int32_t w = mx.x - mn.x + 1;
            const int32_t h = mx.y - mn.y + 1;
            text += ", " + std::to_string(w) + " by " + std::to_string(h);
        }
        return text;
    }

    // The localised name of a loaded object, or an empty string if not found.
    static std::string GetObjectName(ObjectType type, ObjectEntryIndex index)
    {
        auto& objManager = GetContext()->GetObjectManager();
        auto* obj = objManager.GetLoadedObject(type, index);
        return obj != nullptr ? std::string(obj->GetName()) : std::string();
    }

    // Describes what is on a tile: rides, the park entrance, ride entrances/exits, paths,
    // queues, fences, scenery (named), water, or empty land (inside or outside the park).
    static std::string GetTileDescription(const TileCoordsXY& tile)
    {
        RideId ride = RideId::GetNull();
        bool parkEntrance = false, rideEntrance = false, rideExit = false;
        bool queue = false, path = false, wall = false, scenery = false;
        std::string wallName, sceneryName, pathName;

        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (auto* track = el->asTrack(); track != nullptr)
            {
                ride = track->GetRideIndex();
            }
            else if (auto* entrance = el->asEntrance(); entrance != nullptr)
            {
                switch (entrance->GetEntranceType())
                {
                    case ENTRANCE_TYPE_PARK_ENTRANCE:
                        parkEntrance = true;
                        break;
                    case ENTRANCE_TYPE_RIDE_ENTRANCE:
                        rideEntrance = true;
                        break;
                    case ENTRANCE_TYPE_RIDE_EXIT:
                        rideExit = true;
                        break;
                }
            }
            else if (auto* p = el->asPath(); p != nullptr)
            {
                path = true;
                queue = p->IsQueue();
                if (pathName.empty())
                {
                    if (p->HasLegacyPathEntry())
                        pathName = GetObjectName(ObjectType::paths, p->GetLegacyPathEntryIndex());
                    else
                        pathName = GetObjectName(ObjectType::footpathSurface, p->GetSurfaceEntryIndex());
                }
            }
            else if (auto* w = el->asWall(); w != nullptr)
            {
                wall = true;
                if (wallName.empty())
                    wallName = GetObjectName(ObjectType::walls, w->GetEntryIndex());
            }
            else if (auto* ss = el->asSmallScenery(); ss != nullptr)
            {
                scenery = true;
                if (sceneryName.empty())
                    sceneryName = GetObjectName(ObjectType::smallScenery, ss->GetEntryIndex());
            }
            else if (auto* ls = el->asLargeScenery(); ls != nullptr)
            {
                scenery = true;
                if (sceneryName.empty())
                    sceneryName = GetObjectName(ObjectType::largeScenery, ls->GetEntryIndex());
            }

            if (el->isLastForTile())
                break;
            el++;
        }

        if (!ride.IsNull())
            return GetRideDescription(ride);
        if (parkEntrance)
            return "Park entrance";
        if (rideEntrance)
            return "Ride entrance";
        if (rideExit)
            return "Ride exit";
        if (queue)
        {
            if (pathName.empty())
                return "Queue line";
            // Queue surfaces are usually named just by colour, so append "queue" unless the
            // name already mentions it.
            std::string lower = pathName;
            for (auto& c : lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return (lower.find("queue") != std::string::npos) ? pathName : pathName + " queue";
        }
        if (path)
            return pathName.empty() ? "Path" : pathName;
        if (wall)
            return wallName.empty() ? "Fence" : wallName;
        if (scenery)
            return sceneryName.empty() ? "Scenery" : sceneryName;

        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface != nullptr && surface->GetWaterHeight() > 0)
            return "Water";

        const bool owned = surface != nullptr && (surface->GetOwnership() & OWNERSHIP_OWNED) != 0;
        return owned ? "Empty" : "Outside park";
    }

    // Sets the coordinate origin to the bottom-left usable map corner and picks a start tile.
    static void InitialiseCursor()
    {
        _initialised = true;
        _lastTileDescription.clear();
        _lastElevation = -1;

        const auto mapSize = getGameState().mapSize;
        _origin = TileCoordsXY{ 1, 1 };

        // Start on the first owned tile (inside the park) if there is one, else the centre.
        _cursor = TileCoordsXY{ mapSize.x / 2, mapSize.y / 2 };
        for (int32_t y = 1; y <= mapSize.y - 2; y++)
        {
            bool found = false;
            for (int32_t x = 1; x <= mapSize.x - 2; x++)
            {
                if (IsTileOwned(TileCoordsXY{ x, y }))
                {
                    _cursor = TileCoordsXY{ x, y };
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
    }

    // Centres the view on the keyboard cursor. The mouse is independent and never moved by us.
    static void CentreViewportOnCursor()
    {
        auto* w = WindowGetMain();
        if (w == nullptr)
            return;

        auto loc = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ().ToTileCentre();
        loc.z = TileElementHeight(loc);
        WindowScrollToLocation(*w, loc);
    }

    void FollowCursorTo(const TileCoordsXY& tile)
    {
        if (!_initialised)
            InitialiseCursor();

        const auto mapSize = getGameState().mapSize;
        if (tile.x < 1 || tile.y < 1 || tile.x > mapSize.x - 2 || tile.y > mapSize.y - 2)
            return;

        _cursor = tile;
        CentreViewportOnCursor();

        // Keep the cursor's own bookkeeping coherent so it behaves normally once following ends.
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            _lastElevation = surface->baseHeight / 2;
            _scanHeight = surface->baseHeight;
        }
        _lastTileDescription = GetTileDescription(_cursor);
    }

    // Builds a mono 16-bit PCM WAV in memory holding a sine wave at the given frequency, with
    // short fade in/out so the beep starts and ends without an audible click. Returns the raw
    // bytes of a complete .wav file, ready to hand to CreateStreamFromWAV.
    static std::vector<uint8_t> BuildSineWav(double freq, int32_t sampleRate, double seconds, double amplitude)
    {
        const int32_t numSamples = static_cast<int32_t>(sampleRate * seconds);
        const uint16_t numChannels = 1;
        const uint16_t bitsPerSample = 16;
        const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
        const uint32_t dataSize = static_cast<uint32_t>(numSamples) * blockAlign;

        std::vector<uint8_t> buf;
        buf.reserve(44 + dataSize);
        const auto put16 = [&](uint16_t v) {
            buf.push_back(v & 0xFF);
            buf.push_back((v >> 8) & 0xFF);
        };
        const auto put32 = [&](uint32_t v) {
            buf.push_back(v & 0xFF);
            buf.push_back((v >> 8) & 0xFF);
            buf.push_back((v >> 16) & 0xFF);
            buf.push_back((v >> 24) & 0xFF);
        };
        const auto putTag = [&](const char* s) {
            for (int32_t i = 0; i < 4; i++)
                buf.push_back(static_cast<uint8_t>(s[i]));
        };

        putTag("RIFF");
        put32(36 + dataSize);
        putTag("WAVE");
        putTag("fmt ");
        put32(16);  // PCM fmt chunk size
        put16(1);   // audio format = PCM
        put16(numChannels);
        put32(static_cast<uint32_t>(sampleRate));
        put32(byteRate);
        put16(blockAlign);
        put16(bitsPerSample);
        putTag("data");
        put32(dataSize);

        constexpr double kPi = 3.14159265358979323846;
        const double step = 2.0 * kPi * freq / sampleRate;
        const int32_t attack = std::max(1, sampleRate / 100); // ~10 ms fade-in
        const int32_t release = std::max(1, sampleRate / 50);  // ~20 ms fade-out
        for (int32_t i = 0; i < numSamples; i++)
        {
            double env = 1.0;
            if (i < attack)
                env = static_cast<double>(i) / attack;
            else if (i >= numSamples - release)
                env = static_cast<double>(numSamples - i) / release;
            const double sample = std::sin(step * i) * amplitude * env;
            put16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(sample * 32767.0))));
        }
        return buf;
    }

    // Plays the elevation beep at a pitch encoding the given elevation (surface baseHeight/2).
    // The sine for each elevation step is synthesised at its exact target frequency on first use
    // and cached, then played at rate 1.0 so the mixer never resamples it. Higher elevation ->
    // higher pitch, clamped to the cap.
    static void PlayElevationTone(int32_t elevation)
    {
        if (!Audio::IsAvailable())
            return;

        const int32_t clamped = std::clamp(elevation, 0, kElevToneRange);
        Audio::IAudioSource*& source = _elevationToneSources[clamped];
        if (source == nullptr)
        {
            const double frac = static_cast<double>(clamped) / kElevToneRange;
            // Sweep pitch geometrically (log-frequency) so each elevation step sounds evenly spaced.
            const double freq = kElevToneMinFreq * std::pow(kElevToneMaxFreq / kElevToneMinFreq, frac);
            auto wav = BuildSineWav(freq, 44100, 0.12, 0.35);
            auto stream = std::make_unique<MemoryStream>(wav);
            source = GetContext()->GetAudioContext().CreateStreamFromWAV(std::move(stream));
        }
        if (source == nullptr)
            return;

        Audio::CreateAudioChannel(source, Audio::MixerGroup::Sound, false, Audio::kMixerVolumeMax, 0.5f, 1.0, true);
    }

    static void Move(int32_t dx, int32_t dy, const char* directionName)
    {
        const auto mapSize = getGameState().mapSize;
        const TileCoordsXY target{ _cursor.x + dx, _cursor.y + dy };

        // The usable map excludes the outermost ring; its edge is the true map border.
        if (target.x < 1 || target.y < 1 || target.x > mapSize.x - 2 || target.y > mapSize.y - 2)
        {
            ScreenReaderSpeak(std::string(directionName) + " border");
            return;
        }

        _cursor = target;
        CentreViewportOnCursor();

        // Elevation tone: beep only when the new tile's height differs from the last one, so
        // moving across flat ground stays silent. Pitch rises with elevation.
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            const int32_t elevation = surface->baseHeight / 2;
            if (elevation != _lastElevation)
            {
                PlayElevationTone(elevation);
                _lastElevation = elevation;
            }
            _scanHeight = surface->baseHeight; // the Z-axis probe starts from ground on each move
        }

        // Announce the tile only when its description changes from the previous tile.
        std::string description = GetTileDescription(_cursor);
        if (description != _lastTileDescription)
        {
            ScreenReaderSpeak(description);
            _lastTileDescription = std::move(description);
        }
    }

    static void ReadCoordinates()
    {
        const int32_t x = _cursor.x - _origin.x;
        const int32_t y = _cursor.y - _origin.y;
        std::string text = "X " + std::to_string(x) + ", Y " + std::to_string(y);
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
            text += ", elevation " + std::to_string(surface->baseHeight / 2);
        ScreenReaderSpeak(text);
    }

    static void AnnounceMoney()
    {
        const auto cash = getGameState().park.cash;
        const StringId fmt = cash < 0 ? STR_BOTTOM_TOOLBAR_CASH_NEGATIVE : STR_BOTTOM_TOOLBAR_CASH;
        ScreenReaderSpeak(OpenRCT2::FormatStringID(fmt, cash));
    }

    static void AnnounceDateTime()
    {
        auto& date = GetDate();
        const int32_t year = date.GetYear() + 1;
        const int32_t month = date.GetMonth();
        const int32_t day = date.GetDay();

        const StringId fmt = DateFormatStringFormatIds[Config::Get().general.dateFormat];
        Formatter ft;
        ft.Add<StringId>(DateDayNames[day]);
        ft.Add<int16_t>(static_cast<int16_t>(month));
        ft.Add<int16_t>(static_cast<int16_t>(year));
        std::string text = OpenRCT2::FormatStringIDLegacy(fmt, ft.Data());

        if (GameIsPaused())
            text += ", paused";
        ScreenReaderSpeak(text);
    }

    static WindowBase* GetToolbar()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr != nullptr ? windowMgr->FindByClass(WindowClass::topToolbar) : nullptr;
    }

    static bool EnterMenuMode()
    {
        auto* toolbar = GetToolbar();
        if (toolbar == nullptr)
            return false;

        _menuMode = true;
        toolbar->onAccessibilityAction(AccessibilityAction::cancel);   // reset focus to start
        toolbar->onAccessibilityAction(AccessibilityAction::moveDown); // focus + announce first item
        return true;
    }

    static void ExitMenuMode()
    {
        if (auto* toolbar = GetToolbar(); toolbar != nullptr)
            toolbar->onAccessibilityAction(AccessibilityAction::cancel);
        _menuMode = false;
        ScreenReaderSpeak("Menu closed");
    }

    void LeaveMenuMode()
    {
        if (!_menuMode)
            return;
        if (auto* toolbar = GetToolbar(); toolbar != nullptr)
            toolbar->onAccessibilityAction(AccessibilityAction::cancel);
        _menuMode = false;
    }

    static bool HandleMenuModeKey(uint32_t key)
    {
        auto* toolbar = GetToolbar();
        if (toolbar == nullptr)
        {
            _menuMode = false;
            return false;
        }

        switch (key)
        {
            case SDLK_ESCAPE:
            {
                // If a dropdown sub-menu is open, Escape closes just that and stays in the
                // toolbar menu; otherwise it leaves menu mode entirely.
                auto* windowMgr = GetWindowManager();
                const bool dropdownOpen = windowMgr != nullptr
                    && windowMgr->FindByClass(WindowClass::dropdown) != nullptr;
                if (dropdownOpen)
                    toolbar->onAccessibilityAction(AccessibilityAction::cancel);
                else
                    ExitMenuMode();
                return true;
            }
            case SDLK_TAB:
            case SDLK_DOWN:
            case SDLK_RIGHT:
                toolbar->onAccessibilityAction(AccessibilityAction::moveDown);
                return true;
            case SDLK_UP:
            case SDLK_LEFT:
                toolbar->onAccessibilityAction(AccessibilityAction::moveUp);
                return true;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                toolbar->onAccessibilityAction(AccessibilityAction::activate);
                return true;
            default:
                // First-letter navigation: jump to the next toolbar item starting with this
                // letter. Also swallows other letters so they don't leak to the map cursor.
                if (key >= SDLK_a && key <= SDLK_z)
                    return toolbar->onAccessibilityTypeahead(key);
                return false;
        }
    }

    static void BuildPath()
    {
        // Ensure a valid default path type is selected.
        if (!Windows::WindowFootpathSelectDefault())
        {
            ScreenReaderSpeak("No path type available");
            return;
        }

        // Let the engine work out the base height and slope from the terrain, so paths follow
        // gentle hills (becoming stairs) instead of always being placed flat.
        auto placement = FootpathGetOnTerrainPlacement(_cursor);
        if (!placement.isValid())
        {
            ScreenReaderSpeak("Cannot build a path here");
            return;
        }
        if (placement.slope.type == FootpathSlopeType::irregular)
        {
            ScreenReaderSpeak("Ground is too uneven to build a path here");
            return;
        }

        ObjectEntryIndex type = gFootpathSelection.getSelectedSurface();
        PathConstructFlags flags = 0;
        if (gFootpathSelection.isQueueSelected)
            flags |= PathConstructFlag::IsQueue;
        if (gFootpathSelection.legacyPath != kObjectEntryIndexNull)
        {
            flags |= PathConstructFlag::IsLegacyPathObject;
            type = gFootpathSelection.legacyPath;
        }

        const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
        const CoordsXYZ loc{ world.x, world.y, placement.baseZ };

        auto action = GameActions::FootpathPlaceAction(
            loc, placement.slope, type, gFootpathSelection.railings, kInvalidDirection, flags);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            _lastTileDescription.clear();
            std::string msg = gFootpathSelection.isQueueSelected ? "Queue built" : "Path built";
            if (placement.slope.type == FootpathSlopeType::sloped)
                msg += ", sloped";
            ScreenReaderSpeak(msg);
        }
        // Failures are spoken automatically via the error window.
    }

    // Cycles the selected footpath surface (the current normal or queue set) to the next
    // loaded type and announces it. direction is +1 (next) or -1 (previous).
    static void CyclePathType(int32_t direction)
    {
        if (!Windows::WindowFootpathSelectDefault())
        {
            ScreenReaderSpeak("No path type available");
            return;
        }

        auto& objManager = GetContext()->GetObjectManager();
        const bool queue = gFootpathSelection.isQueueSelected;
        const bool showEditorPaths = (gLegacyScene == LegacyScene::scenarioEditor || getGameState().cheats.sandboxMode);

        std::vector<ObjectEntryIndex> options;
        for (ObjectEntryIndex i = 0; i < kMaxFootpathSurfaceObjects; i++)
        {
            const auto* obj = objManager.GetLoadedObject<FootpathSurfaceObject>(i);
            if (obj == nullptr)
                continue;
            const bool isQueue = (obj->Flags & FOOTPATH_ENTRY_FLAG_IS_QUEUE) != 0;
            if (isQueue != queue)
                continue;
            if ((obj->Flags & FOOTPATH_ENTRY_FLAG_SHOW_ONLY_IN_SCENARIO_EDITOR) && !showEditorPaths)
                continue;
            options.push_back(i);
        }

        if (options.empty())
        {
            ScreenReaderSpeak("No path type available");
            return;
        }

        const ObjectEntryIndex current = queue ? gFootpathSelection.queueSurface : gFootpathSelection.normalSurface;
        int32_t index = 0;
        for (size_t k = 0; k < options.size(); k++)
        {
            if (options[k] == current)
            {
                index = static_cast<int32_t>(k);
                break;
            }
        }
        const int32_t count = static_cast<int32_t>(options.size());
        index = (index + direction + count) % count;
        const ObjectEntryIndex chosen = options[index];

        // Use the modern surface-based selection rather than a combined legacy path object.
        gFootpathSelection.legacyPath = kObjectEntryIndexNull;
        if (queue)
            gFootpathSelection.queueSurface = chosen;
        else
            gFootpathSelection.normalSurface = chosen;

        std::string name = GetObjectName(ObjectType::footpathSurface, chosen);
        ScreenReaderSpeakItem(name.empty() ? "Path type" : name, index, count);
    }

    // Switches between building normal paths and queue paths, announcing the new mode.
    static void ToggleQueueMode()
    {
        if (!Windows::WindowFootpathSelectDefault())
        {
            ScreenReaderSpeak("No path type available");
            return;
        }

        gFootpathSelection.isQueueSelected = !gFootpathSelection.isQueueSelected;
        gFootpathSelection.legacyPath = kObjectEntryIndexNull;

        std::string mode = gFootpathSelection.isQueueSelected ? "Queue paths" : "Normal paths";
        std::string name = GetObjectName(ObjectType::footpathSurface, gFootpathSelection.getSelectedSurface());
        if (!name.empty())
            mode += ", " + name;
        ScreenReaderSpeak(mode);
    }

    // World-coordinate bounds of the brush square centred on the cursor, clamped to the usable
    // map. Outputs the bottom-left (a) and top-right (b) tile corners in world coordinates.
    static void GetBrushBounds(int32_t& ax, int32_t& ay, int32_t& bx, int32_t& by)
    {
        const auto mapSize = getGameState().mapSize;
        const int32_t half = (_brushSize - 1) / 2;
        const int32_t minTileX = std::clamp(_cursor.x - half, 1, mapSize.x - 2);
        const int32_t minTileY = std::clamp(_cursor.y - half, 1, mapSize.y - 2);
        const int32_t maxTileX = std::clamp(_cursor.x + half, 1, mapSize.x - 2);
        const int32_t maxTileY = std::clamp(_cursor.y + half, 1, mapSize.y - 2);
        ax = minTileX * kCoordsXYStep;
        ay = minTileY * kCoordsXYStep;
        bx = maxTileX * kCoordsXYStep;
        by = maxTileY * kCoordsXYStep;
    }

    // Removes small and large scenery (trees, bushes, statues, etc.) across the brush area.
    static void ClearSceneryAtCursor()
    {
        int32_t ax, ay, bx, by;
        GetBrushBounds(ax, ay, bx, by);

        const GameActions::ClearableItems items = GameActions::CLEARABLE_ITEMS::kScenerySmall
            | GameActions::CLEARABLE_ITEMS::kSceneryLarge;
        auto action = GameActions::ClearAction(MapRange(ax, ay, bx, by), items);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            _lastTileDescription.clear();
            ScreenReaderSpeak("Scenery cleared");
        }
        // Failures are spoken automatically via the error window.
    }

    // Raises or lowers the whole brush area by one step, keeping tiles flat (full-tile mode).
    // One-line description of a single tile element for the Z-axis scan: its kind, and a name
    // for paths, rides and scenery. Mirrors the per-element classification in GetTileDescription.
    static std::string DescribeScanElement(TileElement* el)
    {
        if (auto* track = el->asTrack(); track != nullptr)
        {
            auto* ride = GetRide(track->GetRideIndex());
            return ride != nullptr ? std::string(ride->getName()) : std::string("Ride track");
        }
        if (auto* entrance = el->asEntrance(); entrance != nullptr)
        {
            switch (entrance->GetEntranceType())
            {
                case ENTRANCE_TYPE_PARK_ENTRANCE:
                    return "Park entrance";
                case ENTRANCE_TYPE_RIDE_ENTRANCE:
                    return "Ride entrance";
                case ENTRANCE_TYPE_RIDE_EXIT:
                    return "Ride exit";
            }
            return "Entrance";
        }
        if (auto* p = el->asPath(); p != nullptr)
        {
            std::string name = p->HasLegacyPathEntry() ? GetObjectName(ObjectType::paths, p->GetLegacyPathEntryIndex())
                                                        : GetObjectName(ObjectType::footpathSurface, p->GetSurfaceEntryIndex());
            if (p->IsQueue())
                return name.empty() ? "Queue line" : name + " queue";
            return name.empty() ? "Path" : name;
        }
        if (auto* w = el->asWall(); w != nullptr)
        {
            std::string name = GetObjectName(ObjectType::walls, w->GetEntryIndex());
            return name.empty() ? "Fence" : name;
        }
        if (auto* ss = el->asSmallScenery(); ss != nullptr)
        {
            std::string name = GetObjectName(ObjectType::smallScenery, ss->GetEntryIndex());
            return name.empty() ? "Scenery" : name;
        }
        if (auto* ls = el->asLargeScenery(); ls != nullptr)
        {
            std::string name = GetObjectName(ObjectType::largeScenery, ls->GetEntryIndex());
            return name.empty() ? "Scenery" : name;
        }
        if (auto* surface = el->asSurface(); surface != nullptr)
            return surface->GetWaterHeight() > 0 ? "Water surface" : "Ground";
        return "Object";
    }

    // Read-only vertical scan: steps the probe to the next tile element above (dir > 0) or below
    // (dir < 0) the current scan height on the cursor's tile, reporting its type and height. Plays
    // the elevation tone at that height so the player can feel where it sits.
    static void ScanZLevel(int32_t dir)
    {
        TileElement* best = nullptr;
        int32_t bestHeight = (dir > 0) ? std::numeric_limits<int32_t>::max() : std::numeric_limits<int32_t>::min();
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            const int32_t h = el->baseHeight;
            if (dir > 0 && h > _scanHeight && h < bestHeight)
            {
                bestHeight = h;
                best = el;
            }
            else if (dir < 0 && h < _scanHeight && h > bestHeight)
            {
                bestHeight = h;
                best = el;
            }
            if (el->isLastForTile())
                break;
            el++;
        }

        if (best == nullptr)
        {
            ScreenReaderSpeak(dir > 0 ? "Nothing above" : "Nothing below");
            return;
        }

        _scanHeight = bestHeight;
        PlayElevationTone(bestHeight / 2);
        ScreenReaderSpeak(DescribeScanElement(best) + ", height " + std::to_string(bestHeight / 2));
    }

    static void ChangeLandHeight(bool raise)
    {
        int32_t ax, ay, bx, by;
        GetBrushBounds(ax, ay, bx, by);
        const int32_t centreX = (ax + bx) / 2 + 16;
        const int32_t centreY = (ay + by) / 2 + 16;

        // Re-read the cursor tile AFTER the change applies so the player hears the new height
        // (and any water<->land change) without moving off the tile and back, plus the elevation
        // tone at the new height. The land action is applied a tick later, so this must run in the
        // game-action callback - reading immediately after Execute would report the old height.
        const auto announceAfterChange = [](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // failures are spoken automatically via the error window

            // Report the new elevation (and tone) every press, but only name the tile type when
            // it actually changes, e.g. water <-> land - not "Water"/"Empty" on every press.
            const std::string tileType = GetTileDescription(_cursor);
            const bool typeChanged = (tileType != _lastTileDescription);
            _lastTileDescription = tileType; // keep the move-time change baseline in sync

            std::string spoken;
            if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
            {
                const int32_t elevation = surface->baseHeight / 2;
                PlayElevationTone(elevation);
                _lastElevation = elevation;
                _scanHeight = surface->baseHeight; // keep the Z-axis probe at the new ground level
                spoken = "elevation " + std::to_string(elevation);
            }
            if (typeChanged)
                spoken = spoken.empty() ? tileType : (tileType + ", " + spoken);
            if (!spoken.empty())
                ScreenReaderSpeak(spoken);
        };

        if (raise)
        {
            auto action = GameActions::LandRaiseAction({ centreX, centreY }, { ax, ay, bx, by }, MapSelectType::full);
            action.SetCallback(announceAfterChange);
            GameActions::Execute(&action, getGameState());
        }
        else
        {
            auto action = GameActions::LandLowerAction({ centreX, centreY }, { ax, ay, bx, by }, MapSelectType::full);
            action.SetCallback(announceAfterChange);
            GameActions::Execute(&action, getGameState());
        }
    }

    static void ChangeWaterHeight(bool raise)
    {
        int32_t ax, ay, bx, by;
        GetBrushBounds(ax, ay, bx, by);

        // Like land, the water action applies a tick later, so report the new level from the
        // game-action callback rather than immediately after Execute.
        const auto announceAfterChange = [](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // failures are spoken automatically via the error window

            auto* surface = MapGetSurfaceElementAt(_cursor);
            if (surface == nullptr)
                return;

            const int32_t waterHeight = surface->GetWaterHeight();
            if (waterHeight > 0)
            {
                // The water surface sits one elevation step above the land it covers (water
                // replaces the tile rather than stacking on it), so drop one step to report the
                // water on the same scale the land would read at that spot.
                const int32_t level = std::max(0, waterHeight / kWaterHeightStep - 1);
                PlayElevationTone(level);
                ScreenReaderSpeak("Water level " + std::to_string(level));
            }
            else
            {
                ScreenReaderSpeak("No water");
            }
        };

        if (raise)
        {
            auto action = GameActions::WaterRaiseAction({ ax, ay, bx, by });
            action.SetCallback(announceAfterChange);
            GameActions::Execute(&action, getGameState());
        }
        else
        {
            auto action = GameActions::WaterLowerAction({ ax, ay, bx, by });
            action.SetCallback(announceAfterChange);
            GameActions::Execute(&action, getGameState());
        }
    }

    // Cycles the clear/terraform brush through 1x1, 3x3, 5x5, 7x7 and announces the new size.
    static void CycleBrushSize()
    {
        _brushSize += 2;
        if (_brushSize > 7)
            _brushSize = 1;
        ScreenReaderSpeak(std::to_string(_brushSize) + " by " + std::to_string(_brushSize) + " brush");
    }

    static void RemovePath()
    {
        PathElement* pathElement = nullptr;
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (auto* p = el->asPath(); p != nullptr)
            {
                pathElement = p;
                break;
            }
            if (el->isLastForTile())
                break;
            el++;
        }

        if (pathElement == nullptr)
        {
            ScreenReaderSpeak("No path here");
            return;
        }

        const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
        const CoordsXYZ loc{ world.x, world.y, pathElement->getBaseZ() };
        auto action = GameActions::FootpathRemoveAction(loc);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            _lastTileDescription.clear();
            ScreenReaderSpeak("Path removed");
        }
    }

    static void GoToEntrance()
    {
        const auto& entrances = getGameState().park.entrances;
        if (entrances.empty())
        {
            ScreenReaderSpeak("No park entrance");
            return;
        }

        if (!_initialised)
            InitialiseCursor();

        const auto& entrance = entrances[0];
        _cursor = TileCoordsXY{ entrance.x / kCoordsXYStep, entrance.y / kCoordsXYStep };
        _menuMode = false;
        CentreViewportOnCursor();

        _lastTileDescription = GetTileDescription(_cursor);
        const int32_t x = _cursor.x - _origin.x;
        const int32_t y = _cursor.y - _origin.y;
        ScreenReaderSpeak("Park entrance, X " + std::to_string(x) + ", Y " + std::to_string(y));
    }

    static void ReportFacing()
    {
        static constexpr const char* kDirections[] = { "North", "East", "South", "West" };
        const uint8_t rotation = GetCurrentRotation() & 3;
        ScreenReaderSpeak(std::string("Facing ") + kDirections[rotation]);
    }

    static bool HandleMapCursorKey(uint32_t key, uint32_t modifiers)
    {
        // During pre-built ride placement, dedicated keys rotate / build / cancel the design.
        // Arrow keys and the rest fall through so the map cursor still positions the ride.
        if (Windows::WindowTrackPlaceIsActive())
        {
            switch (key)
            {
                case SDLK_r:
                    Windows::WindowTrackPlaceRotate();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    if (!_initialised)
                        InitialiseCursor();
                    const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    Windows::WindowTrackPlaceAtTile(CoordsXY{ world.x, world.y });
                    return true;
                }
                case SDLK_ESCAPE:
                    Windows::WindowTrackPlaceCancel();
                    return true;
            }
        }

        // Shop/stall placement uses the same scheme: cursor positions the footprint, R rotates,
        // Enter builds, Escape cancels. Arrows fall through to move the cursor.
        if (IsAccessibleRidePlacementActive())
        {
            switch (key)
            {
                case SDLK_r:
                    AccessibleRidePlacementRotate();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    if (!_initialised)
                        InitialiseCursor();
                    const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    AccessibleRidePlacementAtTile(CoordsXY{ world.x, world.y });
                    return true;
                }
                case SDLK_ESCAPE:
                    AccessibleRidePlacementCancel();
                    return true;
            }
        }

        // Scenery placement: cursor positions the object, R rotates (small/large), Enter places,
        // Escape finishes. Shift+W/A/S/D picks the tile edge (walls/banners) and Shift+Q/E/Z/C picks
        // the corner (small scenery), as the player sees the tile - top/bottom/left/right. Plain
        // arrows fall through to move the cursor.
        if (IsAccessibleSceneryPlacementActive())
        {
            if (modifiers & KMOD_SHIFT)
            {
                switch (key)
                {
                    case SDLK_w:
                        AccessibleSceneryPlacementSetEdge(0, "Top edge");
                        return true;
                    case SDLK_d:
                        AccessibleSceneryPlacementSetEdge(1, "Right edge");
                        return true;
                    case SDLK_s:
                        AccessibleSceneryPlacementSetEdge(2, "Bottom edge");
                        return true;
                    case SDLK_a:
                        AccessibleSceneryPlacementSetEdge(3, "Left edge");
                        return true;
                    case SDLK_q:
                        AccessibleSceneryPlacementSetCorner(0, "Top left corner");
                        return true;
                    case SDLK_e:
                        AccessibleSceneryPlacementSetCorner(1, "Top right corner");
                        return true;
                    case SDLK_c:
                        AccessibleSceneryPlacementSetCorner(2, "Bottom right corner");
                        return true;
                    case SDLK_z:
                        AccessibleSceneryPlacementSetCorner(3, "Bottom left corner");
                        return true;
                }
            }
            switch (key)
            {
                case SDLK_r:
                    AccessibleSceneryPlacementRotate();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    if (!_initialised)
                        InitialiseCursor();
                    const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    AccessibleSceneryPlacementAtTile(CoordsXY{ world.x, world.y });
                    return true;
                }
                case SDLK_ESCAPE:
                    AccessibleSceneryPlacementCancel();
                    return true;
            }
        }

        if (key == SDLK_TAB)
            return EnterMenuMode();

        if (key != SDLK_UP && key != SDLK_DOWN && key != SDLK_LEFT && key != SDLK_RIGHT && key != SDLK_c
            && key != SDLK_t && key != SDLK_m && key != SDLK_SPACE && key != SDLK_d && key != SDLK_e
            && key != SDLK_f && key != SDLK_LEFTBRACKET && key != SDLK_RIGHTBRACKET && key != SDLK_p
            && key != SDLK_q && key != SDLK_x && key != SDLK_b && key != SDLK_PAGEUP && key != SDLK_PAGEDOWN)
            return false;

        if (!_initialised)
            InitialiseCursor();

        switch (key)
        {
            case SDLK_c:
                ReadCoordinates();
                break;
            case SDLK_t:
                AnnounceDateTime();
                break;
            case SDLK_m:
                AnnounceMoney();
                break;
            case SDLK_SPACE:
                BuildPath();
                break;
            case SDLK_d:
                RemovePath();
                break;
            case SDLK_p:
                CyclePathType(1);
                break;
            case SDLK_q:
                ToggleQueueMode();
                break;
            case SDLK_x:
                ClearSceneryAtCursor();
                break;
            case SDLK_PAGEUP:
                // Shift raises land, Ctrl raises water, no modifier scans up the Z axis.
                if (modifiers & KMOD_SHIFT)
                    ChangeLandHeight(true);
                else if (modifiers & KMOD_CTRL)
                    ChangeWaterHeight(true);
                else
                    ScanZLevel(1);
                break;
            case SDLK_PAGEDOWN:
                if (modifiers & KMOD_SHIFT)
                    ChangeLandHeight(false);
                else if (modifiers & KMOD_CTRL)
                    ChangeWaterHeight(false);
                else
                    ScanZLevel(-1);
                break;
            case SDLK_b:
                CycleBrushSize();
                break;
            case SDLK_e:
                GoToEntrance();
                break;
            case SDLK_f:
                ReportFacing();
                break;
            case SDLK_LEFTBRACKET:
                CycleAnnouncementHistory(-1);
                break;
            case SDLK_RIGHTBRACKET:
                CycleAnnouncementHistory(1);
                break;
            case SDLK_UP:
                Move(0, 1, "North");
                break;
            case SDLK_DOWN:
                Move(0, -1, "South");
                break;
            case SDLK_RIGHT:
                Move(1, 0, "East");
                break;
            case SDLK_LEFT:
                Move(-1, 0, "West");
                break;
        }
        return true;
    }

    bool HandleMapNavigationKey(const InputEvent& e)
    {
        if (e.deviceKind != InputDeviceKind::keyboard)
            return false;

        // Only active during normal gameplay. Reset so a freshly loaded park rescans.
        if (gLegacyScene != LegacyScene::playing)
        {
            _initialised = false;
            _menuMode = false;
            return false;
        }

        const uint32_t key = e.button;

        // Act on key-down (incl. key-repeat); swallow the matching key-up.
        if (e.state != InputEventState::down)
        {
            if (key == _lastHandledKey)
            {
                _lastHandledKey = 0;
                return true;
            }
            return false;
        }

        // Ctrl+Space toggles between keyboard-cursor mode and free-mouse mode. Works in either
        // mode so the player can always switch back.
        if (key == SDLK_SPACE && (e.modifiers & KMOD_CTRL))
        {
            _mouseMode = !_mouseMode;
            _menuMode = false; // leave any open toolbar menu when switching
            ScreenReaderSpeak(_mouseMode ? "Mouse mode" : "Keyboard mode");
            _lastHandledKey = key;
            return true;
        }

        // In mouse mode the keyboard cursor is inactive: let keys fall through to the game so
        // the mouse can drive everything normally. (Open windows are still navigable via the
        // separate menu-navigation handler.)
        if (_mouseMode)
        {
            _lastHandledKey = 0;
            return false;
        }

        // Cursor-driven ride placement owns the keyboard; make sure a toolbar menu left open
        // from selecting the ride doesn't intercept the arrows used to position it.
        if (_menuMode
            && (Windows::WindowTrackPlaceIsActive() || IsAccessibleRidePlacementActive()
                || IsAccessibleSceneryPlacementActive()))
            _menuMode = false;

        const bool handled = _menuMode ? HandleMenuModeKey(key) : HandleMapCursorKey(key, e.modifiers);
        _lastHandledKey = handled ? key : 0;
        return handled;
    }

    void GoToRide(RideId rideId)
    {
        auto* ride = GetRide(rideId);
        const std::string name = ride != nullptr ? std::string(ride->getName()) : std::string("Ride");

        TileCoordsXY mn, mx;
        if (!ComputeRideBounds(rideId, mn, mx))
        {
            ScreenReaderSpeak(name + ", location unknown");
            return;
        }

        if (!_initialised)
            InitialiseCursor();

        _cursor = mn; // bottom-left corner of the ride's footprint
        _menuMode = false; // hand control back to the map cursor
        CentreViewportOnCursor();

        const int32_t w = mx.x - mn.x + 1;
        const int32_t h = mx.y - mn.y + 1;
        _lastTileDescription = name + ", " + std::to_string(w) + " by " + std::to_string(h);
        ScreenReaderSpeak(_lastTileDescription);
    }

    std::string GetRideLocationText(RideId rideId)
    {
        TileCoordsXY mn, mx;
        if (!ComputeRideBounds(rideId, mn, mx))
            return {};

        if (!_initialised)
            InitialiseCursor();

        const int32_t x = mn.x - _origin.x;
        const int32_t y = mn.y - _origin.y;
        return "X " + std::to_string(x) + ", Y " + std::to_string(y);
    }

    void ReannounceToolbarItemIfMenuMode()
    {
        if (!_menuMode)
            return;
        if (auto* toolbar = GetToolbar(); toolbar != nullptr)
            toolbar->onAccessibilityAction(AccessibilityAction::announce);
    }

    bool IsMapCursorActive()
    {
        return gLegacyScene == LegacyScene::playing && !_menuMode && !_mouseMode;
    }

    std::string DescribeTile(const TileCoordsXY& tile)
    {
        return GetTileDescription(tile);
    }

    void UpdateMapCursorFromMouse()
    {
        // The mouse is fully independent of the keyboard cursor. In either mode, moving it
        // reads aloud whatever tile it is over - but it never moves the keyboard cursor (_cursor)
        // or the camera. Camera scrolling for the mouse is the engine's normal edge-scrolling,
        // which is only enabled in mouse mode.
        if (gLegacyScene != LegacyScene::playing)
        {
            _mouseTracking = false;
            return;
        }

        const auto pos = ContextGetCursorPosition();

        // Seed the position on the first frame so the mouse's initial resting place is not
        // treated as a deliberate move (which would auto-announce a tile on load).
        if (!_mouseTracking)
        {
            _lastMousePos = pos;
            _mouseTracking = true;
            return;
        }

        // React only to genuine mouse movement.
        if (pos.x == _lastMousePos.x && pos.y == _lastMousePos.y)
            return;
        _lastMousePos = pos;

        Viewport* vp = nullptr;
        const auto mapCoords = ScreenGetMapXY(pos, &vp);
        if (!mapCoords.has_value())
            return; // pointer is not over the map (e.g. hovering a window)

        const auto mapSize = getGameState().mapSize;
        const TileCoordsXY tile{ mapCoords->x / kCoordsXYStep, mapCoords->y / kCoordsXYStep };
        // Stay within the usable area, matching Move().
        if (tile.x < 1 || tile.y < 1 || tile.x > mapSize.x - 2 || tile.y > mapSize.y - 2)
            return;

        // Read the hovered tile on change, tracked separately from the keyboard cursor.
        std::string description = GetTileDescription(tile);
        if (description != _lastHoverDescription)
        {
            ScreenReaderSpeak(description);
            _lastHoverDescription = std::move(description);
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
