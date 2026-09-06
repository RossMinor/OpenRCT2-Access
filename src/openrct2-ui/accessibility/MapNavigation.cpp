/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MapNavigation.h"

#include "AccessSounds.h"
#include "Direction.h"
#include "Elevation.h"
#include "ElevationTone.h"
#include "GuestRescue.h"
#include "MenuNavigation.h"
#include "graph/GraphScreens.h"
#include "RidePlacement.h"
#include "SceneryPlacement.h"
#include "ScreenReader.h"

#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <openrct2-ui/UiContext.h>
#include <openrct2-ui/UiStringIds.h>
#include <openrct2-ui/input/ShortcutManager.h>
#include "ListNavigation.h"

#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathPlaceAction.h>
#include <openrct2/actions/general/GameSetSpeedAction.h>
#include <openrct2/actions/general/PauseToggleAction.h>
#include <openrct2/actions/footpath/FootpathAdditionRemoveAction.h>
#include <openrct2/actions/footpath/FootpathRemoveAction.h>
#include <openrct2/actions/terraform/ClearAction.h>
#include <openrct2/actions/ride/RideEntranceExitRemoveAction.h>
#include <openrct2/actions/scenery/BannerRemoveAction.h>
#include <openrct2/actions/scenery/LargeSceneryRemoveAction.h>
#include <openrct2/actions/scenery/SmallSceneryRemoveAction.h>
#include <openrct2/actions/scenery/WallRemoveAction.h>
#include <openrct2/actions/track/TrackRemoveAction.h>
#include <openrct2/actions/park/LandBuyRightsAction.h>
#include <openrct2/actions/peep/PeepPickupAction.h>
#include <openrct2/actions/terraform/LandLowerAction.h>
#include <openrct2/actions/terraform/LandRaiseAction.h>
#include <openrct2/actions/terraform/WaterLowerAction.h>
#include <openrct2/actions/terraform/WaterRaiseAction.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/core/Numerics.hpp>
#include <openrct2/Date.h>
#include <openrct2/entity/EntityList.h>
#include <openrct2/entity/Guest.h>
#include <openrct2/entity/Litter.h>
#include <openrct2/entity/Peep.h>
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
#include <openrct2/network/Network.h>
#include <openrct2/object/FootpathEntry.h>
#include <openrct2/object/FootpathObject.h>
#include <openrct2/object/FootpathSurfaceObject.h>
#include <openrct2/object/Object.h>
#include <openrct2/object/ObjectLimits.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectTypes.h>
#include <openrct2/object/PathAdditionEntry.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideConstruction.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/TrackIteration.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Banner.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/TileElementsView.h>
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
#include <utility>
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

    // Cache of every tile a ride occupies, for the whole-ride focus highlight (issue: highlight the
    // entire ride under the cursor, not a single tile). World coords, one per occupied tile.
    static RideId _cachedTilesRide = RideId::GetNull();
    static std::vector<CoordsXY> _cachedRideTiles;
    static bool _cachedTilesValid = false;

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

    // Numbered waypoints: bookmarks on the map. Shift + number drops or moves the waypoint in that
    // slot at the cursor; Ctrl + number warps the cursor to it. Slots are the digit keys 1-9 then 0
    // (the tenth). Session-only - cleared on park load, like the terraform markers.
    static constexpr int32_t kWaypointCount = 10;
    static TileCoordsXY _waypoints[kWaypointCount]{};
    static bool _waypointSet[kWaypointCount] = {};

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

    // How far the focus elevation (_scanHeight) may be raised above the ground with the Home/End
    // keys, in elevation units - well past any sane bridge height.
    static constexpr int32_t kMaxFocusElevationAboveGround = 64;

    // Order a tile's stacked features are read (Config::sound.accessibilityTileReadingOrder): from the
    // lowest feature up (default, read in build order like a queue) or the highest feature down (the
    // list reversed, like a stack). See GatherTileFeatures.
    static constexpr uint8_t kTileReadingOrderLowestFirst = 0;
    static constexpr uint8_t kTileReadingOrderHighestFirst = 1;

    // Elevation tone: a short sine beep whose pitch rises with terrain height. It plays only
    // when the cursor moves onto a tile at a different elevation, so scanning flat ground stays
    // silent. The sine sample is synthesised once and cached; pitch is set per play via the
    // mixer's playback rate.
    // Every level the previous tile sounded a tone for, so a level the cursor has been running
    // alongside stays silent until it actually changes.
    static std::vector<int32_t> _lastElevationLevels;
    static int32_t _lastStepCat = -1; // step-cue category of the previous tile (for "on transition" mode)

    // The cursor's focus elevation, as a tile-element baseHeight. Shift+Home/End snap it to the
    // element below/above on the tile (see ScanZLevel), and path/scenery deletion acts at this level.
    // While it sits at the ground (not lifted onto a structure) it tracks the ground as the cursor
    // moves; once a scan lifts it above the ground it is "locked" and stays put across moves, so the
    // player can navigate at that level - until a scan brings it back down to the ground, or a jump
    // resets it. _scanLocked distinguishes those two states.
    static int32_t _scanHeight = 0;
    static bool _scanLocked = false;

    // Whether a (non-queue) footpath is a soft dirt/soil path rather than a hard surface (tarmac,
    // stone, etc.), so the right footstep cue plays. There is no engine flag for this, so we key off
    // the surface object's identifier, which is "…footpath_surface.dirt" for the dirt paths.
    static bool PathIsDirtSurface(const PathElement& pathEl)
    {
        std::string_view id;
        if (const auto* surface = pathEl.GetSurfaceEntry(); surface != nullptr)
            id = surface->GetIdentifier();
        else if (const auto* legacy = pathEl.GetLegacyPathEntry(); legacy != nullptr)
            id = legacy->GetIdentifier();
        return id.find("dirt") != std::string_view::npos;
    }

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

    // Every tile (in world coords) that the given ride occupies, cached for the last ride so the map
    // is only rescanned when the highlighted ride changes. Used to outline the whole ride under the
    // cursor. Returns an empty list for a null ride.
    static const std::vector<CoordsXY>& ComputeRideTiles(RideId rideId)
    {
        if (_cachedTilesValid && _cachedTilesRide == rideId)
            return _cachedRideTiles;

        _cachedRideTiles.clear();
        _cachedTilesRide = rideId;
        _cachedTilesValid = true;
        if (rideId.IsNull())
            return _cachedRideTiles;

        const auto mapSize = getGameState().mapSize;
        for (int32_t y = 0; y < mapSize.y; y++)
            for (int32_t x = 0; x < mapSize.x; x++)
                if (GetRideAtTile(TileCoordsXY{ x, y }) == rideId)
                    _cachedRideTiles.push_back(TileCoordsXY{ x, y }.ToCoordsXY());

        return _cachedRideTiles;
    }

    // The localised name of a loaded object, or an empty string if not found.
    static std::string GetObjectName(ObjectType type, ObjectEntryIndex index)
    {
        auto& objManager = GetContext()->GetObjectManager();
        auto* obj = objManager.GetLoadedObject(type, index);
        return obj != nullptr ? std::string(obj->GetName()) : std::string();
    }

    // The surface name of a footpath, rewritten FOR SPEECH ONLY - the game's own windows and
    // tooltips keep showing the object's real name. Several of the stock paths are named after
    // something a sighted player can see but a blind one cannot, and a few carry a shape suffix that
    // says nothing about the surface. The original name is always kept recognisable so anything the
    // player reads in a guide or forum post still matches what they hear.
    std::string PathNameWithoutSuffix(std::string name)
    {
        // "Dirt Footpath (Rounded)", "Red Tarmac Footpath (Stairs)". The suffix describes the kerb or
        // step artwork, not the surface, so it pads out a name the player hears on every tile - and
        // it gets in the way of matching a path against a table of known types.
        if (!name.empty() && name.back() == ')')
        {
            const auto open = name.rfind('(');
            if (open != std::string::npos && open > 0)
            {
                name.erase(open);
                while (!name.empty() && name.back() == ' ')
                    name.pop_back();
            }
        }
        return name;
    }

    std::string SpokenPathSurfaceName(std::string name)
    {
        if (name.empty())
            return name;

        name = PathNameWithoutSuffix(std::move(name));

        const auto equalsIgnoreCase = [](const std::string& a, const char* b) {
            size_t i = 0;
            for (; i < a.size() && b[i] != '\0'; i++)
            {
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                    return false;
            }
            return i == a.size() && b[i] == '\0';
        };

        // Matched against the WHOLE name, so the colour-prefixed variants ("Red Tarmac Footpath")
        // are left alone - only the plain, unprefixed ones are ambiguous to the ear.
        if (equalsIgnoreCase(name, "Crazy Paving Footpath"))
            return name + " (cobblestone)"; // keeps the searchable name, adds what it looks like
        if (equalsIgnoreCase(name, "Tarmac Footpath"))
            return "Blue " + name;
        if (equalsIgnoreCase(name, "Ash Footpath"))
            return "Black " + name;

        return name;
    }

    // The footpath surface name to speak for a path element, from whichever object type it uses.
    static std::string GetSpokenPathName(const PathElement& path)
    {
        return SpokenPathSurfaceName(
            path.HasLegacyPathEntry() ? GetObjectName(ObjectType::paths, path.GetLegacyPathEntryIndex())
                                      : GetObjectName(ObjectType::footpathSurface, path.GetSurfaceEntryIndex()));
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

    static std::string DescribeTrackPiece(
        const OpenRCT2::TrackMetadata::TrackElementDescriptor& ted, const TrackElement* element = nullptr);
    static bool IsRideConstructionWindowOpen();
    static void GetBrushBounds(int32_t& ax, int32_t& ay, int32_t& bx, int32_t& by);

    // Ride name plus its footprint size in tiles, e.g. "Wooden Roller Coaster, 9 by 5". Used when
    // the map cursor passes over a finished ride outside build mode, so it reads as one ride rather
    // than a string of individual track pieces.
    static std::string RideNameWithDimensions(RideId rideId)
    {
        auto ride = GetRide(rideId);
        std::string name = ride != nullptr ? std::string(ride->getName()) : std::string("Ride");
        TileCoordsXY mn, mx;
        if (ComputeRideBounds(rideId, mn, mx))
            name += ", " + std::to_string(mx.x - mn.x + 1) + " by " + std::to_string(mx.y - mn.y + 1);
        return name;
    }

    // Describes everything on a tile, read from the top down so a blind player learns what is
    // stacked on it (e.g. a ride bridging over a path, or a bench on a path), not just the topmost
    // feature. Each element becomes one comma-separated part; the park-ownership status ("outside
    // park") is appended last, since it describes the land beneath everything. "Outside park" means
    // the tile is outside the owned/buildable area; buying the land makes it read as inside.
    // A spoken tile description plus whether the tile is bare ground (nothing but owned/unowned
    // land: "Empty" or "Outside park"). Callers that suppress a redundant read after a boundary cue
    // key off the bareGround flag, not off comparing the text to the literal words - so rewording the
    // description can never silently break that suppression.
    struct TileReadout
    {
        std::string text;
        bool bareGround = false;
    };

    // Ground litter (vomit, food wrappers, cans, cups, rubbish) sits on a tile as sprites, not as
    // tile elements, so the tile-element loop never sees it. Guests drop it on paths; without this a
    // blind player has no way to know a path is filthy or needs a handyman. Litter of the same kind
    // is grouped with a count, so a messy tile reads e.g. "Vomit, 3 empty cups" rather than a dozen
    // separate items. Appends its parts to the caller's list.
    static void GatherGroundLitter(const TileCoordsXY& tile, std::vector<std::string>& parts)
    {
        // name -> count, kept in first-seen order for a stable readout.
        std::vector<std::pair<std::string, int32_t>> counts;
        for (auto* litter : EntityTileList<Litter>(tile.ToCoordsXY()))
        {
            const StringId id = litter->getName();
            if (id == kStringIdNone)
                continue;
            std::string name = OpenRCT2::FormatStringID(id);
            auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& c) { return c.first == name; });
            if (it == counts.end())
                counts.emplace_back(std::move(name), 1);
            else
                it->second++;
        }
        for (auto& [name, n] : counts)
            parts.push_back(n > 1 ? std::to_string(n) + " " + name : name);
    }

    // True if a litter bin stands on this tile. Bins are path additions rather than objects in their
    // own right, so they hang off the path element, sharing that slot with benches, lamps and
    // fountains - the IS_BIN flag is what tells them apart. A ghost addition is a placement preview
    // that is not really there yet, so it does not count.
    static bool TileHasTrashBin(const TileCoordsXY& tile)
    {
        for (auto* pathEl : TileElementsView<PathElement>(tile.ToCoordsXY()))
        {
            if (pathEl->isGhost() || !pathEl->HasAddition() || pathEl->AdditionIsGhost())
                continue;
            const auto* entry = pathEl->GetAdditionEntry();
            if (entry != nullptr && (entry->flags & PATH_ADDITION_FLAG_IS_BIN))
                return true;
        }
        return false;
    }

    // True if there is vomit on this tile. Vomit is a litter entity rather than a tile element, and
    // the game stores it as two visual variants that mean the same thing to a player, so both count.
    static bool TileHasVomit(const TileCoordsXY& tile)
    {
        for (auto* litter : EntityTileList<Litter>(tile.ToCoordsXY()))
        {
            if (litter->subType == Litter::Type::vomit || litter->subType == Litter::Type::vomitAlt)
                return true;
        }
        return false;
    }

    // Gathers the spoken feature parts on a tile, ordered top-down (topmost feature first), with
    // water appended last since it sits beneath any structures. Land ownership is deliberately not
    // included - the caller decides how to phrase "Empty"/"Outside park" for a single tile versus a
    // whole brush area. A tile with nothing on it returns an empty vector.
    static std::vector<std::string> GatherTileFeatures(const TileCoordsXY& tile)
    {
        // Built in a single canonical bottom-to-top order (lowest feature first): water, then the
        // tile elements in the order they are stored (which is by height), then litter on top. The
        // reading-order setting decides how this list is read out: lowest-to-highest as built
        // (the default - like a queue), or reversed to highest-to-lowest (like a stack). Both modes
        // read exactly the same features in exactly mirrored order.
        std::vector<std::string> parts;

        // Water is the flooded surface, sitting beneath any structure, so it is the lowest feature.
        if (auto* surface = MapGetSurfaceElementAt(tile); surface != nullptr && surface->GetWaterHeight() > 0)
            parts.push_back("Water");

        // In build mode (the ride construction window is open) read each placed track piece's
        // shape/slope/bank for detailed construction; otherwise read the ride as a whole - its name
        // and footprint size - once, however many of its pieces sit on this tile.
        const bool buildMode = IsRideConstructionWindowOpen();
        RideId namedRide = RideId::GetNull();

        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            // Skip the construction preview ghost so the not-yet-built next piece never reads here.
            if (auto* track = el->asTrack(); track != nullptr && !el->isGhost())
            {
                if (buildMode)
                {
                    std::string piece = DescribeTrackPiece(
                        OpenRCT2::TrackMetadata::GetTrackElementDescriptor(track->GetTrackType()), track);
                    if (!piece.empty())
                        parts.push_back(piece);
                }
                else if (const RideId rid = track->GetRideIndex(); rid != namedRide)
                {
                    namedRide = rid; // only announce the ride once per tile, not per piece
                    std::string desc = RideNameWithDimensions(rid);
                    // Stalls (shops/facilities) have no entrance/exit tiles, so append which way the
                    // stall itself faces - matching the "facing" the ride entrance/exit readout gives.
                    if (auto ride = GetRide(rid);
                        ride != nullptr && ride->getRideTypeDescriptor().flags.has(RtdFlag::isShopOrFacility))
                    {
                        if (auto facing = GetShopFacing(*track); facing.has_value())
                            desc += std::string(", facing ") + GetWorldDirectionName(*facing);
                    }
                    parts.push_back(std::move(desc));
                }
            }
            else if (auto* entrance = el->asEntrance(); entrance != nullptr)
            {
                switch (entrance->GetEntranceType())
                {
                    case ENTRANCE_TYPE_PARK_ENTRANCE:
                        parts.push_back("Park entrance");
                        break;
                    case ENTRANCE_TYPE_RIDE_ENTRANCE:
                        // The doorway (where guests enter) faces opposite the element's stored
                        // direction, which points toward the station platform.
                        parts.push_back(std::string("Ride entrance, facing ") + GetWorldDirectionName(GetEntranceFacing(*entrance)));
                        break;
                    case ENTRANCE_TYPE_RIDE_EXIT:
                        parts.push_back(std::string("Ride exit, facing ") + GetWorldDirectionName(GetEntranceFacing(*entrance)));
                        break;
                }
            }
            else if (auto* p = el->asPath(); p != nullptr)
            {
                std::string name = GetSpokenPathName(*p);

                std::string label;
                if (p->IsQueue())
                {
                    if (name.empty())
                    {
                        label = "Queue line";
                    }
                    else
                    {
                        // Queue surfaces are usually named just by colour, so append "queue"
                        // unless the name already mentions it.
                        std::string lower = name;
                        for (auto& c : lower)
                            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        label = (lower.find("queue") != std::string::npos) ? name : name + " queue";
                    }
                }
                else
                {
                    label = name.empty() ? "Path" : name;
                }

                // A ramp reads as sloped so the player can tell a hill piece from a flat one.
                parts.push_back(p->IsSloped() ? "Sloped " + label : label);

                // Railings are the supports and side fencing, and they are a SEPARATE object from
                // the surface - so a path can legitimately carry railings that look nothing like it.
                // The game only draws them where a path leaves the ground, which is exactly why this
                // went unnoticed for so long: a sighted player sees the mismatch the moment a ramp
                // goes up, while the readout never mentioned that half of the path existed. Named
                // only where they are visible, so flat ground paths do not gain a word on every tile.
                if (!p->HasLegacyPathEntry())
                {
                    auto* pathSurface = MapGetSurfaceElementAt(tile);
                    const bool visible = p->IsSloped()
                        || (pathSurface != nullptr && p->baseHeight > pathSurface->baseHeight);
                    if (visible)
                    {
                        std::string rails = GetObjectName(ObjectType::footpathRailings, p->GetRailingsEntryIndex());
                        if (!rails.empty())
                            parts.push_back(std::move(rails));
                    }
                }

                // Path additions are the benches, litter bins, lamps and fountains placed on a
                // path. They live on the same element as the path; announce them as their own part
                // so a tile reads e.g. "Bench, Tarmac path".
                if (p->HasAddition())
                {
                    std::string addition = GetObjectName(ObjectType::pathAdditions, p->GetAdditionEntryIndex());
                    if (addition.empty())
                        addition = "Path addition";
                    // A vandalised addition (broken bench, bin, lamp or queue TV) still occupies the
                    // tile but no longer works until a handyman fixes it; call that out.
                    if (p->IsBroken())
                        addition += ", vandalized";
                    parts.push_back(std::move(addition));
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

        // Litter (dropped on the ground/paths) sits on top of the ground clutter, so it is the
        // highest feature - appended last in the canonical bottom-to-top order.
        GatherGroundLitter(tile, parts);

        // Default reads lowest-to-highest (as built). In highest-to-lowest mode, reverse the whole
        // list so the topmost feature is announced first - the exact mirror of the default order.
        if (Config::Get().sound.accessibilityTileReadingOrder == kTileReadingOrderHighestFirst)
            std::reverse(parts.begin(), parts.end());

        return parts;
    }

    // Every distinct elevation a tile's readout should name, in half steps. Bare ground never
    // qualifies - its height is the baseline everything else is measured against, and saying it on
    // every tile would be noise. What does qualify is anything standing off that baseline: an
    // elevated path or bridge, a raised ride entrance or exit, track passing overhead - and a
    // sloped path, which spans two levels by nature and so has no single "ground" to be level with.
    //
    // A tile can hold several of these at once (a path with a coaster flying over it), and each is
    // worth hearing, so they are all collected. Ordered by the tile reading-order setting, the same
    // way the feature list itself is, so heights and features are recited in the same direction.
    static std::vector<int32_t> TileReadoutElevations(const TileCoordsXY& tile)
    {
        std::vector<int32_t> heights;
        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface == nullptr)
            return heights;

        const int32_t ground = surface->baseHeight;
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (!el->isGhost() && el->getType() != TileElementType::surface)
            {
                auto* path = el->asPath();
                const bool sloped = path != nullptr && path->IsSloped();
                if (el->baseHeight > ground || sloped)
                {
                    // Several elements often share one level (a bench on a bridge deck); that is one
                    // height to the ear, not two.
                    const int32_t h = el->baseHeight;
                    if (std::find(heights.begin(), heights.end(), h) == heights.end())
                        heights.push_back(h);
                }
            }
            if (el->isLastForTile())
                break;
            el++;
        }

        // Tile elements are stored bottom-to-top, which is already the default reading order.
        std::sort(heights.begin(), heights.end());
        if (Config::Get().sound.accessibilityTileReadingOrder == kTileReadingOrderHighestFirst)
            std::reverse(heights.begin(), heights.end());
        return heights;
    }

    // Every level a tile should sound a tone for: the GROUND the cursor sits on, plus anything
    // standing above it at a different height. The ground is the base note - the terrain cue the
    // tone has always been - and each thing overhead gets its own, so a path with track above it
    // sounds as two pitches. Ordered the same way the readout is, so notes and words arrive in the
    // same direction.
    static std::vector<int32_t> ToneLevelsAt(const TileCoordsXY& tile)
    {
        auto levels = TileReadoutElevations(tile);
        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface == nullptr)
            return levels;

        const int32_t ground = surface->baseHeight;
        if (std::find(levels.begin(), levels.end(), ground) == levels.end())
        {
            if (Config::Get().sound.accessibilityTileReadingOrder == kTileReadingOrderHighestFirst)
                levels.push_back(ground);
            else
                levels.insert(levels.begin(), ground);
        }
        return levels;
    }

    // Sound a tile's levels, but only the ones that are NEW since the last tile. Each level is judged
    // on its own: a level the cursor has been running alongside - the ground beneath a bridge, or
    // track overhead - has not changed just because something else on the tile did, so it stays
    // silent. Walking under a coaster on level ground therefore beeps for the track alone, and
    // crossing flat open ground stays silent as it always has. Every cursor move and deliberate jump
    // goes through here so they all agree on what counts as a change.
    static void SoundElevationOnChange(const TileCoordsXY& tile)
    {
        auto levels = ToneLevelsAt(tile);

        std::vector<int32_t> fresh;
        for (int32_t level : levels)
            if (std::find(_lastElevationLevels.begin(), _lastElevationLevels.end(), level) == _lastElevationLevels.end())
                fresh.push_back(level);

        if (!fresh.empty())
            PlayElevationTones(fresh);

        // The baseline is what the tile actually holds, not just what sounded, so the next tile is
        // compared against the whole picture.
        _lastElevationLevels = std::move(levels);
    }

    // The elevations last appended to a tile readout, so "on change" can stay quiet while the cursor
    // runs along a bridge at one height. Cleared whenever a tile reports none, so stepping off a
    // bridge and back on says the height again.
    static std::vector<int32_t> _lastSpokenTileElevations;

    static TileReadout DescribeTileReadout(const TileCoordsXY& tile)
    {
        auto parts = GatherTileFeatures(tile);

        auto* surface = MapGetSurfaceElementAt(tile);
        const bool owned = surface != nullptr && (surface->GetOwnership() & OWNERSHIP_OWNED) != 0;

        if (parts.empty())
        {
            _lastSpokenTileElevations.clear();
            return { owned ? "Empty" : "Outside park", true };
        }

        // The composition helper owns the ", " joins and drops any empty fragment, so the seam is
        // decided in one place.
        SpeechBuilder sb;
        for (const auto& part : parts)
            sb.add(part);
        // The land beneath everything: note when the tile is outside the owned park area.
        if (!owned)
            sb.add("outside park");

        // Then the height of everything standing above the ground here, per the elevation-reading
        // setting: every tile, only when the set of heights changes, or never.
        const auto elevations = TileReadoutElevations(tile);
        const uint8_t mode = Config::Get().sound.accessibilityElevationReadMode;
        if (!elevations.empty() && mode != 2)
        {
            if (mode == 0 || _lastSpokenTileElevations != elevations)
            {
                // "elevation 5, 2.5" - the word once, then the bare numbers separated by commas.
                // Deliberately no "and" before the last: these are read constantly while moving, so
                // every extra word is one the player hears hundreds of times an hour.
                std::string text = "elevation ";
                for (size_t i = 0; i < elevations.size(); i++)
                {
                    if (i > 0)
                        text += ", ";
                    text += ElevationText(elevations[i]);
                }
                sb.add(text);
            }
        }
        _lastSpokenTileElevations = elevations;

        return { sb.str(), false };
    }

    // Reads out the whole square brush area (3x3, 5x5, 7x7) centred on the cursor, listing every
    // feature on every tile one by one - so a player sweeping with a large brush hears all the
    // bushes, fences, benches and so on inside it, not just the centre tile. Row-major over the
    // brush's world-tile bounds (clamped to the usable map, so it shrinks near the edge). Land
    // ownership is summarised once for the area rather than repeated per tile. This is a read-out
    // only; the step/elevation sounds and boundary cues stay tied to the single centre tile.
    static TileReadout DescribeBrushArea()
    {
        int32_t ax, ay, bx, by;
        GetBrushBounds(ax, ay, bx, by); // world coords, clamped to the usable map
        const int32_t minX = ax / kCoordsXYStep;
        const int32_t minY = ay / kCoordsXYStep;
        const int32_t maxX = bx / kCoordsXYStep;
        const int32_t maxY = by / kCoordsXYStep;

        // A brush read spans many tiles, so listing heights across them would be meaningless and none
        // are appended. Clear the "on change" baseline so shrinking back to a 1x1 brush names the
        // heights on the cursor's tile rather than comparing against whatever preceded the sweep.
        _lastSpokenTileElevations.clear();

        SpeechBuilder sb;
        bool anyFeature = false;
        bool anyUnowned = false;
        for (int32_t y = minY; y <= maxY; y++)
        {
            for (int32_t x = minX; x <= maxX; x++)
            {
                const TileCoordsXY tile{ x, y };
                for (auto& part : GatherTileFeatures(tile))
                {
                    sb.add(part);
                    anyFeature = true;
                }
                if (!IsTileOwned(tile))
                    anyUnowned = true;
            }
        }

        if (!anyFeature)
            return { anyUnowned ? "Outside park" : "Empty", true };
        // Note once if any of the area lies outside the owned park, rather than per tile.
        if (anyUnowned)
            sb.add("outside park");
        return { sb.str(), false };
    }

    static std::string GetTileDescription(const TileCoordsXY& tile)
    {
        return DescribeTileReadout(tile).text;
    }

    // Picks a starting tile for the cursor (first owned tile, else map centre).
    static void InitialiseCursor()
    {
        _initialised = true;
        _lastTileDescription.clear();
        _lastElevationLevels.clear();

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
            _lastElevationLevels = ToneLevelsAt(_cursor);
            _scanHeight = surface->baseHeight;
            _scanLocked = false;
        }
        _lastTileDescription = GetTileDescription(_cursor);
    }

    int32_t GetCursorWorkingZ()
    {
        return TileCoordsXYZ(_cursor.x, _cursor.y, _scanHeight).ToCoordsXYZ().z;
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
        using OpenRCT2::TrackMetadata::TrackCurve;

        // A station platform is three distinct piece types, not one: a one-tile station is
        // `endStation`, and extending it rewrites the earlier tiles into `beginStation` and
        // `middleStation`. Only `endStation` carries an in-game description, so the other two fell
        // through to the geometry below and - being straight and level - read as "straight". The
        // platform therefore disappeared from every tile except the most recently placed one. All
        // three share TrackGroup::stationEnd, so name them from the group and let every platform
        // tile speak the game's own wording.
        if (ted.definition.group == OpenRCT2::TrackGroup::stationEnd)
            return OpenRCT2::FormatStringID(STR_STATION_PLATFORM);

        const int32_t rotBegin = ted.coordinates.rotationBegin;
        const int32_t rotEnd = ted.coordinates.rotationEnd;

        // A rotation is not a plain compass value: the low two bits are the quarter it faces, and
        // BIT 2 marks a diagonal heading. So a piece turns if either changes - a quarter turn moves
        // the compass bits, while an eighth turn (the pieces the widest curve is built from) only
        // flips the diagonal bit. Masking the difference with 3, as this used to, discarded that bit
        // and reported half of the eighth pieces as "straight": rightEighthToDiag runs 0 -> 4 and
        // leftEighthToOrthogonal runs 4 -> 0, both of which mask to no turn at all.
        const bool quarterTurn = (rotBegin & 3) != (rotEnd & 3);
        const bool eighthTurn = (rotBegin & 4) != (rotEnd & 4);
        if (!quarterTurn && !eighthTurn)
            return ((rotBegin & 4) != 0) ? "diagonal straight" : "straight";

        if (((rotEnd - rotBegin) & 3) == 2)
            return "half turn";

        // Direction and radius come from the game's own curve classification rather than from the
        // geometry: it is exactly what the construction window's turn buttons select, so the name
        // matches the piece the player asked for. Deriving the radius from the track group or the
        // tile count instead never worked for every piece - the sloped turns carry no size in their
        // group at all, and the widest curve's pieces sit in the plain `flat` group.
        const auto& chain = ted.curveChain;
        const OpenRCT2::TrackMetadata::TypeOrCurve* named = nullptr;
        if (!chain.next.isTrackType && chain.next.curve != TrackCurve::none)
            named = &chain.next;
        else if (!chain.previous.isTrackType && chain.previous.curve != TrackCurve::none)
            named = &chain.previous;

        if (named != nullptr)
        {
            switch (named->curve)
            {
                case TrackCurve::leftVerySmall:
                    return "left very small curve";
                case TrackCurve::rightVerySmall:
                    return "right very small curve";
                case TrackCurve::leftSmall:
                    return "left small curve";
                case TrackCurve::rightSmall:
                    return "right small curve";
                case TrackCurve::left:
                    return "left curve";
                case TrackCurve::right:
                    return "right curve";
                case TrackCurve::leftLarge:
                    return "left large curve";
                case TrackCurve::rightLarge:
                    return "right large curve";
                default:
                    break;
            }
        }

        // Unclassified turn: fall back to the geometry for at least a direction.
        return std::string((((rotEnd - rotBegin) & 3) == 1) ? "right" : "left") + " curve";
    }

    // Builds the full spoken description of one track piece: its shape (the game's piece name, or a
    // derived name for basic pieces), its slope (level / gentle up / steep down, or a transition
    // like "level to gentle up"), and its banking when banked - all of the piece's attributes.
    //
    // `element` is the PLACED piece, when there is one. Whether a piece carries the chain lift is a
    // property of the placement, not of the piece type - the same slope exists with and without it -
    // so it can only be read off the element. Passing nullptr describes the type alone.
    static std::string DescribeTrackPiece(
        const OpenRCT2::TrackMetadata::TrackElementDescriptor& ted, const TrackElement* element)
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

        // Last, because it is the piece's most consequential property: a chain lift is what carries
        // the train up, and a slope built without one is the classic reason a coaster stalls.
        if (element != nullptr && element->HasChain())
            s += ", chain lift";
        return s;
    }

    std::string DescribeTrackPieceText(const TrackElement& element)
    {
        return DescribeTrackPiece(OpenRCT2::TrackMetadata::GetTrackElementDescriptor(element.GetTrackType()), &element);
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

        // While placing a pre-built ride, drive the game's placement ghost to follow the cursor so the
        // visual outline tracks the keyboard, showing exactly where the ride will land. No-op once the
        // preview is frozen (the ghost stays put for footprint inspection).
        if (Windows::WindowTrackPlaceIsActive())
        {
            const auto tdWorld = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
            Windows::WindowTrackPlaceUpdateGhost(CoordsXY{ tdWorld.x, tdWorld.y });
        }

        // Elevation tone: beep only when the new tile's set of heights differs from the last one, so
        // moving across flat ground stays silent. Pitch rises with elevation. The cursor's own level
        // always sounds first or last per the reading order; where something else stands at a
        // different height on the same tile - track flying over a path - that height gets its own
        // note, so the ear hears the same layering the readout describes.
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            SoundElevationOnChange(_cursor);
            // The focus elevation tracks the ground as the cursor moves, unless a scan has locked it
            // onto a raised level - then it stays put so the player can navigate at that elevation.
            if (!_scanLocked)
                _scanHeight = surface->baseHeight;
        }

        // Footstep-style cue for what the cursor just stepped onto: a dirt path, a hard path (tarmac/
        // stone), a queue, or open water. A path takes precedence over water beneath a bridge. Each
        // category randomly picks one of its clip variations. Plays at the accessibility cue volume,
        // and how often is governed by the step-sound mode setting (every step / on change / off).
        {
            // Category of the current tile for the step cue: 0 none, 1 dirt, 2 hard, 3 queue, 4 water.
            int32_t stepCat = 0;
            StepSound stepSnd = StepSound::dirt;
            bool hasStepSnd = false;
            for (auto* pathEl : TileElementsView<PathElement>(_cursor.ToCoordsXY()))
            {
                if (pathEl->isGhost())
                    continue;
                if (pathEl->IsQueue())
                {
                    stepCat = 3;
                    stepSnd = StepSound::queue;
                }
                else if (PathIsDirtSurface(*pathEl))
                {
                    stepCat = 1;
                    stepSnd = StepSound::dirt;
                }
                else
                {
                    stepCat = 2;
                    stepSnd = StepSound::hard;
                }
                hasStepSnd = true;
                break;
            }
            if (stepCat == 0)
            {
                if (auto* surf = MapGetSurfaceElementAt(_cursor); surf != nullptr && surf->GetWaterHeight() > 0)
                {
                    stepCat = 4;
                    stepSnd = StepSound::water;
                    hasStepSnd = true;
                }
            }

            const auto mode = static_cast<StepSoundMode>(Config::Get().sound.accessibilityStepSoundMode);
            // "On transition" mirrors the spoken tile announcements: only sound when the category
            // changes (e.g. first step onto a path), staying silent while continuing across the same
            // kind of tile. "Every step" always sounds; "off" never does.
            if (mode != StepSoundMode::off && hasStepSnd
                && (mode == StepSoundMode::everyStep || stepCat != _lastStepCat))
            {
                PlayStepSound(stepSnd);
            }
            _lastStepCat = stepCat;
        }

        // Object cues for two things a sighted player spots at a glance but that are easy to lose in
        // the middle of a spoken tile readout: a litter bin, and vomit on the ground. Between them
        // they answer "is this path being kept clean, and is there a bin here to keep it clean with",
        // which is otherwise several tiles' worth of listening to find out.
        //
        // Unlike the footstep cues these are not governed by the step-sound mode. That setting is
        // about the surface underfoot, which is continuous and quickly becomes noise when repeated;
        // these mark discrete objects the player is actively sweeping the cursor to find, so
        // suppressing them on repeats would hide exactly the tiles being searched for. Each sounds at
        // most once per tile no matter how many bins or vomit piles are on it.
        if (TileHasTrashBin(_cursor))
            PlayAccessSound(AccessSound::trashBin);
        if (TileHasVomit(_cursor))
            PlayAccessSound(AccessSound::vomit);

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

        // Every tile reads through one path, whatever stands on it. Ride track used to be
        // special-cased below: in build mode each piece was announced on EVERY move, bypassing the
        // tile-speech mode, and because it spoke a track-only string it dropped the tile's
        // elevations along with it. Track pieces are already part of the tile description in build
        // mode (see GatherTileFeatures), so the ordinary path reads them together with everything
        // else and honours the player's "every tile" / "on change" / "off" setting over track
        // exactly as over ground. If we just announced a boundary crossing, queue the read
        // (interrupt = false) so both are heard, but skip the bare-ground labels ("Empty"/"Outside
        // park") since the crossing already said it.
        // With a larger brush selected (3x3/5x5/7x7), the tile read-out enumerates every feature in
        // the whole brush area, one by one, so the player can survey it in a single move. The 1x1
        // brush reads the single cursor tile exactly as before. Only the read-out widens - the
        // step/elevation sounds and boundary cues above stay tied to the centre tile.
        TileReadout readout = (_brushSize > 1) ? DescribeBrushArea() : DescribeTileReadout(_cursor);
        std::string description = std::move(readout.text);
        // While a ride-placement preview is frozen, read its footprint tiles as though the ride were
        // already there, so the player can arrow over the preview and trace its shape/position.
        bool onPreviewTile = false;
        if (auto previewLabel = AccessibleRidePlacementPreviewLabel(_cursor); previewLabel.has_value())
        {
            // Shop / flat-ride footprint: a small area, so always read (onPreviewTile) regardless of the
            // tile-speech mode, letting the player trace each of its few tiles.
            description = readout.bareGround ? *previewLabel : (*previewLabel + ", " + description);
            readout.bareGround = false;
            onPreviewTile = true;
        }
        // A frozen pre-built ride/coaster design preview (the track-place window). Its footprint is
        // large, so this follows the tile-speech mode rather than forcing a read on every tile: on
        // "every tile" each tile reads, on "on change" the ride name reads once as the player enters the
        // footprint and stays quiet across the rest, and on "off" it is silent - whatever the setting.
        else if (auto tdLabel = Windows::WindowTrackPlacePreviewLabel(_cursor); tdLabel.has_value())
        {
            description = readout.bareGround ? *tdLabel : (*tdLabel + ", " + description);
            readout.bareGround = false;
        }
        const auto tileMode = static_cast<TileSpeechMode>(Config::Get().sound.accessibilityTileSpeechMode);
        if (tileMode != TileSpeechMode::off || onPreviewTile)
        {
            // "Every tile" reads on every move; "on change" (the original behaviour) reads only when
            // the description differs from the previous tile. A ride-preview footprint tile always
            // reads so the player can trace the whole shape.
            if (onPreviewTile || tileMode == TileSpeechMode::everyTile || description != _lastTileDescription)
            {
                // Skip the bare-ground label right after a boundary cue (which already said it). This
                // rides on the describer's flag, not on matching its wording.
                if (!(announcedCrossing && readout.bareGround))
                    ScreenReaderSpeak(description, !announcedCrossing);
            }
            _lastTileDescription = std::move(description);
        }
        else
        {
            // Off: stay silent, but keep the baseline current so switching back to "on change" does
            // not immediately re-announce a stale tile.
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
        // The cursor's own level: the ground it sits on, or whatever height the focus has been
        // deliberately lifted to. Deliberately NOT the highest walkable surface on the tile - a
        // bridge or elevated queue passing overhead is not where the cursor is, and reporting it
        // here told the player they were standing on something they were underneath. The heights of
        // things above are named by the tile readout instead, which can list them all.
        if (MapGetSurfaceElementAt(_cursor) != nullptr)
            text += ", elevation " + ElevationText(_scanHeight);
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
        static RideId lastHighlightRide = RideId::GetNull();

        const bool wantTile = IsMapCursorActive() && _initialised && !gInputFlags.has(InputFlag::toolActive);
        if (wantTile)
        {
            const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();

            // While positioning a ride, highlight the whole footprint it would occupy (range highlight).
            MapRange footprint;
            const bool placing = AccessibleRidePlacementFootprintRange(CoordsXY{ world.x, world.y }, footprint);

            // Over a finished ride, outline the entire ride (every tile it occupies) rather than the
            // single tile under the cursor.
            const RideId rideUnderCursor = placing ? RideId::GetNull()
                                                   : GetRideAtTile(TileCoordsXY{ _cursor.x, _cursor.y });

            if (!rideUnderCursor.IsNull())
            {
                // Rebuild the selected-tile set only when the highlighted ride changes; it's the same
                // list every frame otherwise.
                if (rideUnderCursor != lastHighlightRide || !gMapSelectFlags.has(MapSelectFlag::enableConstruct))
                {
                    MapSelection::clearSelectedTiles();
                    for (const auto& t : ComputeRideTiles(rideUnderCursor))
                        MapSelection::addSelectedTile(t);
                }
                gMapSelectFlags.unset(MapSelectFlag::enable);
                gMapSelectFlags.set(MapSelectFlag::enableConstruct);
            }
            else
            {
                // A single cursor tile, or the footprint of a ride being positioned.
                if (placing)
                    setMapSelectRange(footprint);
                else
                    setMapSelectRange(CoordsXY{ world.x, world.y });
                gMapSelectType = MapSelectType::full;
                gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
                MapSelection::clearSelectedTiles();
                gMapSelectFlags.set(MapSelectFlag::enable);
            }
            lastHighlightRide = rideUnderCursor;
            MapSelection::invalidate();
            weSetSelection = true;
        }
        else if (weSetSelection && !gInputFlags.has(InputFlag::toolActive))
        {
            gMapSelectFlags.unset(MapSelectFlag::enable);
            gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
            MapSelection::clearSelectedTiles();
            MapSelection::invalidate();
            weSetSelection = false;
            lastHighlightRide = RideId::GetNull();
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

    // The in-game date as spoken text, e.g. "Monday 1st March, Year 1", plus ", paused" when paused.
    static std::string DateText()
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
        return text;
    }

    [[maybe_unused]] static void AnnounceDateTime()
    {
        ScreenReaderSpeak(DateText());
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

    // --- Bottom-toolbar status readout (T key) ---------------------------------------------------
    // A small read-only "menu" of the figures on the game's bottom toolbar: date, park rating,
    // guests in park, cash, and access to recent messages. T opens it, Up/Down (or Left/Right)
    // cycle the items, Enter on "Recent messages" opens the news window, and Escape (or T) leaves.
    static bool _statusMode = false;
    static int32_t _statusIndex = 0;
    static constexpr int32_t kStatusItemCount = 5;

    static std::string StatusItemText(int32_t index)
    {
        auto& park = getGameState().park;
        switch (index)
        {
            case 0:
                return "Date, " + DateText();
            case 1:
                return "Park rating, " + std::to_string(park.rating) + " out of 999";
            case 2:
                return std::to_string(park.numGuestsInPark)
                    + (park.numGuestsInPark == 1 ? " guest in park" : " guests in park");
            case 3:
            {
                const StringId id = park.cash < 0 ? STR_BOTTOM_TOOLBAR_CASH_NEGATIVE : STR_BOTTOM_TOOLBAR_CASH;
                return "Cash, " + OpenRCT2::FormatStringID(id, park.cash);
            }
            case 4:
                return "Recent messages, press Enter to open";
        }
        return {};
    }

    static void AnnounceStatusItem()
    {
        ScreenReaderSpeakItem(StatusItemText(_statusIndex), _statusIndex, kStatusItemCount);
    }

    static void EnterStatusMode()
    {
        _statusMode = true;
        _statusIndex = 0;
        AnnounceStatusItem();
    }

    // Handles keys while the status readout is open. Returns true if the key was consumed.
    static bool HandleStatusModeKey(uint32_t key)
    {
        switch (key)
        {
            case SDLK_ESCAPE:
            case SDLK_t:
                _statusMode = false;
                ScreenReaderSpeak("Closed");
                return true;
            case SDLK_UP:
            case SDLK_LEFT:
                _statusIndex = (_statusIndex - 1 + kStatusItemCount) % kStatusItemCount;
                AnnounceStatusItem();
                return true;
            case SDLK_DOWN:
            case SDLK_RIGHT:
                _statusIndex = (_statusIndex + 1) % kStatusItemCount;
                AnnounceStatusItem();
                return true;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (_statusIndex == 4) // Recent messages
                {
                    _statusMode = false;
                    ContextOpenWindow(WindowClass::recentNews);
                    ScreenReaderSpeak("Recent messages");
                }
                else
                {
                    AnnounceStatusItem();
                }
                return true;
            default:
                return true; // swallow other keys while the readout is open
        }
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

    // Raises (+1) or lowers (-1) the cursor's focus elevation by one elevation unit and announces it.
    // This is the same single elevation the Shift+Home/End scan sets, the read-coordinates key
    // reports, and path building / deletion act at: at ground level it tracks the terrain; once lifted
    // it locks and persists as the cursor moves, so the player can build, delete, and navigate at a
    // chosen elevation (an elevated path spans water and gaps there). One unit is two baseHeight steps.
    static void ChangeFocusElevation(int32_t delta)
    {
        auto* surface = MapGetSurfaceElementAt(_cursor);
        const int32_t ground = surface != nullptr ? surface->baseHeight : 0;
        const int32_t maxHeight = ground + kMaxFocusElevationAboveGround * 2;

        const int32_t previous = _scanHeight;
        _scanHeight = std::clamp(_scanHeight + delta * 2, ground, maxHeight);
        _scanLocked = _scanHeight > ground;

        if (_scanHeight == previous)
        {
            ScreenReaderSpeak(
                std::string(delta > 0 ? "Maximum elevation, elevation " : "Ground level, elevation ")
                + ElevationText(_scanHeight));
            return;
        }
        ScreenReaderSpeak("Elevation " + ElevationText(_scanHeight));
    }

    // Defined later in the file; declared here so the path commands can act on a marked area.
    static bool HasMarkedArea();
    static void GetTerraformBounds(int32_t& ax, int32_t& ay, int32_t& bx, int32_t& by);
    static void BuildPathArea(ObjectEntryIndex type, PathConstructFlags flags);
    static void RemovePathArea();

    // The world direction the player is facing: the direction toward the top of the screen at the
    // current camera rotation - the way the Up arrow moves the cursor, and exactly what the F key
    // reports. Derived with the same screen-delta + camera-rotation logic the cursor movement uses,
    // so it stays correct as the camera is rotated. A sloped path rises (or falls) toward this, so
    // the slope follows where the player is facing rather than whichever way they last nudged.
    static Direction CameraFacingDirection()
    {
        int32_t dx = 0, dy = -1; // screen "up", as MoveScreen uses it
        for (int32_t i = 0, steps = GetCurrentRotation() & 3; i < steps; i++)
        {
            const int32_t nx = dy;
            const int32_t ny = -dx;
            dx = nx;
            dy = ny;
        }
        // World delta to Direction, matching Move(): +x = West, -x = East, +y = South, -y = North.
        if (dx > 0)
            return 2; // West
        if (dx < 0)
            return 0; // East
        if (dy > 0)
            return 1; // South
        return 3;     // North
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

        // With a marked rectangle active, pave the whole area (flat, terrain-following) rather than a
        // single tile - the same area the land/water tools and scenery clearing act on.
        if (HasMarkedArea())
        {
            BuildPathArea(type, flags);
            return;
        }

        const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
        const Direction dir = CameraFacingDirection();
        const auto behindEdge = BehindPathEdgeHeight(dir);

        FootpathSlope slope{};
        int32_t baseZ = 0;
        Direction actionDir = kInvalidDirection;
        std::string what;

        auto* buildSurface = MapGetSurfaceElementAt(_cursor);
        const bool elevated = buildSurface != nullptr && _scanHeight > buildSurface->baseHeight;
        if (_slopeMode != SlopeMode::flat)
        {
            // Sloped path. The ramp is anchored to the cursor's current focus elevation: to slope up,
            // stand at the elevation the existing path ends at and the ramp rises one step toward the
            // facing direction; to slope down, it falls one step that way. On success (below) the cursor
            // then follows the ramp to its new end so successive pieces chain from where it left off.
            const int32_t cursorZ = _scanHeight * kCoordsZStep;
            actionDir = dir;
            if (_slopeMode == SlopeMode::up)
            {
                baseZ = cursorZ;
                slope = { FootpathSlopeType::sloped, dir };
                what = std::string(elevated ? "Elevated ramp up to the " : "Ramp up to the ")
                    + GetWorldDirectionName(dir);
            }
            else // SlopeMode::down
            {
                baseZ = cursorZ - kPathHeightStep;
                slope = { FootpathSlopeType::sloped, DirectionReverse(dir) };
                what = std::string(elevated ? "Elevated ramp down to the " : "Ramp down to the ")
                    + GetWorldDirectionName(dir);
            }
        }
        else if (elevated)
        {
            // Flat path lifted above the ground: lay a deck at the focus elevation so a bridge can
            // cross water or a gap. Supports are generated by the game.
            baseZ = _scanHeight * kCoordsZStep;
            actionDir = dir;
            slope = { FootpathSlopeType::flat, 0 };
            what = gFootpathSelection.isQueueSelected ? "Elevated queue built" : "Elevated path built";
        }
        else
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

            // Bridge over a ride: the engine refuses a flat path at ground level when a ride's track
            // occupies the tile (rollercoasters can't take level crossings), reporting an obstruction.
            // If that is the only problem, lift the path into a flat elevated deck sitting just above
            // the track so it passes over the ride. This only triggers when a ride is actually in the
            // way AND the ground placement fails with an obstruction, so ordinary ground paths - even
            // right next to a ride - are unaffected. Probing uses Query (no side effects, no error
            // window); the first clear height above the ground wins.
            const CoordsXYZ groundLoc{ world.x, world.y, baseZ };
            auto groundProbe = GameActions::FootpathPlaceAction(
                groundLoc, slope, type, gFootpathSelection.railings, actionDir, flags);
            if (GameActions::Query(&groundProbe, getGameState()).error == GameActions::Status::noClearance
                && !GetRideAtTile(_cursor).IsNull())
            {
                // Climb in kPathHeightStep, the engine's own path step, from a step-aligned base.
                // Probing in kCoordsZStep (half a step) would take the first height that merely
                // clears the ride, which is off the path grid half the time - and a path half a step
                // off can never connect to its neighbours, however far it runs.
                const int32_t groundZ = Numerics::floor2(baseZ, kPathHeightStep);
                const int32_t maxZ = groundZ + 20 * kPathHeightStep; // generous: clears even tall coasters
                for (int32_t z = groundZ + kPathHeightStep; z <= maxZ; z += kPathHeightStep)
                {
                    auto lifted = GameActions::FootpathPlaceAction(
                        CoordsXYZ{ world.x, world.y, z }, FootpathSlope{ FootpathSlopeType::flat, 0 }, type,
                        gFootpathSelection.railings, dir, flags);
                    if (GameActions::Query(&lifted, getGameState()).error == GameActions::Status::ok)
                    {
                        baseZ = z;
                        slope = { FootpathSlopeType::flat, 0 };
                        actionDir = dir;
                        const int32_t steps = (z - groundZ) / kPathHeightStep;
                        what = gFootpathSelection.isQueueSelected ? "Queue built over the ride"
                                                                  : "Path built over the ride";
                        if (steps > 0)
                            what += ", " + std::to_string(steps) + (steps == 1 ? " tile up" : " tiles up");
                        break;
                    }
                }
            }
        }

        // Matching the sighted game, a path on a water tile goes on the submerged land (the vanilla
        // path tool clicks straight through water to the bed and the engine allows it). A sighted
        // player watches it disappear under the surface, so say so.
        if (buildSurface != nullptr && buildSurface->GetWaterHeight() > baseZ)
            what += ", underwater";

        const CoordsXYZ loc{ world.x, world.y, baseZ };
        auto action = GameActions::FootpathPlaceAction(
            loc, slope, type, gFootpathSelection.railings, actionDir, flags);
        const auto result = GameActions::Execute(&action, getGameState());
        if (result.error == GameActions::Status::ok)
        {
            PlayAccessSound(AccessSound::place);
            _lastTileDescription.clear();

            // A ramp carries the focus elevation to its far end so successive ramp pieces chain without
            // re-snapping: build up and the cursor rises one step, build down and it drops one. The lock
            // keeps that elevation as the cursor moves, releasing only once it settles back on the ground.
            if (_slopeMode != SlopeMode::flat)
            {
                const int32_t stepBase = kPathHeightStep / kCoordsZStep; // base-height units per path step
                _scanHeight += (_slopeMode == SlopeMode::up) ? stepBase : -stepBase;
                _scanLocked = buildSurface == nullptr || _scanHeight > buildSurface->baseHeight;
                what += ", elevation " + ElevationText(_scanHeight);
            }

            ScreenReaderSpeak(what);
        }
        // Failures are spoken automatically via the error window.
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
            // Match normal movement: only sound the elevation tone when the levels actually change,
            // so jumping to a marker/waypoint at the same height as the current tile stays silent.
            SoundElevationOnChange(_cursor);
            _scanHeight = surface->baseHeight;
            _scanLocked = false; // a deliberate jump lands the focus back on the ground
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
            // Match normal movement: only sound the elevation tone when the levels actually change,
            // so jumping to a marker/waypoint at the same height as the current tile stays silent.
            SoundElevationOnChange(_cursor);
            _scanHeight = surface->baseHeight;
            _scanLocked = false; // a deliberate jump lands the focus back on the ground
        }

        const std::string description = GetTileDescription(_cursor);
        _lastTileDescription = description;
        ScreenReaderSpeak(
            "Waypoint " + std::to_string(slot + 1) + ", " + description + ", X "
            + std::to_string(SpokenCoordX(_cursor)) + ", Y " + std::to_string(SpokenCoordY(_cursor)));
    }

    // Removes small and large scenery (trees, bushes, statues, etc.) across the brush area.
    // Removes scenery (small, large and walls) sitting at exactly targetBaseHeight across the given
    // world-coordinate area. Elements are gathered before any removal, since removing one rewrites the
    // tile's element list. Used when the cursor's focus is lifted onto a raised level, so clearing
    // acts at that elevation rather than on everything stacked on the tile.
    static void ClearSceneryAtElevation(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t targetBaseHeight)
    {
        // type: 0 = small scenery, 1 = large scenery, 2 = wall.
        struct Removal
        {
            int32_t type;
            CoordsXYZD loc;
            uint8_t quadrant;
            ObjectEntryIndex entryOrSequence;
        };
        std::vector<Removal> removals;
        for (int32_t wy = ay; wy <= by; wy += kCoordsXYStep)
        {
            for (int32_t wx = ax; wx <= bx; wx += kCoordsXYStep)
            {
                const TileCoordsXY tile{ wx / kCoordsXYStep, wy / kCoordsXYStep };
                for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
                {
                    if (!el->isGhost() && el->baseHeight == targetBaseHeight)
                    {
                        if (auto* ss = el->asSmallScenery(); ss != nullptr)
                            removals.push_back({ 0, { wx, wy, el->getBaseZ(), 0 }, ss->GetSceneryQuadrant(), ss->GetEntryIndex() });
                        else if (auto* ls = el->asLargeScenery(); ls != nullptr)
                            removals.push_back(
                                { 1, { wx, wy, el->getBaseZ(), el->getDirection() }, 0, ls->GetSequenceIndex() });
                        else if (el->asWall() != nullptr)
                            removals.push_back({ 2, { wx, wy, el->getBaseZ(), el->getDirection() }, 0, 0 });
                    }
                    if (el->isLastForTile())
                        break;
                    el++;
                }
            }
        }

        int32_t removed = 0;
        const bool prevErrorSound = Windows::gDisableErrorWindowSound;
        Windows::gDisableErrorWindowSound = true; // several removals would otherwise stack error beeps
        for (const auto& r : removals)
        {
            bool ok = false;
            if (r.type == 0)
            {
                auto a = GameActions::SmallSceneryRemoveAction(
                    { r.loc.x, r.loc.y, r.loc.z }, r.quadrant, r.entryOrSequence);
                ok = GameActions::Execute(&a, getGameState()).error == GameActions::Status::ok;
            }
            else if (r.type == 1)
            {
                auto a = GameActions::LargeSceneryRemoveAction(r.loc, static_cast<uint16_t>(r.entryOrSequence));
                ok = GameActions::Execute(&a, getGameState()).error == GameActions::Status::ok;
            }
            else
            {
                auto a = GameActions::WallRemoveAction(r.loc);
                ok = GameActions::Execute(&a, getGameState()).error == GameActions::Status::ok;
            }
            if (ok)
                removed++;
        }
        Windows::gDisableErrorWindowSound = prevErrorSound;

        _lastTileDescription.clear();
        ScreenReaderSpeak(removed > 0 ? "Scenery cleared" : "No scenery at this elevation");
    }

    static void ClearSceneryAtCursor()
    {
        int32_t ax, ay, bx, by;
        GetTerraformBounds(ax, ay, bx, by);

        // With the focus lifted onto a raised level, clear only scenery at that elevation. At the
        // default ground focus, clear everything in the area (all heights) as before, so the common
        // case and uneven terrain (where a brush spans tiles at different ground heights) are unaffected.
        if (_scanLocked)
        {
            ClearSceneryAtElevation(ax, ay, bx, by, _scanHeight);
            return;
        }

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

    // Paves every tile in the marked rectangle with a flat, terrain-following path. Slope mode does
    // not apply to an area fill - it lays flat paths, not ramps. Tiles where a path cannot sit (too
    // uneven, already occupied, or off the player's land) are skipped, and the total is announced.
    static void BuildPathArea(ObjectEntryIndex type, PathConstructFlags flags)
    {
        int32_t ax, ay, bx, by;
        GetTerraformBounds(ax, ay, bx, by);

        int32_t built = 0;
        int32_t builtUnderwater = 0;
        // Tiles that already hold a path or are otherwise occupied fail to build; each failure would
        // sound the error window, stacking into a painful blast across an area, so silence it for the
        // sweep (the tile counts below already report how many actually paved).
        const bool prevErrorSound = Windows::gDisableErrorWindowSound;
        Windows::gDisableErrorWindowSound = true;
        for (int32_t wy = ay; wy <= by; wy += kCoordsXYStep)
        {
            for (int32_t wx = ax; wx <= bx; wx += kCoordsXYStep)
            {
                const TileCoordsXY tile{ wx / kCoordsXYStep, wy / kCoordsXYStep };
                auto placement = FootpathGetOnTerrainPlacement(tile);
                if (!placement.isValid() || placement.slope.type == FootpathSlopeType::irregular)
                    continue;

                const CoordsXYZ loc{ wx, wy, placement.baseZ };
                auto action = GameActions::FootpathPlaceAction(
                    loc, placement.slope, type, gFootpathSelection.railings, kInvalidDirection, flags);
                if (GameActions::Execute(&action, getGameState()).error == GameActions::Status::ok)
                {
                    built++;
                    // On-terrain placement goes on the submerged land of water tiles, like the
                    // sighted path tool; count those so the summary can say where the paving sank.
                    auto* surface = MapGetSurfaceElementAt(tile);
                    if (surface != nullptr && surface->GetWaterHeight() > placement.baseZ)
                        builtUnderwater++;
                }
            }
        }
        Windows::gDisableErrorWindowSound = prevErrorSound;

        _lastTileDescription.clear();
        if (built > 0)
        {
            std::string spoken = (gFootpathSelection.isQueueSelected ? "Marked area queued, " : "Marked area paved, ")
                + std::to_string(built) + (built == 1 ? " tile" : " tiles");
            if (builtUnderwater > 0)
                spoken += ", " + std::to_string(builtUnderwater) + " underwater";
            ScreenReaderSpeak(spoken);
        }
        else
            ScreenReaderSpeak("Could not build any paths in the marked area");
    }

    // Counts the non-ghost path elements across the tile rectangle [ax,ay]..[bx,by] (world coords).
    static int32_t CountPathsInArea(int32_t ax, int32_t ay, int32_t bx, int32_t by)
    {
        int32_t count = 0;
        for (int32_t wy = ay; wy <= by; wy += kCoordsXYStep)
        {
            for (int32_t wx = ax; wx <= bx; wx += kCoordsXYStep)
            {
                const TileCoordsXY tile{ wx / kCoordsXYStep, wy / kCoordsXYStep };
                for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
                {
                    if (auto* p = el->asPath(); p != nullptr && !p->isGhost())
                        count++;
                    if (el->isLastForTile())
                        break;
                    el++;
                }
            }
        }
        return count;
    }

    // Removes every path from every tile in the marked rectangle. Uses a single ClearAction (the
    // same command the clear-scenery tool uses), which removes each path through a NESTED action -
    // so, unlike a per-tile loop of top-level FootpathRemoveActions, it never opens a per-tile error
    // window. That matters because each such error window plays the error sound and is read aloud;
    // across an area those stacked into a painfully loud blast. Announces the number actually removed.
    static void RemovePathArea()
    {
        int32_t ax, ay, bx, by;
        GetTerraformBounds(ax, ay, bx, by);

        const int32_t before = CountPathsInArea(ax, ay, bx, by);

        auto action = GameActions::ClearAction(
            MapRange(ax, ay, bx, by), GameActions::CLEARABLE_ITEMS::kSceneryFootpath);
        GameActions::Execute(&action, getGameState());

        // Key the message off how many paths were present before the clear, not a recount after it:
        // the ClearAction applies a tick later, so recounting immediately still sees the paths and
        // wrongly reports "No paths" even though the removal worked.
        _lastTileDescription.clear();
        if (before > 0)
            ScreenReaderSpeak("Paths removed");
        else
            ScreenReaderSpeak("No paths in the marked area");
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
            std::string name = GetSpokenPathName(*p);
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

    // Read-only vertical (Z-axis) scan, bound to Shift+Home (dir > 0, next element above) and
    // Shift+End (dir < 0, next element below). Snaps the probe to the nearest element above/below
    // the current scan height on the cursor's tile and reads it as "<name> <elevation>" (e.g.
    // "Tarmac path 6"), playing the elevation tone so the player can feel where it sits. The scan
    // height resets to the ground each time the cursor moves, so a scan starts from ground level.
    static void ScanZLevel(int32_t dir)
    {
        TileElement* best = nullptr;
        int32_t bestHeight = (dir > 0) ? std::numeric_limits<int32_t>::max() : std::numeric_limits<int32_t>::min();
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (!el->isGhost())
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
        // Lock the focus when it is lifted above the ground so it persists as the cursor moves; when a
        // scan brings it back down to the ground, unlock so it tracks the ground again.
        auto* surf = MapGetSurfaceElementAt(_cursor);
        _scanLocked = surf != nullptr && bestHeight > surf->baseHeight;

        // bestHeight is a baseHeight, which is already the mod's half-step unit. A Z scan is the one
        // place that deliberately lands on odd heights - track and sloped paths sit between steps -
        // so this is exactly where the ".5" earns its keep.
        PlayElevationTone(bestHeight);
        ScreenReaderSpeak(DescribeScanElement(best) + " " + ElevationText(bestHeight));
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

            PlayAccessSound(AccessSound::land);

            std::string spoken;
            if (auto* surface = MapGetSurfaceElementAt(sample); surface != nullptr)
            {
                // The land's own new height - this command raised or lowered the terrain, so a path
                // bridging overhead is not what just changed and must not be what is reported.
                const int32_t elevation = surface->baseHeight;
                PlayElevationTone(elevation);
                spoken = "elevation " + ElevationText(elevation);
                if (!marked)
                {
                    // Terraforming changed the tile under the cursor, so the level set it last
                    // sounded no longer describes it; let the next move sound it afresh.
                    _lastElevationLevels.clear();
                    _scanHeight = surface->baseHeight; // keep the Z-axis probe at the new ground level
                    _scanLocked = false;
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

            PlayAccessSound(AccessSound::water);

            auto* surface = MapGetSurfaceElementAt(sample);
            if (surface == nullptr)
                return;

            const std::string prefix = marked ? "Marked area, " : "";
            const int32_t waterHeight = surface->GetWaterHeight();
            if (waterHeight > 0)
            {
                // Report the water surface on the same engine scale as everything else. This is the
                // one place the water level itself is the number that matters; the ambient elevation
                // cues (movement tone, coordinate readout) read the land like the game's own height
                // markers do, so water only speaks its level here, where the player is changing it.
                const int32_t level = ElevationHalfSteps(waterHeight);
                PlayElevationTone(level);
                ScreenReaderSpeak(prefix + "Water level " + ElevationText(level));
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
        // With a marked rectangle active, strip paths from the whole area, like clearing scenery.
        if (HasMarkedArea())
        {
            RemovePathArea();
            return;
        }

        // Remove the path at the cursor's focus elevation (set by the Shift+Home/End scan). If the
        // focus is locked onto a raised level, only a path exactly there qualifies; at the default
        // ground focus, fall back to the tile's path so plain deletion still works without scanning.
        PathElement* pathElement = nullptr;
        PathElement* firstPath = nullptr;
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (auto* p = el->asPath(); p != nullptr)
            {
                if (firstPath == nullptr)
                    firstPath = p;
                if (p->baseHeight == _scanHeight)
                {
                    pathElement = p;
                    break;
                }
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        if (pathElement == nullptr && !_scanLocked)
            pathElement = firstPath;

        if (pathElement == nullptr)
        {
            ScreenReaderSpeak(firstPath != nullptr ? "No path at this elevation" : "No path here");
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

    // ---- Delete: remove one thing at the cursor -------------------------------------------------
    //
    // Delete is the scalpel; D and X remain the area sweeps, working at the focus elevation across a
    // marked rectangle. Where several removable things share the tile - a bin on a path, a fence
    // beside a tree - Delete lists them and lets the player choose, rather than guessing. The game
    // has no undo, so a wrong guess is unrecoverable; a list costs one keypress and cannot be wrong.
    //
    // Unlike D and X this deliberately ignores the focus elevation and offers everything on the tile.
    // The list is the disambiguator, so the player never has to get Home/End right before deleting.

    struct DeleteCandidate
    {
        enum class Kind : uint8_t
        {
            pathAddition,
            path,
            smallScenery,
            largeScenery,
            wall,
            banner,
            rideEntranceExit,
            track,
        };

        Kind kind{};
        std::string label;
        CoordsXYZD loc{};
        uint8_t quadrant = 0;                // small scenery
        ObjectEntryIndex entryIndex = 0;     // small scenery
        uint16_t sequence = 0;               // large scenery
        RideId rideId = RideId::GetNull();
        StationIndex station = StationIndex::GetNull();
        bool isExit = false;
        TrackElemType trackType{};
        int32_t trackSequence = 0;
    };

    // Removing a ride's track is the one entry that cannot simply be rebuilt, so it is confirmed
    // before it happens. Everything else in the list is cheap to put back.
    static bool DeleteNeedsConfirmation(const DeleteCandidate& c)
    {
        return c.kind == DeleteCandidate::Kind::track;
    }

    static std::vector<DeleteCandidate> GatherDeleteCandidates(const TileCoordsXY& tile)
    {
        std::vector<DeleteCandidate> out;
        const auto world = TileCoordsXYZ(tile.x, tile.y, 0).ToCoordsXYZ();

        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (el->isGhost())
            {
                if (el->isLastForTile())
                    break;
                el++;
                continue;
            }

            const CoordsXYZD loc{ world.x, world.y, el->getBaseZ(), el->getDirection() };

            if (auto* p = el->asPath(); p != nullptr)
            {
                // The addition is offered before the path carrying it: removing the path takes the bin
                // with it, so the narrower, less destructive choice should be the one reached first.
                if (p->HasAddition() && !p->AdditionIsGhost())
                {
                    DeleteCandidate c;
                    c.kind = DeleteCandidate::Kind::pathAddition;
                    c.loc = loc;
                    std::string name = GetObjectName(ObjectType::pathAdditions, p->GetAdditionEntryIndex());
                    c.label = name.empty() ? "Path addition" : name;
                    out.push_back(std::move(c));
                }

                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::path;
                c.loc = loc;
                std::string name = GetSpokenPathName(*p);
                c.label = name.empty() ? "Path" : name;
                out.push_back(std::move(c));
            }
            else if (auto* ss = el->asSmallScenery(); ss != nullptr)
            {
                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::smallScenery;
                c.loc = loc;
                c.quadrant = ss->GetSceneryQuadrant();
                c.entryIndex = ss->GetEntryIndex();
                std::string name = GetObjectName(ObjectType::smallScenery, ss->GetEntryIndex());
                c.label = name.empty() ? "Scenery" : name;
                out.push_back(std::move(c));
            }
            else if (auto* ls = el->asLargeScenery(); ls != nullptr)
            {
                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::largeScenery;
                c.loc = loc;
                c.sequence = ls->GetSequenceIndex();
                std::string name = GetObjectName(ObjectType::largeScenery, ls->GetEntryIndex());
                c.label = name.empty() ? "Large scenery" : name;
                out.push_back(std::move(c));
            }
            else if (auto* w = el->asWall(); w != nullptr)
            {
                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::wall;
                c.loc = loc;
                std::string name = GetObjectName(ObjectType::walls, w->GetEntryIndex());
                c.label = name.empty() ? "Wall" : name;
                out.push_back(std::move(c));
            }
            else if (auto* b = el->asBanner(); b != nullptr)
            {
                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::banner;
                // Banners are found by their edge position, not the element's direction - the remove
                // action matches on GetPosition(), so passing getDirection() would silently miss.
                c.loc = { world.x, world.y, el->getBaseZ(), b->GetPosition() };
                c.label = "Sign";
                out.push_back(std::move(c));
            }
            else if (auto* e = el->asEntrance(); e != nullptr)
            {
                // Park entrances are left out: they are removed through the park's own tools, and a
                // stray Delete on the gate would take the entrance off a working park.
                const uint8_t type = e->GetEntranceType();
                if (type == ENTRANCE_TYPE_RIDE_ENTRANCE || type == ENTRANCE_TYPE_RIDE_EXIT)
                {
                    DeleteCandidate c;
                    c.kind = DeleteCandidate::Kind::rideEntranceExit;
                    c.loc = loc;
                    c.rideId = e->GetRideIndex();
                    c.station = e->GetStationIndex();
                    c.isExit = (type == ENTRANCE_TYPE_RIDE_EXIT);
                    auto* ride = GetRide(c.rideId);
                    const std::string rideName = ride != nullptr ? std::string(ride->getName()) : std::string("Ride");
                    c.label = rideName + (c.isExit ? " exit" : " entrance");
                    out.push_back(std::move(c));
                }
            }
            else if (auto* t = el->asTrack(); t != nullptr)
            {
                DeleteCandidate c;
                c.kind = DeleteCandidate::Kind::track;
                c.loc = loc;
                c.trackType = t->GetTrackType();
                c.trackSequence = t->GetSequenceIndex();
                c.rideId = t->GetRideIndex();
                auto* ride = GetRide(c.rideId);
                const std::string rideName = ride != nullptr ? std::string(ride->getName()) : std::string("Ride");
                c.label = rideName + " track";
                out.push_back(std::move(c));
            }

            if (el->isLastForTile())
                break;
            el++;
        }

        // Match the tile readout's order, so the list arrives in the order the player just heard the
        // tile described. Elements are stored lowest-first, which is the default reading order.
        if (Config::Get().sound.accessibilityTileReadingOrder != 0)
            std::reverse(out.begin(), out.end());

        return out;
    }

    // Runs the removal for one candidate. Returns true if the game accepted it; failures are spoken
    // by the game's own error window, so nothing extra is said here.
    static bool ExecuteDelete(const DeleteCandidate& c)
    {
        auto& gs = getGameState();
        switch (c.kind)
        {
            case DeleteCandidate::Kind::pathAddition:
            {
                auto a = GameActions::FootpathAdditionRemoveAction(CoordsXYZ{ c.loc.x, c.loc.y, c.loc.z });
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::path:
            {
                auto a = GameActions::FootpathRemoveAction(CoordsXYZ{ c.loc.x, c.loc.y, c.loc.z });
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::smallScenery:
            {
                auto a = GameActions::SmallSceneryRemoveAction(
                    CoordsXYZ{ c.loc.x, c.loc.y, c.loc.z }, c.quadrant, c.entryIndex);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::largeScenery:
            {
                auto a = GameActions::LargeSceneryRemoveAction(c.loc, c.sequence);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::wall:
            {
                auto a = GameActions::WallRemoveAction(c.loc);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::banner:
            {
                auto a = GameActions::BannerRemoveAction(c.loc);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::rideEntranceExit:
            {
                auto a = GameActions::RideEntranceExitRemoveAction(
                    CoordsXY{ c.loc.x, c.loc.y }, c.rideId, c.station, c.isExit);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
            case DeleteCandidate::Kind::track:
            {
                auto a = GameActions::TrackRemoveAction(c.trackType, c.trackSequence, c.loc);
                return GameActions::Execute(&a, gs).error == GameActions::Status::ok;
            }
        }
        return false;
    }

    // Non-empty exactly while the picker owns the keyboard.
    static std::vector<DeleteCandidate> _deleteCandidates;
    static int32_t _deleteIndex = 0;
    static bool _deleteConfirming = false;   // showing the yes/no guard on a track entry
    static int32_t _deleteConfirmChoice = 1; // 0 = Yes, 1 = No; starts on No so a stray Enter is safe

    static bool IsDeletePickerActive()
    {
        return !_deleteCandidates.empty();
    }

    static void CloseDeletePicker()
    {
        _deleteCandidates.clear();
        _deleteIndex = 0;
        _deleteConfirming = false;
    }

    static void SpeakDeleteChoice()
    {
        if (_deleteIndex < 0 || _deleteIndex >= static_cast<int32_t>(_deleteCandidates.size()))
            return;
        ScreenReaderSpeakItem(
            _deleteCandidates[_deleteIndex].label, _deleteIndex, static_cast<int32_t>(_deleteCandidates.size()));
    }

    static void PerformDelete(const DeleteCandidate& candidate)
    {
        // Copy the label first: the candidate may reference state the removal invalidates.
        const std::string label = candidate.label;
        if (ExecuteDelete(candidate))
        {
            _lastTileDescription.clear();
            ScreenReaderSpeak(label + " removed");
        }
        // A refusal already opens the game's own error window, which speaks itself; saying anything
        // more here would talk over it.
    }

    static void BeginDeleteConfirm(size_t index)
    {
        _deleteIndex = static_cast<int32_t>(index);
        _deleteConfirming = true;
        _deleteConfirmChoice = 1; // default No
        ScreenReaderSpeak(
            "Delete " + _deleteCandidates[index].label
            + "? This cannot be undone. Up and down to choose, Enter to confirm, Escape to cancel. No.");
    }

    // Delete on the free map cursor.
    static void DeleteAtCursor()
    {
        if (!_initialised)
            InitialiseCursor();

        auto candidates = GatherDeleteCandidates(_cursor);
        if (candidates.empty())
        {
            ScreenReaderSpeak("Nothing here to remove");
            return;
        }

        if (candidates.size() == 1 && !DeleteNeedsConfirmation(candidates[0]))
        {
            // Nothing to disambiguate, so do not make the player answer a question about it.
            PerformDelete(candidates[0]);
            return;
        }

        _deleteCandidates = std::move(candidates);
        if (_deleteCandidates.size() == 1)
        {
            // A lone track piece still confirms: that guard is about how costly the mistake is, not
            // about which thing was meant.
            BeginDeleteConfirm(0);
            return;
        }

        _deleteIndex = 0;
        _deleteConfirming = false;
        ScreenReaderSpeak(
            std::to_string(_deleteCandidates.size())
            + " things here. Up and down to choose, Enter to delete, Escape to cancel.");
        SpeakDeleteChoice();
    }

    // The picker owns the keyboard while open, so every key is answered here.
    static bool HandleDeletePickerKey(uint32_t key)
    {
        const int32_t count = static_cast<int32_t>(_deleteCandidates.size());
        switch (key)
        {
            case SDLK_ESCAPE:
                CloseDeletePicker();
                ScreenReaderSpeak("Cancelled");
                return true;

            case SDLK_UP:
            case SDLK_LEFT:
            case SDLK_DOWN:
            case SDLK_RIGHT:
            {
                const int32_t delta = (key == SDLK_UP || key == SDLK_LEFT) ? -1 : 1;
                if (_deleteConfirming)
                {
                    _deleteConfirmChoice = ((_deleteConfirmChoice + delta) % 2 + 2) % 2;
                    ScreenReaderSpeak(_deleteConfirmChoice == 0 ? "Yes" : "No");
                }
                else
                {
                    _deleteIndex = ListNav::wrap(_deleteIndex, delta, count);
                    SpeakDeleteChoice();
                }
                return true;
            }

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            {
                if (_deleteIndex < 0 || _deleteIndex >= count)
                {
                    CloseDeletePicker();
                    return true;
                }
                // Taken by value: closing the picker destroys the vector it lives in.
                const DeleteCandidate chosen = _deleteCandidates[_deleteIndex];

                if (_deleteConfirming)
                {
                    const bool confirmed = (_deleteConfirmChoice == 0);
                    CloseDeletePicker();
                    if (confirmed)
                        PerformDelete(chosen);
                    else
                        ScreenReaderSpeak("Cancelled");
                    return true;
                }

                if (DeleteNeedsConfirmation(chosen))
                {
                    BeginDeleteConfirm(static_cast<size_t>(_deleteIndex));
                    return true;
                }

                CloseDeletePicker();
                PerformDelete(chosen);
                return true;
            }

            default:
                return true; // modal: swallow everything else until the player chooses
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
        // Uses the same facing the sloped-path build uses, named with the shared compass, so the
        // spoken "Facing X" and the direction a slope rises can never disagree.
        ScreenReaderSpeak(std::string("Facing ") + GetWorldDirectionName(CameraFacingDirection()));
    }

    // The ride whose footprint the cursor is within: first the ride whose track sits on the cursor
    // tile (the common case, when the player has arrowed onto the structure), otherwise - for a
    // cursor inside a ride's area but not on a track tile, e.g. the empty middle of a loop - the
    // smallest-area ride whose tile bounding box contains the cursor. Null when the cursor is not in
    // any ride. The bounding-box fallback does a single map scan and only runs when the cursor is not
    // directly on track, so the usual case stays cheap.
    static RideId RideContainingCursor()
    {
        if (const RideId onTile = GetRideAtTile(_cursor); !onTile.IsNull())
            return onTile;

        struct Bounds
        {
            int32_t minX, minY, maxX, maxY;
        };
        std::unordered_map<uint16_t, Bounds> bounds;
        const auto mapSize = getGameState().mapSize;
        for (int32_t y = 1; y <= mapSize.y - 2; y++)
        {
            for (int32_t x = 1; x <= mapSize.x - 2; x++)
            {
                const RideId rid = GetRideAtTile(TileCoordsXY{ x, y });
                if (rid.IsNull())
                    continue;
                const uint16_t key = rid.ToUnderlying();
                auto it = bounds.find(key);
                if (it == bounds.end())
                    bounds.emplace(key, Bounds{ x, y, x, y });
                else
                {
                    it->second.minX = std::min(it->second.minX, x);
                    it->second.minY = std::min(it->second.minY, y);
                    it->second.maxX = std::max(it->second.maxX, x);
                    it->second.maxY = std::max(it->second.maxY, y);
                }
            }
        }

        RideId best = RideId::GetNull();
        int64_t bestArea = std::numeric_limits<int64_t>::max();
        for (const auto& [key, b] : bounds)
        {
            if (_cursor.x < b.minX || _cursor.x > b.maxX || _cursor.y < b.minY || _cursor.y > b.maxY)
                continue;
            const int64_t area = static_cast<int64_t>(b.maxX - b.minX + 1) * (b.maxY - b.minY + 1);
            if (area < bestArea)
            {
                bestArea = area;
                best = RideId::FromUnderlying(key);
            }
        }
        return best;
    }

    // Ctrl+E: when the cursor is within a ride's area, jump the cursor between that ride's entrance
    // and exit (cycling through all of them for a multi-station ride). Pre-built coasters make it hard
    // to find where the entrance and exit sit, so this snaps straight to them. Repeated presses step
    // to the next entrance/exit; from anywhere else in the ride it lands on the first one.
    static void JumpRideEntranceExit()
    {
        if (!_initialised)
            InitialiseCursor();

        struct Target
        {
            TileCoordsXY tile;
            std::string label;
        };
        std::vector<Target> targets;

        // A pre-built design frozen for inspection has no ride in the game state yet, so read the
        // entrance and exit tiles the design will place instead. This is the point at which the
        // player most needs them: the ride is positioned but not yet built, so finding an entrance
        // sitting somewhere unreachable is still free to fix with Backspace.
        auto pending = Windows::WindowTrackPlaceEntranceExitTiles();
        if (!pending.empty())
        {
            for (const auto& [tile, label] : pending)
                targets.push_back({ TileCoordsXY{ tile }, label });
        }
        else if (Windows::WindowTrackPlaceIsActive())
        {
            // The design is still following the cursor, so its entrance and exit have no fixed
            // ground to sit on yet.
            ScreenReaderSpeak("Press Enter to position the ride first, then Control plus E finds its entrance and exit");
            return;
        }
        else
        {
            const RideId rideId = RideContainingCursor();
            auto ride = rideId.IsNull() ? nullptr : GetRide(rideId);
            if (ride == nullptr)
            {
                ScreenReaderSpeak("Not on a ride");
                return;
            }

            for (const auto& station : ride->getStations())
            {
                if (!station.Entrance.IsNull())
                    targets.push_back({ TileCoordsXY{ station.Entrance.x, station.Entrance.y }, "Ride entrance" });
                if (!station.Exit.IsNull())
                    targets.push_back({ TileCoordsXY{ station.Exit.x, station.Exit.y }, "Ride exit" });
            }
        }

        if (targets.empty())
        {
            ScreenReaderSpeak("This ride has no entrance or exit");
            return;
        }

        // If the cursor already sits on one of the targets, advance to the next (wrapping); otherwise
        // start at the first. This makes repeated presses toggle entrance -> exit -> entrance.
        size_t next = 0;
        for (size_t i = 0; i < targets.size(); i++)
        {
            if (targets[i].tile.x == _cursor.x && targets[i].tile.y == _cursor.y)
            {
                next = (i + 1) % targets.size();
                break;
            }
        }

        const auto& target = targets[next];
        _cursor = target.tile;
        _menuMode = false;
        CentreViewportOnCursor();

        // Same bookkeeping every deliberate jump does (see the marker and waypoint jumps): sound the
        // elevation tone so the landing is audible, and re-seat the focus elevation on the ground so
        // C reports where the cursor now is rather than the height it left behind.
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            SoundElevationOnChange(_cursor);
            _scanHeight = surface->baseHeight;
            _scanLocked = false;
        }

        _lastTileDescription = GetTileDescription(_cursor);
        ScreenReaderSpeak(target.label + ", " + SpokenTileCoordsText(_cursor));
    }

    // What Ctrl+arrows jump between. Ctrl+Shift+Up/Down cycles this. It starts on rides and stalls,
    // which is what the jump always did before the filter existed, so the keys behave as before until
    // the player deliberately changes category.
    enum class JumpCategory : uint8_t
    {
        ridesAndStalls,
        scenery,
        footpathObjects,
        hazards,
    };
    static constexpr int32_t kJumpCategoryCount = 4;
    static JumpCategory _jumpCategory = JumpCategory::ridesAndStalls;

    // Spoken name of a category, announced when cycling.
    static const char* JumpCategoryName(JumpCategory category)
    {
        switch (category)
        {
            case JumpCategory::scenery:
                return "Scenery";
            case JumpCategory::footpathObjects:
                return "Footpath objects";
            case JumpCategory::hazards:
                return "Hazards";
            case JumpCategory::ridesAndStalls:
                break;
        }
        return "Rides and stalls";
    }

    // How a category is named inside "No <this> to the north", which wants lowercase and plural.
    static const char* JumpCategoryPluralName(JumpCategory category)
    {
        switch (category)
        {
            case JumpCategory::scenery:
                return "scenery";
            case JumpCategory::footpathObjects:
                return "footpath objects";
            case JumpCategory::hazards:
                return "hazards";
            case JumpCategory::ridesAndStalls:
                break;
        }
        return "rides";
    }

    // True if this tile holds something in the given category.
    //
    // Rides are the exception: the caller tests them by ride identity rather than through here, so a
    // multi-tile coaster counts as one target instead of a run of tiles. Nothing in the other three
    // categories spans tiles that way - a tree, a bench and a pile of vomit each occupy their own
    // tile - so for those a per-tile test is what "the next one" actually means.
    static bool TileMatchesJumpCategory(const TileCoordsXY& tile, JumpCategory category)
    {
        const auto coords = tile.ToCoordsXY();
        switch (category)
        {
            case JumpCategory::scenery:
            {
                // Trees, flowers, shrubs and statues are small scenery; gazebos and the larger props
                // are large scenery. Both are "scenery" to a player, so both count.
                for (auto* el : TileElementsView<SmallSceneryElement>(coords))
                {
                    if (!el->isGhost())
                        return true;
                }
                for (auto* el : TileElementsView<LargeSceneryElement>(coords))
                {
                    if (!el->isGhost())
                        return true;
                }
                return false;
            }

            case JumpCategory::footpathObjects:
            {
                // Bins, benches, lamps and fountains are all path additions hanging off the path
                // element rather than objects in their own right.
                for (auto* pathEl : TileElementsView<PathElement>(coords))
                {
                    if (!pathEl->isGhost() && pathEl->HasAddition() && !pathEl->AdditionIsGhost())
                        return true;
                }
                return false;
            }

            case JumpCategory::hazards:
            {
                // Dropped litter of every kind - vomit, wrappers, cans, cups - plus vandalised path
                // additions, which are the broken benches, bins and lamps a handyman has to repair.
                // These are the things that quietly drag a park rating down, and the whole point of
                // this category is finding them without sweeping the map tile by tile.
                for ([[maybe_unused]] auto* litter : EntityTileList<Litter>(coords))
                    return true;
                for (auto* pathEl : TileElementsView<PathElement>(coords))
                {
                    if (!pathEl->isGhost() && pathEl->HasAddition() && !pathEl->AdditionIsGhost()
                        && pathEl->IsBroken())
                        return true;
                }
                return false;
            }

            case JumpCategory::ridesAndStalls:
                break;
        }
        return !GetRideAtTile(tile).IsNull();
    }

    // Ctrl+Shift+Up/Down: choose what Ctrl+arrows jump between, wrapping at both ends so either key
    // reaches every category.
    static void CycleJumpCategory(int32_t delta)
    {
        int32_t idx = static_cast<int32_t>(_jumpCategory) + delta;
        idx = ((idx % kJumpCategoryCount) + kJumpCategoryCount) % kJumpCategoryCount;
        _jumpCategory = static_cast<JumpCategory>(idx);
        ScreenReaderSpeak(std::string("Jump filter, ") + JumpCategoryName(_jumpCategory));
    }

    // Jumps the cursor to the nearest target of the current filter category in the pressed screen
    // direction (Ctrl+arrow). Only
    // targets within a 90-degree cone of that direction are considered - so "right" ignores something
    // that is mostly north of you - and among those the closest is chosen. On rides, the ride the
    // cursor is already on is skipped entirely, so a multi-tile ride counts as one target rather than
    // a run of tiles; on the other categories the cursor's own tile is skipped instead. Announces the
    // tile landed on, or the compass direction when the category has nothing that way.
    static void JumpToNearest(uint32_t key)
    {
        if (!_initialised)
            InitialiseCursor();

        // Screen-direction base delta, matching the arrow keys (see HandleMapCursorKey / MoveScreen).
        int32_t bdx = 0, bdy = 0;
        switch (key)
        {
            case SDLK_UP:
                bdx = 0; bdy = -1;
                break;
            case SDLK_DOWN:
                bdx = 0; bdy = 1;
                break;
            case SDLK_RIGHT:
                bdx = -1; bdy = 0;
                break;
            case SDLK_LEFT:
                bdx = 1; bdy = 0;
                break;
            default:
                return;
        }
        // Rotate the screen delta into a world-axis unit vector for the current camera rotation.
        int32_t wdx = bdx, wdy = bdy;
        for (int32_t i = 0, steps = GetCurrentRotation() & 3; i < steps; i++)
        {
            const int32_t nx = wdy;
            const int32_t ny = -wdx;
            wdx = nx;
            wdy = ny;
        }

        // The compass direction that world delta actually points, so the announcement names true
        // north/east/south/west and updates as the camera is rotated (world dir 0=-x=East, 1=+y=South,
        // 2=+x=West, 3=-y=North; see GetWorldDirectionName in Direction.h).
        Direction worldDir = 3; // north (wdy < 0)
        if (wdx < 0)
            worldDir = 0; // east
        else if (wdy > 0)
            worldDir = 1; // south
        else if (wdx > 0)
            worldDir = 2; // west

        const bool byRide = (_jumpCategory == JumpCategory::ridesAndStalls);

        // Skip the ride the cursor is currently on so the whole ride counts as one, not tile by tile.
        const RideId currentRide = byRide ? GetRideAtTile(_cursor) : RideId::GetNull();

        const auto mapSize = getGameState().mapSize;
        bool found = false;
        TileCoordsXY bestTile{};
        int64_t bestDist = std::numeric_limits<int64_t>::max();

        for (int32_t y = 1; y <= mapSize.y - 2; y++)
        {
            for (int32_t x = 1; x <= mapSize.x - 2; x++)
            {
                const TileCoordsXY candidate{ x, y };
                if (byRide)
                {
                    const RideId rid = GetRideAtTile(candidate);
                    if (rid.IsNull() || rid == currentRide)
                        continue;
                }
                else
                {
                    if (x == _cursor.x && y == _cursor.y)
                        continue; // the tile we are standing on is not "the next one"
                    if (!TileMatchesJumpCategory(candidate, _jumpCategory))
                        continue;
                }
                const int64_t dispx = x - _cursor.x;
                const int64_t dispy = y - _cursor.y;
                const int64_t along = dispx * wdx + dispy * wdy; // distance in the pressed direction
                if (along <= 0)
                    continue; // behind or level with the cursor
                const int64_t cross = dispx * wdy - dispy * wdx; // sideways offset
                if ((cross < 0 ? -cross : cross) > along)
                    continue; // outside the 90-degree cone
                const int64_t dist = dispx * dispx + dispy * dispy;
                if (dist < bestDist)
                {
                    bestDist = dist;
                    found = true;
                    bestTile = candidate;
                }
            }
        }

        if (!found)
        {
            ScreenReaderSpeak(
                std::string("No ") + JumpCategoryPluralName(_jumpCategory) + " to the "
                + GetWorldDirectionName(worldDir));
            return;
        }

        _cursor = bestTile;
        _menuMode = false;
        CentreViewportOnCursor();

        // Same bookkeeping every deliberate jump does (see the marker and waypoint jumps): sound the
        // elevation tone so the landing is audible, and re-seat the focus elevation on the ground so
        // C - and anything built here - refers to where the cursor now is rather than the height it
        // left behind. This jump was the one place that skipped it.
        if (auto* surface = MapGetSurfaceElementAt(_cursor); surface != nullptr)
        {
            SoundElevationOnChange(_cursor);
            _scanHeight = surface->baseHeight;
            _scanLocked = false;
        }

        // The description alone does not say where you have landed, and a jump can cross the whole
        // map - so read the coordinates after it, the same way the marker and waypoint jumps do.
        // _lastTileDescription keeps the bare description: it is what the "announce on change" mode
        // compares against, and coordinates differ on every tile, so folding them in would make every
        // tile look like a change and defeat that setting.
        _lastTileDescription = GetTileDescription(_cursor);
        ScreenReaderSpeak(_lastTileDescription + ", " + SpokenTileCoordsText(_cursor));
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
            const std::string height = gap.element != nullptr ? ElevationText(ElevationHalfSteps(gap.element->getBaseZ()))
                                                              : std::string("unknown");
            ScreenReaderSpeak("Track break at " + SpokenTileCoordsText(tile) + ", height " + height);
        }
        else
        {
            ScreenReaderSpeak("Track is a complete circuit, no breaks");
        }
    }

    // A height difference spoken as a number of path/land steps: "half a step", "1 step",
    // "1 step and a half". Input is a magnitude in half steps.
    static std::string StepsText(int32_t halfSteps)
    {
        const int32_t whole = halfSteps / 2;
        const bool half = (halfSteps % 2) != 0;
        std::string text;
        if (whole > 0)
            text = std::to_string(whole) + (whole == 1 ? " step" : " steps");
        if (half)
            text += text.empty() ? "half a step" : " and a half";
        return text;
    }

    // Why a path network stops at this tile, when the reason is a height mismatch: there IS a path
    // on the next tile, but at a level the engine will not link to. Half-step mismatches are the
    // nastiest kind - a path lifted off the step grid looks continuous and, before the elevation
    // readout could say ".5", sounded continuous too. Returns an empty string when the
    // neighbouring tiles simply have no path (an ordinary missing-tile gap, which needs no
    // explanation) so the caller can just append it.
    static std::string DescribePathGapReason(const TileCoordsXY& tile)
    {
        int32_t bestDiff = 0;
        TileCoordsXY bestTile{};
        bool found = false;

        for (auto* here : TileElementsView<PathElement>(tile.ToCoordsXY()))
        {
            if (here->isGhost())
                continue;
            for (Direction dir : kAllDirections)
            {
                const TileCoordsXY neighbour{ tile.x + TileDirectionDelta[dir].x, tile.y + TileDirectionDelta[dir].y };
                for (auto* other : TileElementsView<PathElement>(neighbour.ToCoordsXY()))
                {
                    if (other->isGhost())
                        continue;

                    // Skip pairs that genuinely connect - a ramp meeting a landing sits two half
                    // steps apart and is perfectly fine, so a raw height difference proves nothing.
                    int32_t arrivalZ = here->baseHeight;
                    if (here->IsSloped() && here->GetSlopeDirection() == dir)
                        arrivalZ += 2;
                    if ((here->GetEdges() & (1 << dir)) != 0 && FootpathIsZAndDirectionValid(*other, arrivalZ, dir))
                        continue;

                    const int32_t diff = other->baseHeight - here->baseHeight;
                    if (diff == 0)
                        continue; // same level but not linked: a missing edge, not a height problem
                    if (!found || std::abs(diff) < std::abs(bestDiff))
                    {
                        bestDiff = diff;
                        bestTile = neighbour;
                        found = true;
                    }
                }
            }
        }

        if (!found)
            return {};

        return ". The path at " + SpokenTileCoordsText(bestTile) + " sits " + StepsText(std::abs(bestDiff))
            + (bestDiff > 0 ? " higher" : " lower") + ", so the two cannot join";
    }

    // Opens a game window by class via the game's own window-open path and announces its name, so a
    // blind player hears what just opened. Used by the Shift + letter window shortcuts.
    static void OpenGameWindow(WindowClass wc, const char* name)
    {
        ContextOpenWindow(wc);
        ScreenReaderSpeak(name);
    }

    // The ride or stall occupying the cursor tile, found from a track piece or a ride entrance/exit
    // on the tile (stalls are track elements too). Null if the tile holds no ride.
    static RideId RideIdAtCursor()
    {
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (auto* track = el->asTrack(); track != nullptr)
                return track->GetRideIndex();
            if (auto* entrance = el->asEntrance(); entrance != nullptr)
            {
                const auto type = entrance->GetEntranceType();
                if (type == ENTRANCE_TYPE_RIDE_ENTRANCE || type == ENTRANCE_TYPE_RIDE_EXIT)
                    return entrance->GetRideIndex();
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return RideId::GetNull();
    }

    // True if the cursor tile holds the park entrance gate.
    static bool CursorOnParkEntrance()
    {
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (auto* entrance = el->asEntrance();
                entrance != nullptr && entrance->GetEntranceType() == ENTRANCE_TYPE_PARK_ENTRANCE)
                return true;
            if (el->isLastForTile())
                break;
            el++;
        }
        return false;
    }

    // Opens the edit window for a banner or sign on the cursor tile, mirroring the click behaviour
    // (banners, wall signs, and large-scenery signs each open their detail window). Returns true if
    // one was found and opened.
    static bool OpenBannerOrSignAtCursor()
    {
        for (TileElement* el = MapGetFirstElementAt(_cursor); el != nullptr;)
        {
            if (auto* banner = el->asBanner(); banner != nullptr)
            {
                ContextOpenDetailWindow(WindowDetail::banner, banner->GetIndex().ToUnderlying());
                ScreenReaderSpeak("Banner");
                return true;
            }
            if (auto* wall = el->asWall(); wall != nullptr && !wall->GetBannerIndex().IsNull())
            {
                ContextOpenDetailWindow(WindowDetail::signSmall, wall->GetBannerIndex().ToUnderlying());
                ScreenReaderSpeak("Sign");
                return true;
            }
            if (auto* large = el->asLargeScenery(); large != nullptr && !large->GetBannerIndex().IsNull())
            {
                ContextOpenDetailWindow(WindowDetail::sign, large->GetBannerIndex().ToUnderlying());
                ScreenReaderSpeak("Sign");
                return true;
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return false;
    }

    // Enter on the map cursor: open the information window of the ride/stall under the cursor, the
    // edit window of a banner/sign, or park information for the park entrance gate. Announces when
    // there is nothing to open.
    static void OpenRideOrGateInfoAtCursor()
    {
        if (!_initialised)
            InitialiseCursor();

        const RideId rid = RideIdAtCursor();
        if (!rid.IsNull())
        {
            auto* ride = GetRide(rid);
            auto intent = Intent(WindowClass::ride);
            intent.PutExtra(INTENT_EXTRA_RIDE_ID, rid.ToUnderlying());
            ContextOpenIntent(&intent);
            ScreenReaderSpeak(ride != nullptr ? std::string(ride->getName()) : std::string("Ride"));
            return;
        }
        if (OpenBannerOrSignAtCursor())
            return;
        if (CursorOnParkEntrance())
        {
            OpenGameWindow(WindowClass::parkInformation, "Park information");
            return;
        }
        ScreenReaderSpeak("No ride, sign, or gate here");
    }

    // Ctrl+Enter on the map cursor: open the ride/stall under the cursor in construction mode, using
    // the game's own construction window so all the built-in build tools work.
    static void EnterRideConstructionAtCursor()
    {
        if (!_initialised)
            InitialiseCursor();

        const RideId rid = RideIdAtCursor();
        auto* ride = rid.IsNull() ? nullptr : GetRide(rid);
        if (ride == nullptr)
        {
            ScreenReaderSpeak("No ride here to build on");
            return;
        }
        // A newly created construction window announces itself from its own onOpen, which covers
        // every route into build mode. Only speak here when that window is already up for this ride,
        // since then no window is created and no onOpen will fire.
        auto* windowMgr = GetWindowManager();
        auto* existing = windowMgr != nullptr ? windowMgr->FindByClass(WindowClass::rideConstruction) : nullptr;
        const bool reusingWindow = existing != nullptr && existing->number == static_cast<int16_t>(rid.ToUnderlying());

        RideInitialiseConstructionWindow(*ride);

        if (reusingWindow)
            ScreenReaderSpeak("Build mode. " + std::string(ride->getName()));
    }

    // Shift+Page Up/Down: zoom the main view (matching the game's own zoom) and announce the level.
    static void ZoomView(bool zoomIn)
    {
        Windows::MainWindowZoom(zoomIn, false);
        int32_t level = 0;
        if (auto* w = WindowGetMain(); w != nullptr && w->viewport != nullptr)
            level = static_cast<int32_t>(static_cast<int8_t>(w->viewport->zoom));
        ScreenReaderSpeak("Zoom level " + std::to_string(level));
    }

    static bool HandleMapCursorKey(uint32_t key, uint32_t modifiers)
    {
        // The mod uses no Alt-modified keys, so let any Alt combination fall through to the game's
        // own shortcuts (e.g. Ctrl+Alt+C opens the Cheats window). Without this, a bare-letter map
        // command like 'c' (read coordinates) would swallow the letter before the shortcut fires.
        if (modifiers & KMOD_ALT)
            return false;

        // During pre-built ride placement, dedicated keys rotate / build / cancel the design. Enter is
        // two-stage: the first freezes a preview at the cursor, the second builds it; Backspace picks a
        // frozen preview back up. Arrow keys and the rest fall through so the map cursor still positions
        // the ride (and can trace the frozen preview's footprint).
        if (Windows::WindowTrackPlaceIsActive())
        {
            switch (key)
            {
                case SDLK_r:
                {
                    Windows::WindowTrackPlaceRotate();
                    // Refresh the ghost so the rotated outline shows immediately, without waiting for a move.
                    const auto rWorld = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    Windows::WindowTrackPlaceUpdateGhost(CoordsXY{ rWorld.x, rWorld.y });
                    return true;
                }
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                {
                    if (!_initialised)
                        InitialiseCursor();
                    const auto world = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    Windows::WindowTrackPlaceAtTile(CoordsXY{ world.x, world.y });
                    return true;
                }
                case SDLK_BACKSPACE:
                {
                    Windows::WindowTrackPlacePickup();
                    // The preview is unfrozen; put the ghost back under the cursor right away.
                    const auto bWorld = TileCoordsXYZ(_cursor.x, _cursor.y, 0).ToCoordsXYZ();
                    Windows::WindowTrackPlaceUpdateGhost(CoordsXY{ bWorld.x, bWorld.y });
                    return true;
                }
                case SDLK_ESCAPE:
                    Windows::WindowTrackPlaceCancel();
                    return true;
            }
        }

        // Shop/stall/flat-ride placement: cursor positions the footprint, R rotates. Enter is
        // two-stage - the first freezes a preview the player can arrow around to inspect, the second
        // builds it; Backspace picks a frozen preview back up to reposition. Escape cancels. Arrows
        // fall through to move the cursor (so the preview can be explored).
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
                case SDLK_BACKSPACE:
                    AccessibleRidePlacementPickup();
                    return true;
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

        // Shift + a letter opens the game window that the bare letter used to open (the bare letters
        // now drive mod functions on the map cursor). We open the window here, via the game's own
        // window-open path, so it works regardless of the player's saved shortcut config.
        if (modifiers & KMOD_SHIFT)
        {
            switch (key)
            {
                case SDLK_c:
                    OpenGameWindow(WindowClass::constructRide, "Construct a new ride");
                    return true;
                case SDLK_f:
                    OpenGameWindow(WindowClass::finances, "Finances");
                    return true;
                case SDLK_m:
                    OpenGameWindow(WindowClass::recentNews, "Recent messages");
                    return true;
                case SDLK_d:
                    ContextOpenWindowView(WindowView::rideResearch);
                    ScreenReaderSpeak("Research");
                    return true;
                case SDLK_r:
                    OpenGameWindow(WindowClass::rideList, "Rides");
                    return true;
                case SDLK_p:
                    OpenGameWindow(WindowClass::parkInformation, "Park information");
                    return true;
                case SDLK_g:
                    OpenGameWindow(WindowClass::guestList, "Guest list");
                    return true;
                case SDLK_s:
                    OpenGameWindow(WindowClass::staffList, "Staff");
                    return true;
            }
        }

        // Enter opens the information window of the ride/stall/gate under the cursor. Shift+Enter
        // rotates the camera (like Shift+arrows); Ctrl+Enter opens the ride/stall in construction
        // mode. Placement modes handle Enter themselves above, so this only runs on the free cursor.
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
        {
            if (!_initialised)
                InitialiseCursor();
            if (modifiers & KMOD_SHIFT)
            {
                ViewportRotateAll(1);
                ReportFacing();
            }
            else if (modifiers & KMOD_CTRL)
                EnterRideConstructionAtCursor();
            else
                OpenRideOrGateInfoAtCursor();
            return true;
        }

        // Number keys: Shift + number drops/moves the waypoint in that slot at the cursor; Ctrl +
        // number warps to it. Slots are 1-9 then 0 (the tenth). A plain number is left for the game's
        // own view-toggle shortcut, so we don't consume it. (SDL reports the digit keycode with the
        // modifier flag set, so all three share the key.)
        {
            int32_t slot = -1;
            if (key >= SDLK_1 && key <= SDLK_9)
                slot = static_cast<int32_t>(key - SDLK_1);
            else if (key == SDLK_0)
                slot = kWaypointCount - 1;

            if (slot >= 0)
            {
                if (modifiers & KMOD_SHIFT)
                {
                    if (!_initialised)
                        InitialiseCursor();
                    SetWaypoint(slot);
                    return true;
                }
                if (modifiers & KMOD_CTRL)
                {
                    if (!_initialised)
                        InitialiseCursor();
                    JumpToWaypoint(slot);
                    return true;
                }
                return false; // plain digit: let the game's view-toggle shortcut handle it
            }
        }

        if (key != SDLK_UP && key != SDLK_DOWN && key != SDLK_LEFT && key != SDLK_RIGHT && key != SDLK_c
            && key != SDLK_t && key != SDLK_m && key != SDLK_SPACE && key != SDLK_d && key != SDLK_e
            && key != SDLK_f && key != SDLK_LEFTBRACKET && key != SDLK_RIGHTBRACKET
            && key != SDLK_x && key != SDLK_b && key != SDLK_o && key != SDLK_l
            && key != SDLK_k && key != SDLK_HOME && key != SDLK_END && key != SDLK_PAGEUP
            && key != SDLK_PAGEDOWN && key != SDLK_DELETE)
            return false;

        if (!_initialised)
            InitialiseCursor();

        switch (key)
        {
            case SDLK_c:
                ReadCoordinates();
                break;
            case SDLK_t:
                // Open the bottom-toolbar status readout: date, park rating, guests, cash, and
                // recent messages. Arrow to cycle, Enter opens messages, Escape (or T) closes.
                EnterStatusMode();
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
            case SDLK_x:
                ClearSceneryAtCursor();
                break;
            case SDLK_DELETE:
                // Removes one thing at the cursor, asking which when the tile holds several. Skipped
                // while a placement mode owns the cursor: there Delete would act on the map beneath
                // the thing being positioned, which is never what is meant.
                if (!Windows::WindowTrackPlaceIsActive() && !IsAccessibleRidePlacementActive()
                    && !IsAccessibleSceneryPlacementActive())
                    DeleteAtCursor();
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
                // Plain raises land, Ctrl raises water, Shift zooms out (matching the game's
                // Page Up = zoom out) and announces the level.
                if (modifiers & KMOD_SHIFT)
                    ZoomView(false);
                else if (modifiers & KMOD_CTRL)
                    ChangeWaterHeight(true);
                else
                    ChangeLandHeight(true);
                break;
            case SDLK_PAGEDOWN:
                if (modifiers & KMOD_SHIFT)
                    ZoomView(true);
                else if (modifiers & KMOD_CTRL)
                    ChangeWaterHeight(false);
                else
                    ChangeLandHeight(false);
                break;
            case SDLK_l:
                CycleSlopeMode();
                break;
            case SDLK_END:
                // End lowers, Home raises the cursor's focus elevation by one step (the single
                // elevation used for building, deletion and the coordinate readout). With Shift, snap
                // the focus to the next element below (End) or above (Home) and read it.
                if (modifiers & KMOD_SHIFT)
                    ScanZLevel(-1);
                else
                    ChangeFocusElevation(-1);
                break;
            case SDLK_HOME:
                if (modifiers & KMOD_SHIFT)
                    ScanZLevel(1);
                else
                    ChangeFocusElevation(1);
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

    // Ride construction keyboard menu. While the construction window is open, ALT+arrows operate the
    // build menu (field navigation and value changes), Ctrl+Enter activates the focused item, Ctrl+B
    // reads the build state, and Escape exits (with confirmation). Plain arrows are NOT consumed
    // here, so the map tile cursor keeps working normally during construction. Returns true if the
    // key was consumed. Translated keys are forwarded to the window's onAccessibilityAction.
    //
    // The arrows moved off Ctrl because Ctrl+arrows already mean "jump the map cursor to the nearest
    // ride in that direction" - opening the builder used to swallow that shortcut for as long as it
    // stayed open. Alt is free: the game binds no Alt+arrow at all (its arrow shortcuts are bare or
    // Shift-modified), and shortcut matching compares the exact modifier set, so a held Alt cannot
    // fall through to the bare map-scroll bindings either.
    //
    // Enter and B stay on Ctrl deliberately. Alt+Enter is the game's windowed-mode toggle, and since
    // this handler runs before the shortcut manager, claiming it would break that toggle for as long
    // as the builder was open. Ctrl+Enter and Ctrl+B collide with nothing in this context.
    static bool HandleRideConstructionAccessKey(uint32_t key, uint32_t modifiers)
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return false;
        auto* w = windowMgr->FindByClass(WindowClass::rideConstruction);
        if (w == nullptr)
            return false;

        // Delete and Insert act at the MAP CURSOR rather than at the build focus, so they are
        // handled here instead of as AccessibilityActions: the cursor's tile and focus height live
        // in this file and the window has no way to ask for them. Delete removes the piece under the
        // cursor - any piece in the ride, not just the last one placed - and Insert moves the build
        // focus to the track under the cursor.
        if ((modifiers & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT)) == 0 && (key == SDLK_DELETE || key == SDLK_INSERT))
        {
            const auto pos = TileCoordsXYZ(_cursor.x, _cursor.y, _scanHeight).ToCoordsXYZ();
            if (key == SDLK_DELETE)
                Windows::WindowRideConstructionAccessDeleteAt(pos);
            else
                Windows::WindowRideConstructionAccessFocusAtCursor(pos);
            return true;
        }

        // The whole build menu sits on Ctrl, arrows included. The consequence is that Ctrl+arrows
        // cannot also jump the cursor to a nearby ride while the builder is open - in build mode the
        // menu takes those keys. This handler runs before the game's shortcuts, so the jump simply
        // does not fire here; it works again as soon as the builder is closed.
        std::optional<AccessibilityAction> action;
        if ((modifiers & KMOD_CTRL) && !(modifiers & (KMOD_SHIFT | KMOD_ALT)))
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

    // Terraform/land tool option windows (Land, Water, Land Rights, Clear Scenery). Like ride
    // construction, Ctrl+arrows navigate/adjust the window's options, Ctrl+Enter activates, Ctrl+B
    // reads the current option, and Escape closes; plain arrows fall through so the map cursor keeps
    // positioning the tool. Returns true if the key was consumed.
    static bool HandleToolWindowAccessKey(uint32_t key, uint32_t modifiers)
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return false;
        WindowBase* w = windowMgr->FindByClass(WindowClass::land);
        if (w == nullptr)
            w = windowMgr->FindByClass(WindowClass::water);
        if (w == nullptr)
            w = windowMgr->FindByClass(WindowClass::landRights);
        if (w == nullptr)
            w = windowMgr->FindByClass(WindowClass::clearScenery);
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
                    action = AccessibilityAction::announce;
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

    // Speaks a short summary of the controls available right now, triggered by F1. Contexts are
    // checked most-specific first so the tips always match whatever the player is currently doing.
    static void SpeakContextHelp()
    {
        auto* windowMgr = GetWindowManager();

        // Placing a pre-built ride design.
        if (Windows::WindowTrackPlaceIsActive())
        {
            ScreenReaderSpeak(
                "Placing a ride design. Arrow keys move the cursor. Enter places a preview you can arrow "
                "around to check, then Enter again builds it. Backspace repositions the preview, R rotates, "
                "Escape cancels.");
            return;
        }
        // Ride or maze construction (both use the ride construction window).
        if (IsRideConstructionWindowOpen())
        {
            ScreenReaderSpeak(
                "Construction. Arrow keys move the map cursor. Control up and down choose a build option, "
                "Control left and right change it, Control Enter builds at the cursor, Control B reads the "
                "build state. Delete removes the piece under the cursor, Insert moves the build focus to "
                "the track under the cursor. Escape exits.");
            return;
        }
        // Keyboard-driven flat-ride placement.
        if (IsAccessibleRidePlacementActive())
        {
            ScreenReaderSpeak(
                "Placing a ride. Arrow keys move the cursor, Enter places it, R rotates, Escape cancels.");
            return;
        }
        // Keyboard-driven scenery placement.
        if (IsAccessibleSceneryPlacementActive())
        {
            ScreenReaderSpeak(
                "Placing scenery. Arrow keys move the cursor, Enter places it at the cursor, Escape cancels.");
            return;
        }
        // Terraform tool option windows (Land, Water, Land Rights, Clear Scenery).
        if (windowMgr != nullptr
            && (windowMgr->FindByClass(WindowClass::land) != nullptr
                || windowMgr->FindByClass(WindowClass::water) != nullptr
                || windowMgr->FindByClass(WindowClass::landRights) != nullptr
                || windowMgr->FindByClass(WindowClass::clearScenery) != nullptr))
        {
            ScreenReaderSpeak(
                "Land tool. Arrow keys move the map cursor; Page Up and Page Down raise and lower land, "
                "hold Control for water. Control up and down choose a tool option, Control left and right "
                "change it, Escape closes.");
            return;
        }
        // Footpath window.
        if (windowMgr != nullptr && windowMgr->FindByClass(WindowClass::footpath) != nullptr)
        {
            ScreenReaderSpeak(
                "Footpath window. Up and down choose an option: path type, queue type, railings, or build "
                "mode. Left and right change it, Enter selects. Escape closes the window; then build paths "
                "with the map cursor and Space.");
            return;
        }
        // Bottom-toolbar status readout (opened with T).
        if (_statusMode)
        {
            ScreenReaderSpeak(
                "Status readout. Up and down cycle through the date, park rating, guests, cash, and recent "
                "messages. Enter opens messages, Escape closes.");
            return;
        }
        // Top-toolbar menu (opened with Tab).
        if (_menuMode)
        {
            ScreenReaderSpeak(
                "Toolbar menu. Up and down move through the toolbar buttons, Enter opens the selected one, "
                "Escape returns to the map.");
            return;
        }
        // Any other navigable window (rides, guests, park, finances, options, cheats, shortcut keys, etc.).
        if (GetActiveAccessibleWindow() != nullptr)
        {
            ScreenReaderSpeak(
                "Menu open. Up and down move through items, left and right change values or switch tabs, "
                "Enter activates, Escape closes.");
            return;
        }
        // Free-mouse mode.
        if (_mouseMode)
        {
            ScreenReaderSpeak("Mouse mode is on. Press Control Space to switch back to keyboard mode.");
            return;
        }
        // Default: the free map cursor during play.
        ScreenReaderSpeak(
            "Map cursor. Arrow keys move around the park. C reads the current tile and coordinates. "
            "T opens the status readout, M reads your cash. "
            "Space builds a footpath, D removes one, L cycles the path slope, End and Home lower and "
            "raise the build height for bridges. "
            "Page Up and Page Down raise and lower land, hold Control for water or Shift to zoom. "
            "X clears scenery, O buys land, B changes the brush size, K places markers. "
            "Enter opens the ride, stall, or gate under the cursor, Control Enter starts building it. "
            "Shift with a number sets a waypoint, Control with a number jumps to it. "
            "Shift with F, R, P, G, S, D, or M opens finances, rides, park, guests, staff, research, or "
            "messages. Tab opens the toolbar menu. Shift F1 opens the land tool. "
            "Control H rescues guests stranded on cut-off paths, teleporting them to the park entrance. "
            "Control with an arrow jumps to the nearest object in that direction, and Control Shift up "
            "and down choose what it looks for: rides and stalls, scenery, footpath objects, or hazards. "
            "Delete removes one thing at the cursor, asking which if there are several; D and X clear "
            "paths and scenery across a whole marked area. "
            "Control E jumps between the entrance and exit of the ride you are on. Control P checks whether "
            "the current tile connects to the park entrance. P pauses. Control equals and Control minus "
            "speed the game up and down. Control F1 opens the accessibility settings.");
    }

    // Spoken name for a point on the combined speed ladder (slow-motion factor + game speed).
    static std::string SpeedLabel(int32_t gameSpeed, int32_t slowFactor)
    {
        if (slowFactor >= 4)
            return "Quarter speed";
        if (slowFactor == 2)
            return "Half speed";
        switch (gameSpeed)
        {
            case 1:
                return "Normal speed";
            case 2:
                return "Double speed";
            case 3:
                return "Quadruple speed";
            case 4:
                return "Eight times speed";
            case 8:
                return "Turbo speed";
            default:
                return "Speed changed";
        }
    }

    // Steps the game speed one notch along a single ladder that runs
    //   quarter -> half -> normal -> double -> quadruple -> eight times -> (turbo, debug tools only)
    // dir > 0 speeds up, dir < 0 slows down. Slower-than-normal steps use the single-player slow-motion
    // factor; normal and faster use the engine's game speed (set through the networked action). The new
    // speed is announced.
    static void AdjustGameSpeed(int32_t dir)
    {
        // Fast and slow are mutually exclusive; clear any leftover slow factor if the game is already
        // above normal speed (e.g. after the base +/- shortcuts were used).
        if (gGameSpeed > 1)
            gGameSlowFactor = 1;

        if (dir > 0)
        {
            // Speeding up: climb out of slow motion first, then raise the game speed.
            if (gGameSlowFactor > 1)
            {
                gGameSlowFactor /= 2; // quarter -> half -> normal
                ScreenReaderSpeak(SpeedLabel(1, gGameSlowFactor));
                return;
            }
            int32_t newSpeed = std::min(Config::Get().general.debuggingTools ? 5 : 4, gGameSpeed + 1);
            if (newSpeed == 5)
                newSpeed = 8; // turbo
            if (newSpeed != gGameSpeed)
            {
                auto action = GameActions::GameSetSpeedAction(static_cast<uint8_t>(newSpeed));
                GameActions::Execute(&action, getGameState());
            }
            ScreenReaderSpeak(SpeedLabel(newSpeed, 1));
            return;
        }

        // Slowing down: drop the game speed toward normal first, then into slow motion.
        if (gGameSpeed > 1)
        {
            int32_t newSpeed = (gGameSpeed == 8) ? 4 : std::max(1, gGameSpeed - 1);
            auto action = GameActions::GameSetSpeedAction(static_cast<uint8_t>(newSpeed));
            GameActions::Execute(&action, getGameState());
            ScreenReaderSpeak(SpeedLabel(newSpeed, 1));
            return;
        }
        // Already at normal: enter slow motion (single player only; it would desync a network game).
        if (Network::GetMode() != Network::Mode::none)
        {
            ScreenReaderSpeak("Slow motion is only available in single player");
            return;
        }
        const int32_t newSlow = (gGameSlowFactor < 2) ? 2 : std::min(4, gGameSlowFactor * 2);
        gGameSlowFactor = static_cast<uint8_t>(newSlow);
        ScreenReaderSpeak(SpeedLabel(1, newSlow));
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
            _statusMode = false;
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

        // While rebinding a keyboard shortcut, let every key fall through to the shortcut manager so
        // it captures the new binding (Escape-to-cancel is handled earlier in HandleMenuNavigationKey).
        if (GetShortcutManager().isPendingShortcutChange())
            return false;

        // The Delete picker is modal: while it is open it answers every key, so nothing underneath
        // can act on a keystroke meant for choosing what to remove. Checked before everything else
        // for that reason.
        if (IsDeletePickerActive())
        {
            HandleDeletePickerKey(key);
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+F1 opens the accessibility mod's own settings window.
        if (key == SDLK_F1 && (e.modifiers & KMOD_CTRL) && !(e.modifiers & (KMOD_SHIFT | KMOD_ALT)))
        {
            Windows::AccessibilityOptionsOpen();
            _lastHandledKey = key;
            return true;
        }

        // Bare F1 speaks context-sensitive help for whatever the player is currently doing. Any
        // modified F1 (e.g. Shift+F1, now the land tool) falls through to the game's own shortcuts.
        if (key == SDLK_F1 && !(e.modifiers & (KMOD_SHIFT | KMOD_CTRL | KMOD_ALT)))
        {
            SpeakContextHelp();
            _lastHandledKey = key;
            return true;
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

        // Ctrl+H rescues guests who are stranded on footpaths cut off from any park exit, teleporting
        // them to the nearest entrance. Available in any mode so the player can always trigger it.
        if (key == SDLK_h && (e.modifiers & KMOD_CTRL))
        {
            RescueLostGuests();
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+P reports whether the tile under the cursor has a footpath route back to a park
        // entrance (the same flood-fill the lost-guest rescue uses), so the player can check any spot.
        if (key == SDLK_p && (e.modifiers & KMOD_CTRL) && !(e.modifiers & (KMOD_SHIFT | KMOD_ALT)))
        {
            if (!_initialised)
                InitialiseCursor();
            switch (CheckEntranceReachability(_cursor))
            {
                case EntranceReachability::noEntrance:
                    ScreenReaderSpeak("There is no park entrance");
                    break;
                case EntranceReachability::notOnPath:
                    ScreenReaderSpeak("Not on a footpath");
                    break;
                case EntranceReachability::reachable:
                    ScreenReaderSpeak("Connected to the park entrance");
                    break;
                case EntranceReachability::unreachable:
                {
                    std::string msg = "No path back to the park entrance";
                    // Report where the path stops short of connecting, so the player can go fix the gap.
                    if (auto gap = FindPathDisconnectPoint(_cursor); gap.has_value())
                    {
                        msg += ". Path stops nearest at " + SpokenTileCoordsText(*gap);
                        msg += DescribePathGapReason(*gap);
                    }
                    ScreenReaderSpeak(msg);
                    break;
                }
            }
            _lastHandledKey = key;
            return true;
        }

        // Plain P pauses or unpauses the game (routed through the game action so it is multiplayer
        // safe). Announces the new state. Only from the map cursor - not while the toolbar menu or a
        // navigable window is focused, where P belongs to that menu, so it doesn't pause behind it.
        if (key == SDLK_p && !(e.modifiers & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT)) && !_menuMode
            && Graph::FrontNavigableWindow() == nullptr)
        {
            const bool willPause = !(gGamePaused & GAME_PAUSED_NORMAL);
            auto action = GameActions::PauseToggleAction();
            GameActions::Execute(&action, getGameState());
            ScreenReaderSpeak(willPause ? "Paused" : "Resumed");
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+= speeds the game up and Ctrl+- slows it down, along one ladder from quarter speed up
        // to the maximum, announcing the new speed. (Keypad +/- work too.)
        if ((e.modifiers & KMOD_CTRL) && !(e.modifiers & (KMOD_SHIFT | KMOD_ALT))
            && (key == SDLK_EQUALS || key == SDLK_KP_PLUS || key == SDLK_MINUS || key == SDLK_KP_MINUS))
        {
            AdjustGameSpeed((key == SDLK_EQUALS || key == SDLK_KP_PLUS) ? 1 : -1);
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+E jumps the cursor between the entrance and exit of the ride the cursor is within, so a
        // player can find them on a pre-built coaster without hunting tile by tile. It also works over
        // a frozen track-design preview, where the tiles come from the design rather than a built ride
        // - the cursor roams freely there, so nothing drags the design along. Skipped in mouse/menu/
        // status mode and during the other placements (where Ctrl and the cursor mean other things).
        if (key == SDLK_e && (e.modifiers & KMOD_CTRL) && !(e.modifiers & (KMOD_SHIFT | KMOD_ALT)) && !_mouseMode
            && !_menuMode && !_statusMode && !IsAccessibleRidePlacementActive() && !IsAccessibleSceneryPlacementActive())
        {
            JumpRideEntranceExit();
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

        // Terraform/land tool option windows: same Ctrl+arrow scheme, plain arrows stay with the cursor.
        if (HandleToolWindowAccessKey(key, e.modifiers))
        {
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+Shift+Up/Down chooses what Ctrl+arrows jump between. Guarded identically to the jump
        // itself below, so the filter can never be changed in a mode where the jump does not run -
        // otherwise the setting would silently drift while the keys were doing something else.
        if ((e.modifiers & KMOD_CTRL) && (e.modifiers & KMOD_SHIFT) && !(e.modifiers & KMOD_ALT)
            && (key == SDLK_UP || key == SDLK_DOWN) && !_mouseMode && !_menuMode && !_statusMode
            && !Windows::WindowTrackPlaceIsActive() && !IsAccessibleRidePlacementActive()
            && !IsAccessibleSceneryPlacementActive())
        {
            CycleJumpCategory(key == SDLK_DOWN ? 1 : -1);
            _lastHandledKey = key;
            return true;
        }

        // Ctrl+arrows jump the free map cursor to the nearest target of the current filter category in
        // that direction. Skipped in mouse/menu/status mode and during any placement (there Ctrl+arrows
        // and the cursor mean something else); ride construction already consumed its Ctrl+arrows above.
        if ((e.modifiers & KMOD_CTRL) && !(e.modifiers & (KMOD_SHIFT | KMOD_ALT))
            && (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT) && !_mouseMode
            && !_menuMode && !_statusMode && !Windows::WindowTrackPlaceIsActive() && !IsAccessibleRidePlacementActive()
            && !IsAccessibleSceneryPlacementActive())
        {
            JumpToNearest(key);
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

        // The bottom-toolbar status readout (opened with T) owns the keyboard while it is open.
        if (_statusMode)
        {
            const bool handled = HandleStatusModeKey(key);
            _lastHandledKey = handled ? key : 0;
            return handled;
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

    bool TileHasNonSceneryBlocker(const TileCoordsXY& tile)
    {
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            switch (el->getType())
            {
                case TileElementType::path:
                case TileElementType::track:
                case TileElementType::entrance:
                case TileElementType::banner:
                    return true;
                default: // Surface, small/large scenery and walls/fences are clearable, so they don't count.
                    break;
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return false;
    }

    std::string DescribeEntranceExitConnection(const TileCoordsXYZD& loc)
    {
        if (loc.IsNull())
            return {};

        // A ride entrance/exit is stored facing toward its station platform; its doorway - where
        // guests walk in and a queue/path connects - faces the opposite way.
        const Direction doorway = GetEntranceFacing(loc.direction);
        std::string text = std::string(", facing ") + GetWorldDirectionName(doorway);

        // Is there a footpath on the doorway tile at roughly the entrance's height?
        const auto entranceWorld = TileCoordsXYZ(loc.x, loc.y, loc.z).ToCoordsXYZ();
        const CoordsXY doorTile = CoordsXY{ entranceWorld.x, entranceWorld.y } + CoordsDirectionDelta[doorway];
        const TileCoordsXY doorTileCoords{ doorTile };

        bool connected = false;
        for (TileElement* el = MapGetFirstElementAt(doorTileCoords); el != nullptr;)
        {
            if (auto* path = el->asPath(); path != nullptr)
            {
                const int32_t dz = path->getBaseZ() - entranceWorld.z;
                if ((dz < 0 ? -dz : dz) <= kPathHeightStep)
                {
                    connected = true;
                    break;
                }
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        text += connected ? ", path connected" : ", no path connected yet";
        return text;
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
