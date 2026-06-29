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
#include <optional>
#include <openrct2-ui/UiStringIds.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathPlaceAction.h>
#include <openrct2/actions/footpath/FootpathRemoveAction.h>
#include <openrct2/actions/terraform/ClearAction.h>
#include <openrct2/actions/park/LandBuyRightsAction.h>
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
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/config/Config.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/interface/WindowClasses.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.Date.h>
#include <openrct2/management/Finance.h>
#include <openrct2/object/FootpathEntry.h>
#include <openrct2/object/FootpathSurfaceObject.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectLimits.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectTypes.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideConstruction.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/TrackIteration.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/tile_element/BannerElement.h>
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
    // Cursor state, in absolute map tile coordinates.
    static bool _initialised = false;
    static TileCoordsXY _cursor{};

    // Spoken coordinates are ABSOLUTE: a tile always reports the same X/Y no matter how the camera
    // is rotated. The orientation is fixed to the default view (where screen-right is higher X and
    // screen-up is higher Y). The arrow keys rotate with the camera (see MoveScreen) so they stay
    // consistent with these fixed axes: after rotating East, Up moves the way Right did at North and
    // therefore raises X. The tile's coordinates themselves never move.
    static int32_t SpokenCoordX(const TileCoordsXY& t)
    {
        return (getGameState().mapSize.x - 2) - t.x;
    }
    static int32_t SpokenCoordY(const TileCoordsXY& t)
    {
        return (getGameState().mapSize.y - 2) - t.y;
    }

    // Exported so other features (e.g. ride construction) can speak a tile's position with the exact
    // same absolute X/Y convention the map cursor uses, with no change to how coordinates read.
    std::string SpokenTileCoordsText(const TileCoordsXY& tile)
    {
        return "X " + std::to_string(SpokenCoordX(tile)) + ", Y " + std::to_string(SpokenCoordY(tile));
    }

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

    // Marked terraform area: two markers define an arbitrary rectangle the land/water raise and
    // lower commands act on instead of the cursor brush. The 'k' key cycles through setting the
    // first corner, the opposite corner, then clearing. _markerCount is 0 (none), 1 (one corner
    // set), or 2 (rectangle active). Either corner may be marked first; the rectangle uses the
    // min/max of the two so corner order does not matter.
    static TileCoordsXY _markerA{};
    static TileCoordsXY _markerB{};
    static int32_t _markerCount = 0;

    // Which marker Shift+K jumps to next: 0 = first corner, 1 = second corner. Toggles on each
    // press so the cursor snaps back and forth between the two markers.
    static int32_t _snapTarget = 0;

    // Numbered waypoints: bookmarks on the map. Shift + number (1-9) drops or moves the waypoint in
    // that slot at the cursor; pressing the plain number warps the cursor to it. Session-only - they
    // are cleared on park load, like the terraform markers.
    static constexpr int32_t kWaypointCount = 9;
    static TileCoordsXY _waypoints[kWaypointCount]{};
    static bool _waypointSet[kWaypointCount] = {};

    // The world direction (0..3) of the cursor's most recent arrow move. Used as the "facing" for
    // building sloped paths: a ramp rises (or falls) toward this direction.
    static Direction _lastMoveDir = 0;

    // Footpath slope-build mode, cycled with the L key. In Up/Down, pressing the build-path key
    // lays a ramp rising/falling toward the facing direction (connecting to the path behind the
    // cursor), so the player can build a staircase up to an elevated ride entrance.
    enum class SlopeMode
    {
        flat,
        up,
        down,
    };
    static SlopeMode _slopeMode = SlopeMode::flat;

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

    // The localised name of a loaded object, or an empty string if not found.
    static std::string GetObjectName(ObjectType type, ObjectEntryIndex index)
    {
        auto& objManager = GetContext()->GetObjectManager();
        auto* obj = objManager.GetLoadedObject(type, index);
        return obj != nullptr ? std::string(obj->GetName()) : std::string();
    }

    // Builds the spoken description of a sign-capable element (banner, wall, or large scenery). If
    // the element carries a banner with custom text, it is appended so the player hears what the
    // sign actually says, e.g. "Sign, reading Main Street". Falls back to just the base label.
    static std::string DescribeSign(const std::string& base, const Banner* banner)
    {
        if (banner == nullptr)
            return base;
        std::string text = banner->getText();
        if (text.empty())
            return base;
        return base + ", reading " + text;
    }

    static std::string DescribeTrackPiece(const OpenRCT2::TrackMetadata::TrackElementDescriptor& ted);

    // Describes everything on a tile, read from the top down so a blind player learns what is
    // stacked on it (e.g. a ride bridging over a path, or a bench on a path), not just the topmost
    // feature. Each element becomes one comma-separated part; the park-ownership status ("outside
    // park") is appended last, since it describes the land beneath everything. "Outside park" means
    // the tile is outside the owned/buildable area; buying the land makes it read as inside.
    static std::string GetTileDescription(const TileCoordsXY& tile)
    {
        // Collected bottom-to-top (the order tile elements are stored in), then reversed so the
        // topmost feature is announced first.
        std::vector<std::string> parts;

        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            // Read placed track pieces (shape, slope, bank) with no ride name; skip the construction
            // preview ghost so the not-yet-built next piece never reads here.
            if (auto* track = el->asTrack(); track != nullptr && !el->isGhost())
            {
                std::string piece
                    = DescribeTrackPiece(OpenRCT2::TrackMetadata::GetTrackElementDescriptor(track->GetTrackType()));
                if (!piece.empty())
                    parts.push_back(piece);
            }
            else if (auto* entrance = el->asEntrance(); entrance != nullptr)
            {
                switch (entrance->GetEntranceType())
                {
                    case ENTRANCE_TYPE_PARK_ENTRANCE:
                        parts.push_back("Park entrance");
                        break;
                    case ENTRANCE_TYPE_RIDE_ENTRANCE:
                        parts.push_back("Ride entrance");
                        break;
                    case ENTRANCE_TYPE_RIDE_EXIT:
                        parts.push_back("Ride exit");
                        break;
                }
            }
            else if (auto* p = el->asPath(); p != nullptr)
            {
                std::string name;
                if (p->HasLegacyPathEntry())
                    name = GetObjectName(ObjectType::paths, p->GetLegacyPathEntryIndex());
                else
                    name = GetObjectName(ObjectType::footpathSurface, p->GetSurfaceEntryIndex());

                if (p->IsQueue())
                {
                    if (name.empty())
                    {
                        parts.push_back("Queue line");
                    }
                    else
                    {
                        // Queue surfaces are usually named just by colour, so append "queue"
                        // unless the name already mentions it.
                        std::string lower = name;
                        for (auto& c : lower)
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        parts.push_back((lower.find("queue") != std::string::npos) ? name : name + " queue");
                    }
                }
                else
                {
                    parts.push_back(name.empty() ? "Path" : name);
                }

                // Path additions are the benches, litter bins, lamps and fountains placed on a
                // path. They live on the same element as the path; announce them as their own part
                // so a tile reads e.g. "Bench, Tarmac path".
                if (p->HasAddition())
                {
                    std::string addition = GetObjectName(ObjectType::pathAdditions, p->GetAdditionEntryIndex());
                    parts.push_back(addition.empty() ? "Path addition" : addition);
                }
            }
            else if (auto* b = el->asBanner(); b != nullptr)
            {
                // Banners are the signs placed on path edges to name areas or give directions; the
                // player can type custom text on them. Read that text so it is not lost.
                parts.push_back(DescribeSign("Sign", b->GetBanner()));
            }
            else if (auto* w = el->asWall(); w != nullptr)
            {
                std::string name = GetObjectName(ObjectType::walls, w->GetEntryIndex());
                // A wall can itself be a sign carrying custom text (e.g. a wall-mounted sign).
                parts.push_back(DescribeSign(name.empty() ? "Fence" : name, w->GetBanner()));
            }
            else if (auto* ss = el->asSmallScenery(); ss != nullptr)
            {
                std::string name = GetObjectName(ObjectType::smallScenery, ss->GetEntryIndex());
                parts.push_back(name.empty() ? "Scenery" : name);
            }
            else if (auto* ls = el->asLargeScenery(); ls != nullptr)
            {
                std::string name = GetObjectName(ObjectType::largeScenery, ls->GetEntryIndex());
                // Large scenery with a banner is a sign (the big stand-alone signs); read its text.
                parts.push_back(DescribeSign(name.empty() ? "Scenery" : name, ls->GetBanner()));
            }

            if (el->isLastForTile())
                break;
            el++;
        }

        // Tile elements were gathered bottom-to-top; flip so we read the topmost feature first.
        std::reverse(parts.begin(), parts.end());

        auto* surface = MapGetSurfaceElementAt(tile);
        // Water is the surface itself, so it sits beneath any structures: announce it last.
        if (surface != nullptr && surface->GetWaterHeight() > 0)
            parts.push_back("Water");

        const bool owned = surface != nullptr && (surface->GetOwnership() & OWNERSHIP_OWNED) != 0;

        if (parts.empty())
            return owned ? "Empty" : "Outside park";

        std::string result;
        for (size_t i = 0; i < parts.size(); i++)
        {
            if (i != 0)
                result += ", ";
            result += parts[i];
        }
        // The land beneath everything: note when the tile is outside the owned park area.
        if (!owned)
            result += ", outside park";
        return result;
    }

    // Picks a starting tile for the cursor (first owned tile, else map centre).
    static void InitialiseCursor()
    {
        _initialised = true;
        _lastTileDescription.clear();
        _lastElevation = -1;

        const auto mapSize = getGameState().mapSize;

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

    std::optional<ScreenCoordsXY> GetMapCursorScreenPos()
    {
        if (!_initialised)
            return std::nullopt;
        auto* w = WindowGetMain();
        if (w == nullptr || w->viewport == nullptr)
            return std::nullopt;

        // Centre the view on the cursor so the cursor tile is at the viewport centre, then return
        // that centre point. A screen-to-map lookup there resolves back to the cursor tile.
        CentreViewportOnCursor();
        const auto* vp = w->viewport;
        return ScreenCoordsXY{ vp->pos.x + vp->width / 2, vp->pos.y + vp->height / 2 };
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

    // Returns a spoken label if the given tile holds a terraform-area marker, otherwise nullptr.
    // Used to call out markers as the cursor arrows over them so the player can feel the rectangle.
    static const char* MarkerLabelAt(const TileCoordsXY& t)
    {
        const bool isA = _markerCount >= 1 && t.x == _markerA.x && t.y == _markerA.y;
        const bool isB = _markerCount == 2 && t.x == _markerB.x && t.y == _markerB.y;
        if (isA && isB)
            return "First and second marker";
        if (isA)
            return "First marker";
        if (isB)
            return "Second marker";
        return nullptr;
    }

    // Human-readable slope/bank for a track piece, in the same terms the construction menu uses.
    static const char* PitchName(OpenRCT2::TrackMetadata::TrackPitch p)
    {
        using OpenRCT2::TrackMetadata::TrackPitch;
        switch (p)
        {
            case TrackPitch::up25:
                return "gentle up";
            case TrackPitch::up60:
                return "steep up";
            case TrackPitch::up90:
                return "vertical up";
            case TrackPitch::down25:
                return "gentle down";
            case TrackPitch::down60:
                return "steep down";
            case TrackPitch::down90:
                return "vertical down";
            default:
                return "level";
        }
    }
    static const char* RollName(OpenRCT2::TrackMetadata::TrackRoll r)
    {
        using OpenRCT2::TrackMetadata::TrackRoll;
        switch (r)
        {
            case TrackRoll::left:
                return "banked left";
            case TrackRoll::right:
                return "banked right";
            default:
                return "no bank";
        }
    }

    // Names a basic piece's shape from its geometry, for the many pieces that carry no in-game
    // description (straights, slopes, plain curves). The turn comes from the rotation change across
    // the piece; the curve sharpness from its track group. Special pieces keep their in-game name.
    static std::string DeriveBasicShape(const OpenRCT2::TrackMetadata::TrackElementDescriptor& ted)
    {
        const int32_t turn = (ted.coordinates.rotationEnd - ted.coordinates.rotationBegin) & 3;
        if (turn == 0)
            return "straight";
        if (turn == 2)
            return "half turn";

        const char* dir = (turn == 1) ? "right" : "left";
        const char* sharp = "";
        switch (ted.definition.group)
        {
            case OpenRCT2::TrackGroup::curveVerySmall:
                sharp = "very small ";
                break;
            case OpenRCT2::TrackGroup::curveSmall:
                sharp = "small ";
                break;
            case OpenRCT2::TrackGroup::curveLarge:
                sharp = "large ";
                break;
            default:
                break;
        }
        return std::string(dir) + " " + sharp + "curve";
    }

    // Builds the full spoken description of one track piece: its shape (the game's piece name, or a
    // derived name for basic pieces), its slope (level / gentle up / steep down, or a transition
    // like "level to gentle up"), and its banking when banked - all of the piece's attributes.
    static std::string DescribeTrackPiece(const OpenRCT2::TrackMetadata::TrackElementDescriptor& ted)
    {
        std::string s = OpenRCT2::FormatStringID(ted.description);
        if (s.empty())
            s = DeriveBasicShape(ted);

        const auto& def = ted.definition;
        std::string slope = (def.pitchStart == def.pitchEnd)
            ? PitchName(def.pitchEnd)
            : std::string(PitchName(def.pitchStart)) + " to " + PitchName(def.pitchEnd);
        s += s.empty() ? slope : (", " + slope);

        using OpenRCT2::TrackMetadata::TrackRoll;
        if (def.rollStart != TrackRoll::none || def.rollEnd != TrackRoll::none)
        {
            std::string bank = (def.rollStart == def.rollEnd)
                ? RollName(def.rollEnd)
                : std::string(RollName(def.rollStart)) + " to " + RollName(def.rollEnd);
            s += ", " + bank;
        }
        return s;
    }

    // If the tile holds placed ride track, returns the piece description(s) on it (shape, slope,
    // bank). The ride name is deliberately not included - just the tile's pieces. Empty when the
    // tile has no track, or only the construction preview ghost.
    static std::string GetTrackReadout(const TileCoordsXY& tile)
    {
        std::string pieces;
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            // Skip the construction preview ghost - only read track that has actually been placed.
            if (auto* track = el->asTrack(); track != nullptr && !el->isGhost())
            {
                const auto& ted = OpenRCT2::TrackMetadata::GetTrackElementDescriptor(track->GetTrackType());
                std::string piece = DescribeTrackPiece(ted);
                if (!piece.empty())
                {
                    if (!pieces.empty())
                        pieces += ", ";
                    pieces += piece;
                }
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return pieces;
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

        const bool wasOwned = IsTileOwned(_cursor); // ownership of the tile we are leaving
        _cursor = target;
        CentreViewportOnCursor();

        // Remember which world direction we just moved, as the "facing" for sloped-path building.
        if (dx > 0)
            _lastMoveDir = 2;
        else if (dx < 0)
            _lastMoveDir = 0;
        else if (dy > 0)
            _lastMoveDir = 1;
        else if (dy < 0)
            _lastMoveDir = 3;

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

        // Announce crossing the park boundary in either direction. The tile description (below)
        // already labels unowned land "Outside park", but this gives an explicit, symmetric cue
        // for both leaving and re-entering, regardless of what is on the tile.
        const bool owned = IsTileOwned(_cursor);
        bool announcedCrossing = false;
        if (owned != wasOwned)
        {
            ScreenReaderSpeak(owned ? "Entered the park" : "Left the park");
            announcedCrossing = true;
        }

        // On a ride's track, announce the piece on EVERY move (no change suppression) so the player
        // can trace a layout tile by tile - each tile reads its piece name. Off the track, announce
        // the tile only when its description changes from the previous tile. If we just announced a
        // boundary crossing, queue the read (interrupt = false) so both are heard, but skip the
        // bare-ground labels ("Empty"/"Outside park") since the crossing already said it.
        std::string description = GetTileDescription(_cursor);
        if (std::string track = GetTrackReadout(_cursor); !track.empty())
        {
            ScreenReaderSpeak(track, !announcedCrossing);
            _lastTileDescription = std::move(description); // keep baseline coherent for leaving the track
        }
        else if (description != _lastTileDescription)
        {
            const bool bareGround = (description == "Empty" || description == "Outside park");
            if (!(announcedCrossing && bareGround))
                ScreenReaderSpeak(description, !announcedCrossing);
            _lastTileDescription = std::move(description);
        }

        // Call out a terraform-area marker on the new tile. Queued (interrupt = false) so it is
        // heard after any tile description or boundary cue rather than cutting them off.
        if (const char* marker = MarkerLabelAt(_cursor); marker != nullptr)
            ScreenReaderSpeak(marker, false);
    }

    // Moves the cursor in a SCREEN-relative direction. The (dx, dy) are the world deltas for that
    // direction at the default view rotation; we rotate them by the current camera rotation so the
    // arrow keys rotate together with the view. This keeps them consistent with the absolute spoken
    // coordinates at every rotation: e.g. after rotating East, Up moves the way Right did at North,
    // so it raises X (see SpokenCoordX/Y). The rotation is the 90-degree step (x, y) -> (y, -x),
    // applied `rotation` times (matching how the camera rotates the world onto the screen).
    static void MoveScreen(int32_t dx, int32_t dy, const char* directionName)
    {
        const int32_t steps = GetCurrentRotation() & 3;
        for (int32_t i = 0; i < steps; i++)
        {
            const int32_t nx = dy;
            const int32_t ny = -dx;
            dx = nx;
            dy = ny;
        }
        Move(dx, dy, directionName);
    }

    static void ReadCoordinates()
    {
        const int32_t x = SpokenCoordX(_cursor);
        const int32_t y = SpokenCoordY(_cursor);
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

    // Draws the engine's tile-selection highlight on whatever tile the keyboard map cursor is on,
    // so sighted players/helpers can see where focus is. Re-asserted each frame. Skipped while a
    // real tool owns the selection (e.g. placing a pre-built ride); cleared when focus leaves the
    // map for a menu or window (the window focus highlight takes over there).
    void TickFocusHighlight()
    {
        static bool weSetSelection = false;

        const bool wantTile = IsMapCursorActive() && _initialised && !gInputFlags.has(InputFlag::toolActive);
        if (wantTile)
        {
            const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
            // While positioning a ride, highlight the whole footprint the ride would occupy;
            // otherwise highlight just the cursor tile.
            MapRange footprint;
            if (AccessibleRidePlacementFootprintRange(CoordsXY{ world.x, world.y }, footprint))
                setMapSelectRange(footprint);
            else
                setMapSelectRange(CoordsXY{ world.x, world.y });
            gMapSelectType = MapSelectType::full;
            gMapSelectFlags.set(MapSelectFlag::enable);
            MapSelection::invalidate();
            weSetSelection = true;
        }
        else if (weSetSelection && !gInputFlags.has(InputFlag::toolActive))
        {
            gMapSelectFlags.unset(MapSelectFlag::enable);
            MapSelection::invalidate();
            weSetSelection = false;
        }
    }

    void TickMoneyAnnounce()
    {
        if (gLegacyScene != LegacyScene::playing)
            return;

        money64 amount;
        if (!FinanceAccessConsumePending(amount) || amount == 0)
            return;

        // Positive = money spent, negative = money earned (e.g. a demolish refund or land sale).
        const bool spent = amount > 0;
        const money64 magnitude = spent ? amount : -amount;
        std::string money = OpenRCT2::FormatStringID(STR_BOTTOM_TOOLBAR_CASH, magnitude);
        // interrupt = false so this queues after any action confirmation (e.g. "Ride demolished").
        ScreenReaderSpeak((spent ? "Spent " : "Earned ") + money, false);
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

    // Compass name for a world direction, in the same fixed frame as the spoken coordinates and
    // ride-placement facing (0 = -x = East, 1 = +y = South, 2 = +x = West, 3 = -y = North).
    static const char* WorldDirectionName(Direction dir)
    {
        static constexpr const char* kNames[] = { "East", "South", "West", "North" };
        return kNames[dir & 3];
    }

    // Connecting height of the path on the tile behind the cursor (opposite the facing direction
    // `dir`), at the edge shared with the cursor's tile, or nullopt when there is no path to
    // connect to. A ramp that rises toward the cursor is one path-step higher on that edge.
    static std::optional<int32_t> BehindPathEdgeHeight(Direction dir)
    {
        const auto mapSize = getGameState().mapSize;
        const TileCoordsXY behind{ _cursor.x - CoordsDirectionDelta[dir].x / kCoordsXYStep,
                                   _cursor.y - CoordsDirectionDelta[dir].y / kCoordsXYStep };
        if (behind.x < 1 || behind.y < 1 || behind.x > mapSize.x - 2 || behind.y > mapSize.y - 2)
            return std::nullopt;

        std::optional<int32_t> best;
        for (TileElement* el = MapGetFirstElementAt(behind); el != nullptr;)
        {
            if (auto* path = el->asPath(); path != nullptr)
            {
                int32_t edge = path->getBaseZ();
                if (path->IsSloped() && path->GetSlopeDirection() == dir)
                    edge += kPathHeightStep;
                if (!best.has_value() || edge > *best)
                    best = edge;
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return best;
    }

    // Cycles the footpath slope-build mode (flat -> up -> down) and announces it.
    static void CycleSlopeMode()
    {
        switch (_slopeMode)
        {
            case SlopeMode::flat:
                _slopeMode = SlopeMode::up;
                ScreenReaderSpeak("Slope up");
                break;
            case SlopeMode::up:
                _slopeMode = SlopeMode::down;
                ScreenReaderSpeak("Slope down");
                break;
            case SlopeMode::down:
                _slopeMode = SlopeMode::flat;
                ScreenReaderSpeak("Flat paths");
                break;
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
        const Direction dir = _lastMoveDir & 3;
        const auto behindEdge = BehindPathEdgeHeight(dir);

        FootpathSlope slope{};
        int32_t baseZ = 0;
        Direction actionDir = kInvalidDirection;
        std::string what;

        if (_slopeMode == SlopeMode::flat)
        {
            auto placement = FootpathGetOnTerrainPlacement(_cursor);
            // Prefer connecting to a higher path behind the cursor (e.g. a flat landing at the top
            // of a ramp); otherwise follow the terrain as before so paths step up gentle hills.
            if (behindEdge.has_value() && (!placement.isValid() || *behindEdge > placement.baseZ))
            {
                baseZ = *behindEdge;
                slope = { FootpathSlopeType::flat, 0 };
                actionDir = dir;
            }
            else
            {
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
                baseZ = placement.baseZ;
                slope = placement.slope;
            }
            what = gFootpathSelection.isQueueSelected ? "Queue built" : "Path built";
            if (slope.type == FootpathSlopeType::sloped)
                what += ", sloped";
        }
        else
        {
            // Up or down ramp toward the facing direction, starting from the path behind the cursor
            // (or the ground when there is none, to begin a ramp from terrain).
            int32_t fromZ;
            if (behindEdge.has_value())
            {
                fromZ = *behindEdge;
            }
            else
            {
                // No path behind: start the ramp from the ground at the cursor.
                auto placement = FootpathGetOnTerrainPlacement(_cursor);
                if (!placement.isValid())
                {
                    ScreenReaderSpeak("Cannot build a path here");
                    return;
                }
                fromZ = placement.baseZ;
            }

            actionDir = dir;
            if (_slopeMode == SlopeMode::up)
            {
                baseZ = fromZ;
                slope = { FootpathSlopeType::sloped, dir };
                what = std::string("Ramp up to the ") + WorldDirectionName(dir);
            }
            else
            {
                baseZ = fromZ - kPathHeightStep;
                slope = { FootpathSlopeType::sloped, DirectionReverse(dir) };
                what = std::string("Ramp down to the ") + WorldDirectionName(dir);
            }
        }

        const CoordsXYZ loc{ world.x, world.y, baseZ };
        auto action = GameActions::FootpathPlaceAction(
            loc, slope, type, gFootpathSelection.railings, actionDir, flags);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            _lastTileDescription.clear();
            ScreenReaderSpeak(what);
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

    // Returns true when two markers are set, defining an active terraform rectangle.
    static bool HasMarkedArea()
    {
        return _markerCount == 2;
    }

    // The tile rectangle the land/water raise/lower commands act on: the marked rectangle when two
    // markers are set, otherwise the square brush centred on the cursor (GetBrushBounds). The
    // markers may be in any corner order, so the rectangle is built from their min/max.
    static void GetTerraformBounds(int32_t& ax, int32_t& ay, int32_t& bx, int32_t& by)
    {
        if (!HasMarkedArea())
        {
            GetBrushBounds(ax, ay, bx, by);
            return;
        }

        const auto mapSize = getGameState().mapSize;
        const int32_t minTileX = std::clamp(std::min(_markerA.x, _markerB.x), 1, mapSize.x - 2);
        const int32_t minTileY = std::clamp(std::min(_markerA.y, _markerB.y), 1, mapSize.y - 2);
        const int32_t maxTileX = std::clamp(std::max(_markerA.x, _markerB.x), 1, mapSize.x - 2);
        const int32_t maxTileY = std::clamp(std::max(_markerA.y, _markerB.y), 1, mapSize.y - 2);
        ax = minTileX * kCoordsXYStep;
        ay = minTileY * kCoordsXYStep;
        bx = maxTileX * kCoordsXYStep;
        by = maxTileY * kCoordsXYStep;
    }

    // The cursor tile when using the brush, or the centre tile of the marked rectangle. The
    // terraform commands sample this tile after the change to report the new elevation, so when a
    // marked area is active (and the cursor may sit outside it) the read-out reflects the area.
    static TileCoordsXY TerraformSampleTile()
    {
        if (!HasMarkedArea())
            return _cursor;
        return TileCoordsXY{ (_markerA.x + _markerB.x) / 2, (_markerA.y + _markerB.y) / 2 };
    }

    // 'k' cycles the terraform-area markers: first press sets one corner at the cursor, second
    // press sets the opposite corner (activating the rectangle), third press clears them. Once a
    // rectangle is active, Shift/Ctrl + Page Up/Down raise or lower the whole area at once.
    static void CycleAreaMarker()
    {
        _snapTarget = 0; // any change to the markers restarts the Shift+K snap at the first corner
        if (_markerCount == 0)
        {
            _markerA = _cursor;
            _markerCount = 1;
            ScreenReaderSpeak(
                "First marker set at X " + std::to_string(SpokenCoordX(_cursor)) + ", Y "
                + std::to_string(SpokenCoordY(_cursor)));
        }
        else if (_markerCount == 1)
        {
            _markerB = _cursor;
            _markerCount = 2;
            const int32_t w = std::max(_markerA.x, _markerB.x) - std::min(_markerA.x, _markerB.x) + 1;
            const int32_t h = std::max(_markerA.y, _markerB.y) - std::min(_markerA.y, _markerB.y) + 1;
            ScreenReaderSpeak(
                "Second marker set at X " + std::to_string(SpokenCoordX(_cursor)) + ", Y "
                + std::to_string(SpokenCoordY(_cursor)) + ". Marked area " + std::to_string(w) + " by "
                + std::to_string(h) + " tiles");
        }
        else
        {
            _markerCount = 0;
            ScreenReaderSpeak("Markers cleared");
        }
    }

    // Shift+K snaps the cursor to a terraform marker, alternating between the two on each press so
    // the player can hop back and forth between the corners they defined. Centres the view and
    // reads the marker name and coordinates, like jumping to the park entrance.
    static void SnapToMarker()
    {
        if (_markerCount == 0)
        {
            ScreenReaderSpeak("No markers set");
            return;
        }
        if (!_initialised)
            InitialiseCursor();

        TileCoordsXY dest;
        const char* label;
        if (_markerCount == 1 || _snapTarget == 0)
        {
            dest = _markerA;
            label = "First marker";
            _snapTarget = (_markerCount == 2) ? 1 : 0;
        }
        else
        {
            dest = _markerB;
            label = "Second marker";
            _snapTarget = 0;
        }

        _cursor = dest;
        _menuMode = false;
        CentreViewportOnCursor();

        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            const int32_t elevation = surface->baseHeight / 2;
            // Match normal movement: only sound the elevation tone when the height actually changes,
            // so jumping to a marker/waypoint at the same height as the current tile stays silent.
            if (elevation != _lastElevation)
            {
                PlayElevationTone(elevation);
                _lastElevation = elevation;
            }
            _scanHeight = surface->baseHeight;
        }

        _lastTileDescription = GetTileDescription(_cursor);
        ScreenReaderSpeak(
            std::string(label) + ", X " + std::to_string(SpokenCoordX(_cursor)) + ", Y "
            + std::to_string(SpokenCoordY(_cursor)));
    }

    // Drops (or moves) the numbered waypoint in the given slot at the cursor's current tile.
    static void SetWaypoint(int32_t slot)
    {
        if (!_initialised)
            InitialiseCursor();
        _waypoints[slot] = _cursor;
        _waypointSet[slot] = true;
        ScreenReaderSpeak(
            "Waypoint " + std::to_string(slot + 1) + " set at X " + std::to_string(SpokenCoordX(_cursor)) + ", Y "
            + std::to_string(SpokenCoordY(_cursor)));
    }

    // Warps the cursor to the numbered waypoint in the given slot, centring the view and reading the
    // waypoint number, what is on the tile, and its coordinates. Announces if the slot is empty.
    static void JumpToWaypoint(int32_t slot)
    {
        if (!_waypointSet[slot])
        {
            ScreenReaderSpeak("Waypoint " + std::to_string(slot + 1) + " not set");
            return;
        }
        if (!_initialised)
            InitialiseCursor();

        _cursor = _waypoints[slot];
        _menuMode = false;
        CentreViewportOnCursor();

        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            const int32_t elevation = surface->baseHeight / 2;
            // Match normal movement: only sound the elevation tone when the height actually changes,
            // so jumping to a marker/waypoint at the same height as the current tile stays silent.
            if (elevation != _lastElevation)
            {
                PlayElevationTone(elevation);
                _lastElevation = elevation;
            }
            _scanHeight = surface->baseHeight;
        }

        const std::string description = GetTileDescription(_cursor);
        _lastTileDescription = description;
        ScreenReaderSpeak(
            "Waypoint " + std::to_string(slot + 1) + ", " + description + ", X "
            + std::to_string(SpokenCoordX(_cursor)) + ", Y " + std::to_string(SpokenCoordY(_cursor)));
    }

    // Removes small and large scenery (trees, bushes, statues, etc.) across the brush area.
    static void ClearSceneryAtCursor()
    {
        int32_t ax, ay, bx, by;
        GetTerraformBounds(ax, ay, bx, by);

        const GameActions::ClearableItems items = GameActions::CLEARABLE_ITEMS::kScenerySmall
            | GameActions::CLEARABLE_ITEMS::kSceneryLarge;
        auto action = GameActions::ClearAction(MapRange(ax, ay, bx, by), items);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            _lastTileDescription.clear();
            ScreenReaderSpeak(HasMarkedArea() ? "Marked area scenery cleared" : "Scenery cleared");
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
                name = name.empty() ? "Queue line" : name + " queue";
            else if (name.empty())
                name = "Path";
            if (p->HasAddition())
            {
                std::string addition = GetObjectName(ObjectType::pathAdditions, p->GetAdditionEntryIndex());
                name += ", " + (addition.empty() ? std::string("path addition") : addition);
            }
            return name;
        }
        if (auto* b = el->asBanner(); b != nullptr)
        {
            return DescribeSign("Sign", b->GetBanner());
        }
        if (auto* w = el->asWall(); w != nullptr)
        {
            std::string name = GetObjectName(ObjectType::walls, w->GetEntryIndex());
            return DescribeSign(name.empty() ? "Fence" : name, w->GetBanner());
        }
        if (auto* ss = el->asSmallScenery(); ss != nullptr)
        {
            std::string name = GetObjectName(ObjectType::smallScenery, ss->GetEntryIndex());
            return name.empty() ? "Scenery" : name;
        }
        if (auto* ls = el->asLargeScenery(); ls != nullptr)
        {
            std::string name = GetObjectName(ObjectType::largeScenery, ls->GetEntryIndex());
            return DescribeSign(name.empty() ? "Scenery" : name, ls->GetBanner());
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
        GetTerraformBounds(ax, ay, bx, by);
        const int32_t centreX = (ax + bx) / 2 + 16;
        const int32_t centreY = (ay + by) / 2 + 16;

        const bool marked = HasMarkedArea();
        const TileCoordsXY sample = TerraformSampleTile();

        // Re-read a tile AFTER the change applies so the player hears the new height (and any
        // water<->land change) without moving off the tile and back, plus the elevation tone at the
        // new height. The land action is applied a tick later, so this must run in the game-action
        // callback - reading immediately after Execute would report the old height. With a marked
        // area the sample is the rectangle centre (the cursor may sit outside it) and we prefix
        // "Marked area"; in brush mode the sample is the cursor and we keep its move-time baseline.
        const auto announceAfterChange = [marked, sample](
                                             const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // failures are spoken automatically via the error window

            std::string spoken;
            if (auto* surface = MapGetSurfaceElementAt(sample); surface != nullptr)
            {
                const int32_t elevation = surface->baseHeight / 2;
                PlayElevationTone(elevation);
                spoken = "elevation " + std::to_string(elevation);
                if (!marked)
                {
                    _lastElevation = elevation;
                    _scanHeight = surface->baseHeight; // keep the Z-axis probe at the new ground level
                }
            }

            if (marked)
            {
                spoken = spoken.empty() ? "Marked area" : ("Marked area, " + spoken);
            }
            else
            {
                // Only name the tile type when it actually changes, e.g. water <-> land - not
                // "Water"/"Empty" on every press - and keep the move-time change baseline in sync.
                const std::string tileType = GetTileDescription(sample);
                const bool typeChanged = (tileType != _lastTileDescription);
                _lastTileDescription = tileType;
                if (typeChanged)
                    spoken = spoken.empty() ? tileType : (tileType + ", " + spoken);
            }
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
        GetTerraformBounds(ax, ay, bx, by);

        const bool marked = HasMarkedArea();
        const TileCoordsXY sample = TerraformSampleTile();

        // Like land, the water action applies a tick later, so report the new level from the
        // game-action callback rather than immediately after Execute. With a marked area the sample
        // is the rectangle centre and the read-out is prefixed "Marked area".
        const auto announceAfterChange = [marked, sample](
                                             const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // failures are spoken automatically via the error window

            auto* surface = MapGetSurfaceElementAt(sample);
            if (surface == nullptr)
                return;

            const std::string prefix = marked ? "Marked area, " : "";
            const int32_t waterHeight = surface->GetWaterHeight();
            if (waterHeight > 0)
            {
                // The water surface sits one elevation step above the land it covers (water
                // replaces the tile rather than stacking on it), so drop one step to report the
                // water on the same scale the land would read at that spot.
                const int32_t level = std::max(0, waterHeight / kWaterHeightStep - 1);
                PlayElevationTone(level);
                ScreenReaderSpeak(prefix + "Water level " + std::to_string(level));
            }
            else
            {
                ScreenReaderSpeak(prefix + "No water");
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

    // Buys land ownership or construction rights over the brush (or marked) area. The action always
    // reports success (it silently skips tiles that aren't for sale or are already owned), so we
    // branch on the actual cost: if anything was bought, the finance hook also announces the amount
    // spent; otherwise we explain why nothing happened, based on a sample tile's ownership.
    static void ChangeLandOwnership(GameActions::LandBuyRightSetting setting)
    {
        int32_t ax, ay, bx, by;
        GetTerraformBounds(ax, ay, bx, by);
        const TileCoordsXY sample = TerraformSampleTile();

        const bool rights = (setting == GameActions::LandBuyRightSetting::buyConstructionRights);
        auto action = GameActions::LandBuyRightsAction(MapRange(ax, ay, bx, by), setting);
        action.SetCallback([rights, sample](const GameActions::GameAction*, const GameActions::Result* result) {
            if (result->error != GameActions::Status::ok)
                return; // spoken via the error window

            if (result->cost != 0)
            {
                _lastTileDescription.clear(); // ownership changed, so re-announce the tile next move
                ScreenReaderSpeak(rights ? "Construction rights bought" : "Land bought");
                return;
            }

            // Nothing was bought - explain why, using the sample tile (cursor, or area centre).
            auto* surface = MapGetSurfaceElementAt(sample);
            const int32_t ownership = surface != nullptr ? surface->GetOwnership() : 0;
            if (rights)
            {
                if (ownership & (OWNERSHIP_OWNED | OWNERSHIP_CONSTRUCTION_RIGHTS_OWNED))
                    ScreenReaderSpeak("You already have construction rights here");
                else if (!(ownership & OWNERSHIP_CONSTRUCTION_RIGHTS_AVAILABLE))
                    ScreenReaderSpeak("Construction rights here are not for sale");
                else
                    ScreenReaderSpeak("Nothing to buy here");
            }
            else
            {
                if (ownership & OWNERSHIP_OWNED)
                    ScreenReaderSpeak("You already own this land");
                else if (!(ownership & OWNERSHIP_AVAILABLE))
                    ScreenReaderSpeak("This land is not for sale");
                else
                    ScreenReaderSpeak("Nothing to buy here");
            }
        });
        GameActions::Execute(&action, getGameState());
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
        const int32_t x = SpokenCoordX(_cursor);
        const int32_t y = SpokenCoordY(_cursor);
        ScreenReaderSpeak("Park entrance, X " + std::to_string(x) + ", Y " + std::to_string(y));
    }

    static void ReportFacing()
    {
        static constexpr const char* kDirections[] = { "North", "East", "South", "West" };
        const uint8_t rotation = GetCurrentRotation() & 3;
        ScreenReaderSpeak(std::string("Facing ") + kDirections[rotation]);
    }

    static bool IsRideConstructionWindowOpen();

    // Shift+B: report where a ride's track is broken or incomplete (a gap or an open end), with the
    // coordinate, so the player knows where the circuit still needs joining. Uses the ride being
    // built if the construction window is open, otherwise the ride track under the cursor.
    static void ReportTrackBreaks()
    {
        RideId rideId = RideId::GetNull();
        if (IsRideConstructionWindowOpen())
            rideId = _currentRideIndex;
        if (rideId.IsNull())
        {
            for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
            {
                if (auto* track = el->asTrack(); track != nullptr && !el->isGhost())
                {
                    rideId = track->GetRideIndex();
                    break;
                }
                if (el->isLastForTile())
                    break;
                el++;
            }
        }

        auto* ride = rideId.IsNull() ? nullptr : GetRide(rideId);
        if (ride == nullptr)
        {
            ScreenReaderSpeak("No ride track here");
            return;
        }

        CoordsXYE origin;
        if (!RideTryGetOriginElement(*ride, &origin))
        {
            ScreenReaderSpeak("That ride has no track yet");
            return;
        }

        CoordsXYE gap;
        if (OpenRCT2::findTrackGap(*ride, origin, &gap))
        {
            const TileCoordsXY tile{ CoordsXY{ gap.x, gap.y } };
            const int32_t height = gap.element != nullptr ? gap.element->getBaseZ() / (kCoordsZStep * 2) : 0;
            ScreenReaderSpeak("Track break at " + SpokenTileCoordsText(tile) + ", height " + std::to_string(height));
        }
        else
        {
            ScreenReaderSpeak("Track is a complete circuit, no breaks");
        }
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

        // Shift+Left / Shift+Right rotate the camera. Do the rotation here (rather than letting it
        // fall through to the shortcut manager) so we can announce the compass direction the view
        // snaps to, then keep the spoken arrow-key axes aligned with the new rotation.
        if ((key == SDLK_LEFT || key == SDLK_RIGHT) && (modifiers & KMOD_SHIFT))
        {
            ViewportRotateAll(key == SDLK_RIGHT ? 1 : -1);
            ReportFacing();
            return true;
        }

        // Number keys 1-9 are waypoints: Shift+N drops/moves waypoint N at the cursor, plain N
        // warps to it. (SDL reports the digit keycode with KMOD_SHIFT set, so both share this key.)
        if (key >= SDLK_1 && key <= SDLK_9)
        {
            if (!_initialised)
                InitialiseCursor();
            const int32_t slot = static_cast<int32_t>(key - SDLK_1);
            if (modifiers & KMOD_SHIFT)
                SetWaypoint(slot);
            else
                JumpToWaypoint(slot);
            return true;
        }

        if (key != SDLK_UP && key != SDLK_DOWN && key != SDLK_LEFT && key != SDLK_RIGHT && key != SDLK_c
            && key != SDLK_t && key != SDLK_m && key != SDLK_SPACE && key != SDLK_d && key != SDLK_e
            && key != SDLK_f && key != SDLK_LEFTBRACKET && key != SDLK_RIGHTBRACKET && key != SDLK_p
            && key != SDLK_q && key != SDLK_x && key != SDLK_b && key != SDLK_o && key != SDLK_l
            && key != SDLK_k && key != SDLK_PAGEUP && key != SDLK_PAGEDOWN)
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
            case SDLK_o:
                // O buys land ownership over the brush area; Shift+O buys construction rights. The
                // amount spent is announced by the finance hook.
                if (modifiers & KMOD_SHIFT)
                    ChangeLandOwnership(GameActions::LandBuyRightSetting::buyConstructionRights);
                else
                    ChangeLandOwnership(GameActions::LandBuyRightSetting::buyLand);
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
            case SDLK_l:
                CycleSlopeMode();
                break;
            case SDLK_b:
                // Shift+B reports track breaks; plain B cycles the terraform/clear brush size.
                if (modifiers & KMOD_SHIFT)
                    ReportTrackBreaks();
                else
                    CycleBrushSize();
                break;
            case SDLK_k:
                // K cycles the area markers; Shift+K snaps the cursor between them.
                if (modifiers & KMOD_SHIFT)
                    SnapToMarker();
                else
                    CycleAreaMarker();
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
            // Arrow keys are SCREEN-relative at the default view rotation: the world axes run
            // diagonally on screen, so these world deltas are chosen (and the spoken coordinates
            // flipped, see SpokenCoordX/Y) so Left moves the camera/audio left and lowers X, Up
            // moves away and raises Y, etc.
            case SDLK_UP:
                MoveScreen(0, -1, "Up");
                break;
            case SDLK_DOWN:
                MoveScreen(0, 1, "Down");
                break;
            case SDLK_RIGHT:
                MoveScreen(-1, 0, "Right");
                break;
            case SDLK_LEFT:
                MoveScreen(1, 0, "Left");
                break;
        }
        return true;
    }

    // True while the ride construction window is open (build mode).
    static bool IsRideConstructionWindowOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr != nullptr && windowMgr->FindByClass(WindowClass::rideConstruction) != nullptr;
    }

    // Ride construction keyboard menu. While the construction window is open, Ctrl+arrows operate
    // the build menu (field navigation and value changes), Ctrl+Enter activates the focused item,
    // Ctrl+B reads the build state, and Escape exits (with confirmation). Plain arrows are NOT
    // consumed here, so the map tile cursor keeps working normally during construction. Returns true
    // if the key was consumed. Translated keys are forwarded to the window's onAccessibilityAction.
    static bool HandleRideConstructionAccessKey(uint32_t key, uint32_t modifiers)
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return false;
        auto* w = windowMgr->FindByClass(WindowClass::rideConstruction);
        if (w == nullptr)
            return false;

        std::optional<AccessibilityAction> action;
        if (modifiers & KMOD_CTRL)
        {
            switch (key)
            {
                case SDLK_UP:
                    action = AccessibilityAction::moveUp;
                    break;
                case SDLK_DOWN:
                    action = AccessibilityAction::moveDown;
                    break;
                case SDLK_LEFT:
                    action = AccessibilityAction::moveLeft;
                    break;
                case SDLK_RIGHT:
                    action = AccessibilityAction::moveRight;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    action = AccessibilityAction::activate;
                    break;
                case SDLK_b:
                    action = AccessibilityAction::announce; // Ctrl+B: read build state
                    break;
            }
        }
        if (key == SDLK_ESCAPE)
            action = AccessibilityAction::cancel;

        if (!action.has_value())
            return false;

        w->onAccessibilityAction(*action);
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
            _markerCount = 0; // drop any terraform-area markers; a freshly loaded park starts clean
            std::fill(std::begin(_waypointSet), std::end(_waypointSet), false); // and any waypoints
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

        // Ride construction: Ctrl+arrows (and Ctrl+Enter / Ctrl+B / Escape) drive the build menu
        // when the construction window is open; plain arrows fall through so the map cursor keeps
        // working. Checked before mouse/menu mode so the build menu works regardless of either.
        if (HandleRideConstructionAccessKey(key, e.modifiers))
        {
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

        // Cursor-driven ride placement and ride construction own the keyboard; make sure a toolbar
        // menu left open from selecting the ride doesn't intercept the arrows. Entering build mode
        // (the construction window) leaves toolbar menu mode so plain arrows drive the map cursor.
        if (_menuMode
            && (IsRideConstructionWindowOpen() || Windows::WindowTrackPlaceIsActive()
                || IsAccessibleRidePlacementActive() || IsAccessibleSceneryPlacementActive()))
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

        const int32_t x = SpokenCoordX(mn);
        const int32_t y = SpokenCoordY(mn);
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

    bool IsInMenuMode()
    {
        return _menuMode;
    }

    bool IsKeyboardNavActive()
    {
        // True whenever the keyboard drives navigation in-game - the map cursor OR the toolbar menu
        // (and accessible windows). In these states the arrow keys belong to navigation, so the
        // engine's arrow-key view scrolling must be suppressed or the camera (and positional audio)
        // drifts while the cursor stays put. Only mouse mode lets the keyboard scroll the view.
        return gLegacyScene == LegacyScene::playing && !_mouseMode;
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
