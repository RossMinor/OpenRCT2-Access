/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <openrct2-ui/accessibility/Elevation.h>
#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/MenuNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/accessibility/graph/GraphVocab.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/LandTool.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/FileSystem.hpp>
#include <openrct2/core/UnitConversion.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/TerrainEdgeObject.h>
#include <openrct2/object/TerrainSurfaceObject.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/TileElementsView.h>
#include <openrct2/world/map_generator/MapGen.h>
#include <openrct2/world/map_generator/PngTerrainGenerator.h>
#include <openrct2/world/tile_element/SmallSceneryElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

using namespace OpenRCT2::Drawing;
using namespace OpenRCT2::World;

namespace OpenRCT2::Ui::Windows
{
    namespace Vocab = Accessibility::Graph::Vocab;

    enum
    {
        WINDOW_MAPGEN_PAGE_BASE,
        WINDOW_MAPGEN_PAGE_TERRAIN,
        WINDOW_MAPGEN_PAGE_WATER,
        WINDOW_MAPGEN_PAGE_FORESTS,
        WINDOW_MAPGEN_PAGE_COUNT
    };

    enum WindowMapGenWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PAGE_BACKGROUND,
        WIDX_TAB_1,
        WIDX_TAB_2,
        WIDX_TAB_3,
        WIDX_TAB_4,
        WIDX_MAP_GENERATE,

        TAB_BEGIN,

        WIDX_MAP_SIZE_Y = TAB_BEGIN,
        WIDX_MAP_SIZE_Y_UP,
        WIDX_MAP_SIZE_Y_DOWN,
        WIDX_MAP_SIZE_LINK,
        WIDX_MAP_SIZE_X,
        WIDX_MAP_SIZE_X_UP,
        WIDX_MAP_SIZE_X_DOWN,
        WIDX_HEIGHTMAP_SOURCE,
        WIDX_HEIGHTMAP_SOURCE_DROPDOWN,

        WIDX_SIMPLEX_GROUP,
        WIDX_SIMPLEX_BASE_FREQ,
        WIDX_SIMPLEX_BASE_FREQ_UP,
        WIDX_SIMPLEX_BASE_FREQ_DOWN,
        WIDX_SIMPLEX_OCTAVES,
        WIDX_SIMPLEX_OCTAVES_UP,
        WIDX_SIMPLEX_OCTAVES_DOWN,

        WIDX_HEIGHTMAP_GROUP,
        WIDX_HEIGHTMAP_BROWSE,
        WIDX_HEIGHTMAP_NORMALIZE,
        WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP,
        WIDX_HEIGHTMAP_STRENGTH,
        WIDX_HEIGHTMAP_STRENGTH_UP,
        WIDX_HEIGHTMAP_STRENGTH_DOWN,

        WIDX_HEIGHTMAP_LOW = TAB_BEGIN,
        WIDX_HEIGHTMAP_LOW_UP,
        WIDX_HEIGHTMAP_LOW_DOWN,
        WIDX_HEIGHTMAP_HIGH,
        WIDX_HEIGHTMAP_HIGH_UP,
        WIDX_HEIGHTMAP_HIGH_DOWN,
        WIDX_FLOOR_TEXTURE,
        WIDX_WALL_TEXTURE,
        WIDX_RANDOM_TERRAIN,
        WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES,

        WIDX_WATER_LEVEL = TAB_BEGIN,
        WIDX_WATER_LEVEL_UP,
        WIDX_WATER_LEVEL_DOWN,
        WIDX_ADD_BEACHES,

        WIDX_FORESTS_PLACE_TREES = TAB_BEGIN,
        WIDX_TREE_LAND_RATIO,
        WIDX_TREE_LAND_RATIO_UP,
        WIDX_TREE_LAND_RATIO_DOWN,
        WIDX_TREE_ALTITUDE_MIN,
        WIDX_TREE_ALTITUDE_MIN_UP,
        WIDX_TREE_ALTITUDE_MIN_DOWN,
        WIDX_TREE_ALTITUDE_MAX,
        WIDX_TREE_ALTITUDE_MAX_UP,
        WIDX_TREE_ALTITUDE_MAX_DOWN,
    };

#pragma region Widgets

    static constexpr ScreenSize kWindowSize = { 300, 220 };

    // clang-format off
    static constexpr auto makeMapGenWidgets = [](StringId title) {
        return makeWidgets(
            makeWindowShim(title, kWindowSize),
            makeWidget({   0, 43 }, { kWindowSize.width, 177 }, WidgetType::resize, WindowColour::secondary),
            makeTab   ({   3, 17 }),
            makeTab   ({  34, 17 }),
            makeTab   ({  65, 17 }),
            makeTab   ({  96, 17 }),
            makeWidget({ 185, 200 }, { 109, 14 }, WidgetType::button, WindowColour::secondary, STR_MAPGEN_ACTION_GENERATE)
        );
    };

    static constexpr auto kBaseWidgets = makeWidgets(
        makeMapGenWidgets(STR_MAPGEN_CAPTION_GENERATOR),
        makeHoldableSpinnerWidgets({165, 52}, { 50, 14}, WidgetType::spinner,      WindowColour::secondary, kStringIdEmpty                                        ), // NB: 3 widgets
        makeWidget                ({216, 52}, { 21, 12}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_G2_LINK_CHAIN), STR_MAINTAIN_SQUARE_MAP_TOOLTIP),
        makeHoldableSpinnerWidgets({238, 52}, { 50, 14}, WidgetType::spinner,      WindowColour::secondary, kStringIdEmpty                                        ), // NB: 3 widgets
        makeDropdownWidgets       ({155, 72}, {133, 14}, WidgetType::dropdownMenu, WindowColour::secondary, STR_HEIGHTMAP_FLATLAND                                  ),

        makeWidget                ({  5,  94}, {290, 57}, WidgetType::groupbox, WindowColour::secondary, STR_MAPGEN_SIMPLEX_NOISE), // WIDX_SIMPLEX_GROUP
        makeHoldableSpinnerWidgets({179, 111}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                          ), // WIDX_SIMPLEX_BASE_FREQ{,_UP,_DOWN}
        makeHoldableSpinnerWidgets({179, 129}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                          ), // WIDX_SIMPLEX_OCTAVES{,_UP,_DOWN}

        makeWidget                ({  5,  90+8}, {290, 88}, WidgetType::groupbox, WindowColour::secondary, STR_MAPGEN_SELECT_HEIGHTMAP), // WIDX_HEIGHTMAP_GROUP
        makeWidget                ({223, 107+8}, { 65, 14}, WidgetType::button,   WindowColour::secondary, STR_BROWSE                 ), // WIDX_HEIGHTMAP_BROWSE
        makeWidget                ({ 10, 125+8}, {150, 12}, WidgetType::checkbox, WindowColour::secondary, STR_MAPGEN_NORMALIZE       ), // WIDX_HEIGHTMAP_NORMALIZE
        makeWidget                ({ 10, 141+8}, {150, 12}, WidgetType::checkbox, WindowColour::secondary, STR_MAPGEN_SMOOTH_HEIGHTMAP), // WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP
        makeHoldableSpinnerWidgets({179, 157+8}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                             )  // WIDX_HEIGHTMAP_STRENGTH{,_UP,_DOWN}
    );

    static constexpr auto kTerrainWidgets = makeWidgets(
        makeMapGenWidgets(STR_MAPGEN_CAPTION_TERRAIN),
        makeHoldableSpinnerWidgets({179,  52}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                                  ), // WIDX_HEIGHTMAP_LOW{,_UP,_DOWN}
        makeHoldableSpinnerWidgets({179,  70}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                                  ), // WIDX_HEIGHTMAP_HIGH{,_UP,_DOWN}
        makeWidget        ({179,  88}, { 47, 36}, WidgetType::flatBtn,  WindowColour::secondary, 0xFFFFFFFF, STR_CHANGE_BASE_LAND_TIP    ),
        makeWidget        ({236,  88}, { 47, 36}, WidgetType::flatBtn,  WindowColour::secondary, 0xFFFFFFFF, STR_CHANGE_VERTICAL_LAND_TIP),
        makeWidget        ({ 10, 106}, {150, 12}, WidgetType::checkbox, WindowColour::secondary, STR_MAPGEN_OPTION_RANDOM_TERRAIN        ),
        makeWidget        ({ 10, 122}, {150, 12}, WidgetType::checkbox, WindowColour::secondary, STR_MAPGEN_SMOOTH_TILE                  )  // WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES
    );

    static constexpr auto kWaterWidgets = makeWidgets(
        makeMapGenWidgets(STR_MAPGEN_CAPTION_WATER),
        makeHoldableSpinnerWidgets({179,  52}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                          ), // NB: 3 widgets
        makeWidget                ({ 10,  70}, {255, 12}, WidgetType::checkbox, WindowColour::secondary, STR_BEACHES_WATER_BODIES)
    );

    static constexpr auto kForestsWidgets = makeWidgets(
        makeMapGenWidgets(STR_MAPGEN_CAPTION_FORESTS),
        makeWidget                ({ 10,  52}, {255, 12}, WidgetType::checkbox, WindowColour::secondary, STR_MAPGEN_OPTION_PLACE_TREES),
        makeHoldableSpinnerWidgets({179,  70}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                               ), // WIDX_TREE_LAND_RATIO{,_UP,_DOWN}
        makeHoldableSpinnerWidgets({179,  88}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                               ), // WIDX_TREE_ALTITUDE_MIN{,_UP,_DOWN}
        makeHoldableSpinnerWidgets({179, 106}, {109, 14}, WidgetType::spinner,  WindowColour::secondary                               )  // WIDX_TREE_ALTITUDE_MAX{,_UP,_DOWN}
    );

    static std::span<const Widget> PageWidgets[WINDOW_MAPGEN_PAGE_COUNT] = {
        kBaseWidgets,
        kTerrainWidgets,
        kWaterWidgets,
        kForestsWidgets,
    };
    // clang-format on

#pragma endregion

    // clang-format off
    static constexpr int32_t TabAnimationDivisor[WINDOW_MAPGEN_PAGE_COUNT] = {
        2,
        1,
        1,
        1,
    };
    static constexpr int32_t TabAnimationFrames[WINDOW_MAPGEN_PAGE_COUNT] = {
        4,
        1,
        1,
        1,
    };
    static constexpr int32_t TabAnimationLoops[WINDOW_MAPGEN_PAGE_COUNT] = {
        16,
        1,
        1,
        1,
    };
    // clang-format on

    enum class ResizeDirection
    {
        both,
        x,
        y,
    };

    static void HeightmapLoadsaveCallback(ModalResult result, const utf8* path);

    class MapGenWindow final : public Window
    {
    private:
        ResizeDirection _resizeDirection{ ResizeDirection::both };
        bool _mapWidthAndHeightLinked{ true };
        MapGenerator::Settings _settings{};
        bool _randomTerrain = true;
        bool _heightmapLoaded = false;
        std::string _heightmapFilename{};

        u8string _xSpinnerCaption{};
        u8string _ySpinnerCaption{};

        void setPage(int32_t newPage)
        {
            // Skip setting page if we're already on this page, unless we're initialising the window
            if (page == newPage && !widgets.empty())
                return;

            page = newPage;
            currentFrame = 0;
            removeViewport();

            setWidgets(PageWidgets[newPage]);
            widgetSetPressedExclusive(*this, { WIDX_TAB_1, WIDX_TAB_2, WIDX_TAB_3, WIDX_TAB_4 }, WIDX_TAB_1 + newPage);

            initScrollWidgets();
            invalidate();
            onResize();
        }

        void DrawTabImage(RenderTarget& rt, int32_t newPage, int32_t spriteIndex)
        {
            WidgetIndex widgetIndex = WIDX_TAB_1 + newPage;

            if (!widgetIsDisabled(*this, widgetIndex))
            {
                if (page == newPage)
                {
                    int32_t frame = currentFrame / TabAnimationDivisor[page];
                    spriteIndex += (frame % TabAnimationFrames[page]);
                }

                GfxDrawSprite(
                    rt, ImageId(spriteIndex),
                    windowPos + ScreenCoordsXY{ widgets[widgetIndex].left, widgets[widgetIndex].top });
            }
        }

        void DrawTabImages(RenderTarget& rt)
        {
            DrawTabImage(rt, WINDOW_MAPGEN_PAGE_BASE, SPR_TAB_GEARS_0);
            DrawTabImage(rt, WINDOW_MAPGEN_PAGE_TERRAIN, SPR_G2_MAP_GEN_TERRAIN_TAB);
            DrawTabImage(rt, WINDOW_MAPGEN_PAGE_WATER, SPR_TAB_WATER);
            DrawTabImage(rt, WINDOW_MAPGEN_PAGE_FORESTS, SPR_TAB_SCENERY_TREES);
        }

        void ChangeMapSize(int32_t sizeOffset)
        {
            if (_mapWidthAndHeightLinked)
                _resizeDirection = ResizeDirection::both;

            if (_resizeDirection != ResizeDirection::x)
            {
                _settings.mapSize.y = std::clamp<int32_t>(
                    _settings.mapSize.y + sizeOffset, kMinimumMapSizeTechnical, kMaximumMapSizeTechnical);
            }

            if (_resizeDirection != ResizeDirection::y)
            {
                _settings.mapSize.x = std::clamp<int32_t>(
                    _settings.mapSize.x + sizeOffset, kMinimumMapSizeTechnical, kMaximumMapSizeTechnical);
            }
        }

        void InputMapSize(WidgetIndex callingWidget, int32_t currentValue)
        {
            Formatter ft;
            ft.Add<int16_t>(kMinimumMapSizePractical);
            ft.Add<int16_t>(kMaximumMapSizePractical);

            // Practical map size is 2 lower than the technical map size
            currentValue -= 2;
            WindowTextInputOpen(
                this, callingWidget, STR_MAP_SIZE_2, STR_ENTER_MAP_SIZE, ft, STR_FORMAT_INTEGER, currentValue, 4);
        }

        void SharedMouseUp(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_TAB_1:
                case WIDX_TAB_2:
                case WIDX_TAB_3:
                case WIDX_TAB_4:
                    setPage(widgetIndex - WIDX_TAB_1);
                    break;
                case WIDX_MAP_GENERATE:
                    GenerateMap();
                    break;
            }
        }

        void GenerateMap()
        {
            if (_settings.algorithm == MapGenerator::Algorithm::heightmapImage && !_heightmapLoaded)
                return;

            MapGenerator::Settings mapgenSettings = _settings;
            if (_randomTerrain)
            {
                mapgenSettings.landTexture = -1;
                mapgenSettings.edgeTexture = -1;
            }

            MapGenerator::generate(&mapgenSettings);
            GfxInvalidateScreen();
        }

#pragma region Base page

        void BaseMouseUp(WidgetIndex widgetIndex)
        {
            SharedMouseUp(widgetIndex);

            if (_settings.algorithm == MapGenerator::Algorithm::simplexNoise)
                SimplexMouseUp(widgetIndex);
            else if (_settings.algorithm == MapGenerator::Algorithm::heightmapImage)
                HeightmapMouseUp(widgetIndex);

            switch (widgetIndex)
            {
                case WIDX_MAP_SIZE_Y:
                    _resizeDirection = ResizeDirection::y;
                    InputMapSize(WIDX_MAP_SIZE_Y, _settings.mapSize.y);
                    break;
                case WIDX_MAP_SIZE_X:
                    _resizeDirection = ResizeDirection::x;
                    InputMapSize(WIDX_MAP_SIZE_X, _settings.mapSize.x);
                    break;
                case WIDX_MAP_SIZE_LINK:
                    _mapWidthAndHeightLinked = !_mapWidthAndHeightLinked;
                    break;
            }
        }

        void BaseMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            if (_settings.algorithm == MapGenerator::Algorithm::simplexNoise)
                SimplexMouseDown(widgetIndex, widget);

            else if (_settings.algorithm == MapGenerator::Algorithm::heightmapImage)
                HeightmapMouseDown(widgetIndex, widget);

            switch (widgetIndex)
            {
                case WIDX_MAP_SIZE_Y_UP:
                    _resizeDirection = ResizeDirection::y;
                    ChangeMapSize(+1);
                    invalidate();
                    break;
                case WIDX_MAP_SIZE_Y_DOWN:
                    _resizeDirection = ResizeDirection::y;
                    ChangeMapSize(-1);
                    invalidate();
                    break;
                case WIDX_MAP_SIZE_X_UP:
                    _resizeDirection = ResizeDirection::x;
                    ChangeMapSize(+1);
                    invalidate();
                    break;
                case WIDX_MAP_SIZE_X_DOWN:
                    _resizeDirection = ResizeDirection::x;
                    ChangeMapSize(-1);
                    invalidate();
                    break;
                case WIDX_HEIGHTMAP_SOURCE_DROPDOWN:
                {
                    using namespace Dropdown;

                    constexpr ItemExt items[] = {
                        ToggleOption(0, STR_HEIGHTMAP_FLATLAND),
                        ToggleOption(1, STR_HEIGHTMAP_SIMPLEX_NOISE),
                        ToggleOption(2, STR_HEIGHTMAP_FILE),
                    };

                    SetItems(items);

                    Widget* ddWidget = &widgets[widgetIndex - 1];
                    WindowDropdownShowTextCustomWidth(
                        { windowPos.x + ddWidget->left, windowPos.y + ddWidget->top }, ddWidget->height(), colours[1], 0, {},
                        std::size(items), ddWidget->width() - 3);

                    gDropdown.items[EnumValue(_settings.algorithm)].setChecked(true);
                    break;
                }
            }
        }

        void BaseUpdate()
        {
            // Tab animation
            if (++currentFrame >= TabAnimationLoops[page])
                currentFrame = 0;
            invalidateWidget(WIDX_TAB_1);
        }

        void BaseDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            if (dropdownIndex == -1)
                return;

            switch (widgetIndex)
            {
                case WIDX_HEIGHTMAP_SOURCE_DROPDOWN:
                    _settings.algorithm = MapGenerator::Algorithm(dropdownIndex);
                    invalidate();
                    break;
            }
        }

        void BaseTextInput(WidgetIndex widgetIndex, int32_t value)
        {
            if (_settings.algorithm == MapGenerator::Algorithm::simplexNoise)
                SimplexTextInput(widgetIndex, value);
            else if (_settings.algorithm == MapGenerator::Algorithm::heightmapImage)
                HeightmapTextInput(widgetIndex, value);

            switch (widgetIndex)
            {
                case WIDX_MAP_SIZE_Y:
                case WIDX_MAP_SIZE_X:
                    // The practical size is 2 lower than the technical size
                    auto technicalSize = std::clamp<uint16_t>(value + 2, kMinimumMapSizeTechnical, kMaximumMapSizeTechnical);
                    if (_resizeDirection == ResizeDirection::y || _mapWidthAndHeightLinked)
                        _settings.mapSize.y = technicalSize;
                    if (_resizeDirection == ResizeDirection::x || _mapWidthAndHeightLinked)
                        _settings.mapSize.x = technicalSize;
                    break;
            }

            invalidate();
        }

        void BasePrepareDraw()
        {
            // only allow linking the map size when X and Y are the same
            setWidgetPressed(WIDX_MAP_SIZE_LINK, _mapWidthAndHeightLinked);
            setWidgetDisabled(WIDX_MAP_SIZE_LINK, _settings.mapSize.x != _settings.mapSize.y);

            bool isHeightMapImage = _settings.algorithm == MapGenerator::Algorithm::heightmapImage;
            setWidgetDisabled(WIDX_MAP_SIZE_Y, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_Y_UP, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_Y_DOWN, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_LINK, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_X, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_X_UP, isHeightMapImage);
            setWidgetDisabled(WIDX_MAP_SIZE_X_DOWN, isHeightMapImage);

            // Enable heightmap widgets if one is loaded
            const bool heightmapEnabled = isHeightMapImage && _heightmapLoaded;
            setWidgetEnabled(WIDX_HEIGHTMAP_NORMALIZE, heightmapEnabled);
            setWidgetEnabled(WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP, heightmapEnabled);
            setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH, heightmapEnabled && _settings.smooth_height_map);
            setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH_UP, heightmapEnabled && _settings.smooth_height_map);
            setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH_DOWN, heightmapEnabled && _settings.smooth_height_map);

            _xSpinnerCaption = std::to_string(_settings.mapSize.x - 2);
            widgets[WIDX_MAP_SIZE_X].setString(_xSpinnerCaption.c_str());
            _ySpinnerCaption = std::to_string(_settings.mapSize.y - 2);
            widgets[WIDX_MAP_SIZE_Y].setString(_ySpinnerCaption.c_str());

            auto& sourceWidget = widgets[WIDX_HEIGHTMAP_SOURCE];
            switch (_settings.algorithm)
            {
                case MapGenerator::Algorithm::blank:
                    sourceWidget.text = STR_HEIGHTMAP_FLATLAND;
                    ToggleSimplexWidgets(false);
                    ToggleHeightmapWidgets(false);
                    break;

                case MapGenerator::Algorithm::simplexNoise:
                    sourceWidget.text = STR_HEIGHTMAP_SIMPLEX_NOISE;
                    ToggleSimplexWidgets(true);
                    ToggleHeightmapWidgets(false);
                    break;

                case MapGenerator::Algorithm::heightmapImage:
                    sourceWidget.text = STR_HEIGHTMAP_FILE;
                    ToggleSimplexWidgets(false);
                    ToggleHeightmapWidgets(true);
                    HeightmapPrepareDraw();
                    break;
            }
        }

        void ToggleSimplexWidgets(bool state)
        {
            widgets[WIDX_SIMPLEX_GROUP].setVisible(state);
            widgets[WIDX_SIMPLEX_BASE_FREQ].setVisible(state);
            widgets[WIDX_SIMPLEX_BASE_FREQ_UP].setVisible(state);
            widgets[WIDX_SIMPLEX_BASE_FREQ_DOWN].setVisible(state);
            widgets[WIDX_SIMPLEX_OCTAVES].setVisible(state);
            widgets[WIDX_SIMPLEX_OCTAVES_UP].setVisible(state);
            widgets[WIDX_SIMPLEX_OCTAVES_DOWN].setVisible(state);
        }

        void ToggleHeightmapWidgets(bool state)
        {
            widgets[WIDX_HEIGHTMAP_GROUP].setVisible(state);
            widgets[WIDX_HEIGHTMAP_BROWSE].setVisible(state);
            widgets[WIDX_HEIGHTMAP_NORMALIZE].setVisible(state);
            widgets[WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP].setVisible(state);
            widgets[WIDX_HEIGHTMAP_STRENGTH].setVisible(state);
            widgets[WIDX_HEIGHTMAP_STRENGTH_UP].setVisible(state);
            widgets[WIDX_HEIGHTMAP_STRENGTH_DOWN].setVisible(state);
        }

        void BaseDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            if (_settings.algorithm == MapGenerator::Algorithm::simplexNoise)
                SimplexDraw(rt);

            else if (_settings.algorithm == MapGenerator::Algorithm::heightmapImage)
                HeightmapDraw(rt);

            const auto enabledColour = colours[1];
            const auto disabledColour = enabledColour.withFlag(ColourFlag::inset, true);

            {
                auto textColour = isWidgetDisabled(WIDX_MAP_SIZE_Y) ? disabledColour : enabledColour;
                drawText(rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_MAP_SIZE_Y].top + 1 }, STR_MAP_SIZE, { textColour });
            }

            {
                auto textColour = enabledColour;
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_HEIGHTMAP_SOURCE].top + 1 }, STR_HEIGHTMAP_SOURCE,
                    { textColour });
            }
        }

#pragma endregion

#pragma region Forests page

        void ForestsMouseUp(WidgetIndex widgetIndex)
        {
            SharedMouseUp(widgetIndex);

            switch (widgetIndex)
            {
                case WIDX_FORESTS_PLACE_TREES:
                    _settings.trees ^= true;
                    invalidate();
                    break;

                case WIDX_TREE_LAND_RATIO:
                {
                    Formatter ft;
                    ft.Add<int16_t>(1);
                    ft.Add<int16_t>(50);
                    WindowTextInputOpen(
                        this, widgetIndex, STR_TREE_TO_LAND_RATIO, STR_ENTER_TREE_TO_LAND_RATIO, ft, STR_FORMAT_INTEGER,
                        _settings.treeToLandRatio, 2);
                    break;
                }

                case WIDX_TREE_ALTITUDE_MIN:
                {
                    Formatter ft;
                    ft.Add<int16_t>(BaseZToMetres(kMinimumLandHeight));
                    ft.Add<int16_t>(BaseZToMetres(kMaximumLandHeight));
                    WindowTextInputOpen(
                        this, widgetIndex, STR_MIN_TREE_ALTITUDE, STR_ENTER_MIN_TREE_ALTITUDE, ft, STR_FORMAT_INTEGER,
                        BaseZToMetres(_settings.minTreeAltitude), 6);
                    break;
                }

                case WIDX_TREE_ALTITUDE_MAX:
                {
                    Formatter ft;
                    ft.Add<int16_t>(BaseZToMetres(kMinimumLandHeight));
                    ft.Add<int16_t>(BaseZToMetres(kMaximumLandHeight));
                    WindowTextInputOpen(
                        this, widgetIndex, STR_MAX_TREE_ALTITUDE, STR_ENTER_MAX_TREE_ALTITUDE, ft, STR_FORMAT_INTEGER,
                        BaseZToMetres(_settings.maxTreeAltitude), 6);
                    break;
                }
            }
        }

        void ForestsMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            switch (widgetIndex)
            {
                case WIDX_TREE_LAND_RATIO_UP:
                    _settings.treeToLandRatio = std::min(_settings.treeToLandRatio + 1, 50);
                    invalidateWidget(WIDX_TREE_LAND_RATIO);
                    break;
                case WIDX_TREE_LAND_RATIO_DOWN:
                    _settings.treeToLandRatio = std::max(_settings.treeToLandRatio - 1, 1);
                    invalidateWidget(WIDX_TREE_LAND_RATIO);
                    break;
                case WIDX_TREE_ALTITUDE_MIN_UP:
                    _settings.minTreeAltitude = std::min(_settings.minTreeAltitude + 2, kMaximumLandHeight / 2 - 1);
                    _settings.maxTreeAltitude = std::max(_settings.maxTreeAltitude, _settings.minTreeAltitude + 2);
                    invalidateWidget(WIDX_TREE_ALTITUDE_MIN);
                    break;
                case WIDX_TREE_ALTITUDE_MIN_DOWN:
                    _settings.minTreeAltitude = std::max<int32_t>(_settings.minTreeAltitude - 2, kMinimumLandHeight);
                    invalidateWidget(WIDX_TREE_ALTITUDE_MIN);
                    break;
                case WIDX_TREE_ALTITUDE_MAX_UP:
                    _settings.maxTreeAltitude = std::min<int32_t>(_settings.maxTreeAltitude + 2, kMaximumLandHeight - 1);
                    invalidateWidget(WIDX_TREE_ALTITUDE_MAX);
                    break;
                case WIDX_TREE_ALTITUDE_MAX_DOWN:
                    _settings.maxTreeAltitude = std::max<int32_t>(_settings.maxTreeAltitude - 2, kMinimumLandHeight - 1);
                    _settings.minTreeAltitude = std::min(_settings.minTreeAltitude, _settings.maxTreeAltitude - 2);
                    invalidateWidget(WIDX_TREE_ALTITUDE_MAX);
                    break;
            }
        }

        void ForestsUpdate()
        {
            // Tab animation
            if (++currentFrame >= TabAnimationLoops[page])
                currentFrame = 0;
            invalidateWidget(WIDX_TAB_2);
        }

        void ForestsTextInput(WidgetIndex widgetIndex, int32_t rawValue, int32_t value)
        {
            switch (widgetIndex)
            {
                case WIDX_TREE_LAND_RATIO:
                    _settings.treeToLandRatio = std::clamp(rawValue, 1, 50);
                    break;

                case WIDX_TREE_ALTITUDE_MIN:
                    _settings.minTreeAltitude = value;
                    _settings.maxTreeAltitude = std::max(_settings.minTreeAltitude, _settings.maxTreeAltitude);
                    break;

                case WIDX_TREE_ALTITUDE_MAX:
                    _settings.maxTreeAltitude = value;
                    _settings.minTreeAltitude = std::min(_settings.minTreeAltitude, _settings.maxTreeAltitude);
                    break;
            }

            invalidate();
        }

        void ForestsPrepareDraw()
        {
            setWidgetPressed(WIDX_FORESTS_PLACE_TREES, _settings.trees);

            const bool isFlatland = _settings.algorithm == MapGenerator::Algorithm::blank;

            setWidgetDisabled(WIDX_TREE_LAND_RATIO, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_LAND_RATIO_UP, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_LAND_RATIO_DOWN, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MIN, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MIN_UP, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MIN_DOWN, !_settings.trees);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MAX, !_settings.trees || isFlatland);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MAX_UP, !_settings.trees || isFlatland);
            setWidgetDisabled(WIDX_TREE_ALTITUDE_MAX_DOWN, !_settings.trees || isFlatland);
        }

        void ForestsDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            const auto enabledColour = colours[1];
            const auto disabledColour = enabledColour.withFlag(ColourFlag::inset, true);

            const auto textColour = _settings.trees ? enabledColour : disabledColour;

            // Tree to land ratio, label and value
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_TREE_LAND_RATIO].top + 1 }, STR_MAPGEN_TREE_TO_LAND_RATIO,
                { textColour });

            auto ft = Formatter();
            ft.Add<uint16_t>(_settings.treeToLandRatio);
            drawText(
                rt, windowPos + ScreenCoordsXY{ widgets[WIDX_TREE_LAND_RATIO].left + 1, widgets[WIDX_TREE_LAND_RATIO].top + 1 },
                STR_MAPGEN_TREE_TO_LAND_RATIO_PCT, ft, { textColour });

            // Minimum tree altitude, label and value
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_TREE_ALTITUDE_MIN].top + 1 }, STR_MAPGEN_TREE_MIN_ALTITUDE,
                { textColour });

            ft = Formatter();
            ft.Add<int16_t>(BaseZToMetres(_settings.minTreeAltitude));
            drawText(
                rt,
                windowPos + ScreenCoordsXY{ widgets[WIDX_TREE_ALTITUDE_MIN].left + 1, widgets[WIDX_TREE_ALTITUDE_MIN].top + 1 },
                STR_RIDE_LENGTH_ENTRY, ft, { textColour });

            // Maximum tree altitude, label and value
            const bool isFlatland = _settings.algorithm == MapGenerator::Algorithm::blank;
            const auto maxTreeTextColour = _settings.trees && !isFlatland ? enabledColour : disabledColour;

            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_TREE_ALTITUDE_MAX].top + 1 }, STR_MAPGEN_TREE_MAX_ALTITUDE,
                { maxTreeTextColour });

            ft = Formatter();
            ft.Add<int16_t>(BaseZToMetres(_settings.maxTreeAltitude));
            drawText(
                rt,
                windowPos + ScreenCoordsXY{ widgets[WIDX_TREE_ALTITUDE_MAX].left + 1, widgets[WIDX_TREE_ALTITUDE_MAX].top + 1 },
                STR_RIDE_LENGTH_ENTRY, ft, { maxTreeTextColour });
        }

#pragma endregion

#pragma region Simplex settings, part of generator tab

        void SimplexMouseUp(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_SIMPLEX_BASE_FREQ:
                {
                    Formatter ft;
                    ft.Add<int32_t>(0);
                    ft.Add<int32_t>(1000);
                    WindowTextInputOpen(
                        this, widgetIndex, STR_SIMPLEX_BASE_FREQUENCY, STR_ENTER_BASE_FREQUENCY, ft, STR_FORMAT_COMMA2DP32,
                        _settings.simplex_base_freq, 4);
                    break;
                }

                case WIDX_SIMPLEX_OCTAVES:
                {
                    Formatter ft;
                    ft.Add<int16_t>(1);
                    ft.Add<int16_t>(10);
                    WindowTextInputOpen(
                        this, widgetIndex, STR_SIMPLEX_OCTAVES, STR_ENTER_OCTAVES, ft, STR_FORMAT_INTEGER,
                        _settings.simplex_octaves, 10);
                    break;
                }
            }
        }

        void SimplexMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            switch (widgetIndex)
            {
                case WIDX_SIMPLEX_BASE_FREQ_UP:
                    _settings.simplex_base_freq = std::min<int32_t>(_settings.simplex_base_freq + 5, 1000);
                    invalidate();
                    break;
                case WIDX_SIMPLEX_BASE_FREQ_DOWN:
                    _settings.simplex_base_freq = std::max<int32_t>(_settings.simplex_base_freq - 5, 0);
                    invalidate();
                    break;
                case WIDX_SIMPLEX_OCTAVES_UP:
                    _settings.simplex_octaves = std::min(_settings.simplex_octaves + 1, 10);
                    invalidate();
                    break;
                case WIDX_SIMPLEX_OCTAVES_DOWN:
                    _settings.simplex_octaves = std::max(_settings.simplex_octaves - 1, 1);
                    invalidate();
                    break;
            }
        }

        void SimplexDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            const auto textColour = colours[1];

            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_SIMPLEX_BASE_FREQ].top + 1 },
                STR_MAPGEN_SIMPLEX_NOISE_BASE_FREQUENCY, { textColour });
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_SIMPLEX_OCTAVES].top + 1 }, STR_MAPGEN_SIMPLEX_NOISE_OCTAVES,
                {}, { textColour });

            auto ft = Formatter();
            ft.Add<uint16_t>(_settings.simplex_base_freq);
            drawText(
                rt,
                windowPos + ScreenCoordsXY{ widgets[WIDX_SIMPLEX_BASE_FREQ].left + 1, widgets[WIDX_SIMPLEX_BASE_FREQ].top + 1 },
                STR_WINDOW_COLOUR_2_COMMA2DP32, ft, { textColour });

            ft = Formatter();
            ft.Add<uint16_t>(_settings.simplex_octaves);
            drawText(
                rt, windowPos + ScreenCoordsXY{ widgets[WIDX_SIMPLEX_OCTAVES].left + 1, widgets[WIDX_SIMPLEX_OCTAVES].top + 1 },
                STR_COMMA16, ft, { textColour });
        }

        void SimplexTextInput(WidgetIndex widgetIndex, int32_t value)
        {
            switch (widgetIndex)
            {
                case WIDX_SIMPLEX_BASE_FREQ:
                    _settings.simplex_base_freq = std::clamp(value, 0, 1000);
                    break;

                case WIDX_SIMPLEX_OCTAVES:
                    _settings.simplex_octaves = std::clamp(value, 1, 10);
                    break;
            }
        }

#pragma endregion

#pragma region Heightmap settings, part of generator tab

        void HeightmapMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            switch (widgetIndex)
            {
                case WIDX_HEIGHTMAP_STRENGTH_UP:
                    _settings.smooth_strength = std::min<uint32_t>(_settings.smooth_strength + 1, 20);
                    invalidateWidget(WIDX_HEIGHTMAP_STRENGTH);
                    break;
                case WIDX_HEIGHTMAP_STRENGTH_DOWN:
                    _settings.smooth_strength = std::max<uint32_t>(_settings.smooth_strength - 1, 1);
                    invalidateWidget(WIDX_HEIGHTMAP_STRENGTH);
                    break;
            }
        }

        void HeightmapMouseUp(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                // Page widgets
                case WIDX_HEIGHTMAP_BROWSE:
                {
                    auto intent = Intent(WindowClass::loadsave);
                    intent.PutEnumExtra<LoadSaveAction>(INTENT_EXTRA_LOADSAVE_ACTION, LoadSaveAction::load);
                    intent.PutEnumExtra<LoadSaveType>(INTENT_EXTRA_LOADSAVE_TYPE, LoadSaveType::heightmap);
                    intent.PutExtra(INTENT_EXTRA_CALLBACK, reinterpret_cast<CloseCallback>(HeightmapLoadsaveCallback));
                    ContextOpenIntent(&intent);
                    return;
                }
                case WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP:
                    _settings.smooth_height_map = !_settings.smooth_height_map;
                    setCheckboxValue(WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP, _settings.smooth_height_map);
                    setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH, _settings.smooth_height_map);
                    setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH_UP, _settings.smooth_height_map);
                    setWidgetEnabled(WIDX_HEIGHTMAP_STRENGTH_DOWN, _settings.smooth_height_map);
                    invalidateWidget(WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP);
                    invalidateWidget(WIDX_HEIGHTMAP_STRENGTH);
                    break;

                case WIDX_HEIGHTMAP_NORMALIZE:
                    _settings.normalize_height = !_settings.normalize_height;
                    setCheckboxValue(WIDX_HEIGHTMAP_NORMALIZE, _settings.normalize_height);
                    invalidateWidget(WIDX_HEIGHTMAP_NORMALIZE);
                    break;

                case WIDX_HEIGHTMAP_STRENGTH:
                {
                    Formatter ft;
                    ft.Add<int16_t>(1);
                    ft.Add<int16_t>(20);
                    WindowTextInputOpen(
                        this, widgetIndex, STR_SMOOTH_STRENGTH, STR_ENTER_SMOOTH_STRENGTH, ft, STR_FORMAT_INTEGER,
                        _settings.smooth_strength, 2);
                    break;
                }
            }
        }

        void HeightmapPrepareDraw()
        {
            setCheckboxValue(WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP, _settings.smooth_height_map);
            setCheckboxValue(WIDX_HEIGHTMAP_NORMALIZE, _settings.normalize_height);
        }

        void HeightmapDraw(RenderTarget& rt)
        {
            const auto enabledColour = colours[1];
            const auto disabledColour = enabledColour.withFlag(ColourFlag::inset, true);

            // Smooth strength label and value
            const bool strengthDisabled = isWidgetDisabled(WIDX_HEIGHTMAP_STRENGTH) || !_settings.smooth_height_map;
            const auto strengthColour = strengthDisabled ? disabledColour : enabledColour;

            // Smooth strength label
            drawText(
                rt, windowPos + ScreenCoordsXY{ 24, widgets[WIDX_HEIGHTMAP_STRENGTH].top + 1 }, STR_MAPGEN_SMOOTH_STRENGTH,
                { strengthColour });

            // Smooth strength value
            auto ft = Formatter();
            ft.Add<uint16_t>(_settings.smooth_strength);
            auto pos = ScreenCoordsXY{ widgets[WIDX_HEIGHTMAP_STRENGTH].left + 1, widgets[WIDX_HEIGHTMAP_STRENGTH].top + 1 };
            drawText(rt, windowPos + pos, STR_COMMA16, ft, { strengthColour });

            // Current heightmap image filename
            ft = Formatter();
            if (!_heightmapLoaded)
                ft.Add<char*>(LanguageGetString(STR_MAPGEN_NONE_SELECTED));
            else
                ft.Add<char*>(_heightmapFilename.c_str());

            pos = ScreenCoordsXY{ 10, widgets[WIDX_HEIGHTMAP_BROWSE].top + 1 };
            auto textWidth = widgets[WIDX_HEIGHTMAP_BROWSE].left - 11;
            drawTextEllipsised(rt, windowPos + pos, textWidth, STR_MAPGEN_CURRENT_HEIGHTMAP_FILE, ft);
        }

        void HeightmapTextInput(WidgetIndex widgetIndex, int32_t value)
        {
            switch (widgetIndex)
            {
                case WIDX_HEIGHTMAP_STRENGTH:
                    _settings.smooth_strength = std::clamp(value, 1, 20);
                    break;
            }
        }

#pragma endregion

#pragma region Terrain page

        void TerrainMouseUp(WidgetIndex widgetIndex)
        {
            SharedMouseUp(widgetIndex);

            switch (widgetIndex)
            {
                case WIDX_HEIGHTMAP_LOW:
                {
                    Formatter ft;
                    ft.Add<int16_t>(BaseZToMetres(kMinimumLandHeight));
                    ft.Add<int16_t>(BaseZToMetres(kMaximumLandHeight));
                    WindowTextInputOpen(
                        this, widgetIndex, STR_MIN_LAND_HEIGHT, STR_ENTER_MIN_LAND, ft, STR_FORMAT_INTEGER,
                        BaseZToMetres(_settings.heightmapLow), 6);
                    break;
                }

                case WIDX_HEIGHTMAP_HIGH:
                {
                    Formatter ft;
                    ft.Add<int16_t>(BaseZToMetres(kMinimumLandHeight));
                    ft.Add<int16_t>(BaseZToMetres(kMaximumLandHeight));
                    WindowTextInputOpen(
                        this, widgetIndex, STR_MAX_LAND_HEIGHT, STR_ENTER_MAX_LAND, ft, STR_FORMAT_INTEGER,
                        BaseZToMetres(_settings.heightmapHigh), 6);
                    break;
                }

                case WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES:
                    _settings.smoothTileEdges = !_settings.smoothTileEdges;
                    setCheckboxValue(WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES, _settings.smoothTileEdges);
                    invalidateWidget(WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES);
                    break;
            }
        }

        void TerrainMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            switch (widgetIndex)
            {
                case WIDX_RANDOM_TERRAIN:
                    _randomTerrain = !_randomTerrain;
                    invalidate();
                    break;
                case WIDX_FLOOR_TEXTURE:
                    LandTool::ShowSurfaceStyleDropdown(this, widget, _settings.landTexture);
                    break;
                case WIDX_WALL_TEXTURE:
                    LandTool::ShowEdgeStyleDropdown(this, widget, _settings.edgeTexture);
                    break;
                case WIDX_HEIGHTMAP_LOW_UP:
                    _settings.heightmapLow = std::min(_settings.heightmapLow + 2, kMaximumLandHeight / 2 - 1);
                    _settings.heightmapHigh = std::max(_settings.heightmapHigh, _settings.heightmapLow + 2);
                    invalidateWidget(WIDX_HEIGHTMAP_LOW);
                    break;
                case WIDX_HEIGHTMAP_LOW_DOWN:
                    _settings.heightmapLow = std::max<int32_t>(_settings.heightmapLow - 2, kMinimumLandHeight);
                    invalidateWidget(WIDX_HEIGHTMAP_LOW);
                    break;
                case WIDX_HEIGHTMAP_HIGH_UP:
                    _settings.heightmapHigh = std::min<int32_t>(_settings.heightmapHigh + 2, kMaximumLandHeight - 1);
                    invalidateWidget(WIDX_HEIGHTMAP_HIGH);
                    break;
                case WIDX_HEIGHTMAP_HIGH_DOWN:
                    _settings.heightmapHigh = std::max<int32_t>(_settings.heightmapHigh - 2, kMinimumLandHeight);
                    _settings.heightmapLow = std::min(_settings.heightmapLow, _settings.heightmapHigh - 2);
                    invalidateWidget(WIDX_HEIGHTMAP_HIGH);
                    break;
            }
        }

        void TerrainUpdate()
        {
            // Tab animation
            if (++currentFrame >= TabAnimationLoops[page])
                currentFrame = 0;
            invalidateWidget(WIDX_TAB_3);
        }

        void TerrainTextInput(WidgetIndex widgetIndex, int32_t value)
        {
            switch (widgetIndex)
            {
                case WIDX_HEIGHTMAP_LOW:
                    _settings.heightmapLow = value;
                    _settings.heightmapHigh = std::max(_settings.heightmapLow, _settings.heightmapHigh);
                    break;

                case WIDX_HEIGHTMAP_HIGH:
                    _settings.heightmapHigh = value;
                    _settings.heightmapLow = std::min(_settings.heightmapLow, _settings.heightmapHigh);
                    break;
            }

            invalidate();
        }

        void TerrainDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            int32_t type;

            switch (widgetIndex)
            {
                case WIDX_FLOOR_TEXTURE:
                    if (dropdownIndex == -1)
                        dropdownIndex = gDropdown.highlightedIndex;

                    type = (dropdownIndex == -1) ? _settings.landTexture : dropdownIndex;

                    if (gLandToolTerrainSurface == type)
                    {
                        gLandToolTerrainSurface = kObjectEntryIndexNull;
                    }
                    else
                    {
                        gLandToolTerrainSurface = type;
                        _settings.landTexture = type;
                    }
                    invalidate();
                    break;
                case WIDX_WALL_TEXTURE:
                    if (dropdownIndex == -1)
                        dropdownIndex = gDropdown.highlightedIndex;

                    type = (dropdownIndex == -1) ? _settings.edgeTexture : dropdownIndex;

                    if (gLandToolTerrainEdge == type)
                    {
                        gLandToolTerrainEdge = kObjectEntryIndexNull;
                    }
                    else
                    {
                        gLandToolTerrainEdge = type;
                        _settings.edgeTexture = type;
                    }
                    invalidate();
                    break;
            }
        }

        void DrawDropdownButton(RenderTarget& rt, WidgetIndex widgetIndex, ImageId image)
        {
            const auto& widget = widgets[widgetIndex];
            ScreenCoordsXY pos = { windowPos.x + widget.left, windowPos.y + widget.top };
            if (isWidgetDisabled(widgetIndex))
            {
                // Draw greyed out (light border bottom right shadow)
                auto colour = colours[widget.colour].colour;
                auto paletteIndex = getColourMap(colour).lighter;
                GfxDrawSpriteSolid(rt, image, pos + ScreenCoordsXY{ 1, 1 }, paletteIndex);

                // Draw greyed out (dark)
                colour = colours[widget.colour].colour;
                paletteIndex = getColourMap(colour).midLight;
                GfxDrawSpriteSolid(rt, image, pos, paletteIndex);
            }
            else
            {
                GfxDrawSprite(rt, image, pos);
            }
        }

        void DrawDropdownButtons(RenderTarget& rt, WidgetIndex floorWidgetIndex, WidgetIndex edgeWidgetIndex)
        {
            auto& objManager = GetContext()->GetObjectManager();
            const auto* surfaceObj = objManager.GetLoadedObject<TerrainSurfaceObject>(_settings.landTexture);
            ImageId surfaceImage;
            if (surfaceObj != nullptr)
            {
                surfaceImage = ImageId(surfaceObj->IconImageId);
                if (surfaceObj->Colour != kColourNull)
                {
                    surfaceImage = surfaceImage.WithPrimary(surfaceObj->Colour);
                }
            }

            ImageId edgeImage;
            const auto* edgeObj = objManager.GetLoadedObject<TerrainEdgeObject>(_settings.edgeTexture);
            if (edgeObj != nullptr)
            {
                edgeImage = ImageId(edgeObj->IconImageId);
            }

            DrawDropdownButton(rt, floorWidgetIndex, surfaceImage);
            DrawDropdownButton(rt, edgeWidgetIndex, edgeImage);
        }

        void TerrainPrepareDraw()
        {
            setCheckboxValue(WIDX_RANDOM_TERRAIN, _randomTerrain != 0);
            setCheckboxValue(WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES, _settings.smoothTileEdges);

            // only allow floor and wall texture options if random terrain is disabled
            setWidgetEnabled(WIDX_FLOOR_TEXTURE, !_randomTerrain);
            setWidgetEnabled(WIDX_WALL_TEXTURE, !_randomTerrain);

            // Max land height option is irrelevant for flatland
            setWidgetEnabled(WIDX_HEIGHTMAP_HIGH, _settings.algorithm != MapGenerator::Algorithm::blank);

            // only offer terrain edge smoothing if we don't use flatland terrain
            setWidgetEnabled(WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES, _settings.algorithm != MapGenerator::Algorithm::blank);
        }

        void TerrainDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);
            DrawDropdownButtons(rt, WIDX_FLOOR_TEXTURE, WIDX_WALL_TEXTURE);

            const auto enabledColour = colours[1];
            const auto disabledColour = enabledColour.withFlag(ColourFlag::inset, true);

            // Floor texture label
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_FLOOR_TEXTURE].top + 1 }, STR_TERRAIN_LABEL,
                { enabledColour });

            // Minimum land height label and value
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_HEIGHTMAP_LOW].top + 1 }, STR_MAPGEN_MIN_LAND_HEIGHT,
                { enabledColour });

            auto ft = Formatter();
            ft.Add<int32_t>(BaseZToMetres(_settings.heightmapLow));
            drawText(
                rt, windowPos + ScreenCoordsXY{ widgets[WIDX_HEIGHTMAP_LOW].left + 1, widgets[WIDX_HEIGHTMAP_LOW].top + 1 },
                STR_RIDE_LENGTH_ENTRY, ft, { enabledColour });

            const auto maxLandColour = isWidgetDisabled(WIDX_HEIGHTMAP_HIGH) ? disabledColour : enabledColour;

            // Maximum land height label and value
            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_HEIGHTMAP_HIGH].top + 1 }, STR_MAPGEN_MAX_LAND_HEIGHT,
                { maxLandColour });

            ft = Formatter();
            ft.Add<int32_t>(BaseZToMetres(_settings.heightmapHigh));
            drawText(
                rt, windowPos + ScreenCoordsXY{ widgets[WIDX_HEIGHTMAP_HIGH].left + 1, widgets[WIDX_HEIGHTMAP_HIGH].top + 1 },
                STR_RIDE_LENGTH_ENTRY, ft, { maxLandColour });
        }

#pragma endregion

#pragma region Water page

        void WaterMouseUp(WidgetIndex widgetIndex)
        {
            SharedMouseUp(widgetIndex);

            switch (widgetIndex)
            {
                case WIDX_WATER_LEVEL:
                {
                    Formatter ft;
                    ft.Add<int16_t>(kMinimumWaterHeight);
                    ft.Add<int16_t>(kMaximumWaterHeight);
                    WindowTextInputOpen(
                        this, WIDX_WATER_LEVEL, STR_WATER_LEVEL, STR_ENTER_WATER_LEVEL, ft, STR_FORMAT_INTEGER,
                        _settings.waterLevel, 6);
                    break;
                }

                case WIDX_ADD_BEACHES:
                {
                    _settings.beaches ^= true;
                    invalidate();
                    break;
                }
            }
        }

        void WaterMouseDown(WidgetIndex widgetIndex, Widget* widget)
        {
            switch (widgetIndex)
            {
                case WIDX_WATER_LEVEL_UP:
                    _settings.waterLevel = std::min<int32_t>(_settings.waterLevel + 2, kMaximumWaterHeight);
                    invalidate();
                    break;
                case WIDX_WATER_LEVEL_DOWN:
                    _settings.waterLevel = std::max<int32_t>(_settings.waterLevel - 2, kMinimumWaterHeight);
                    invalidate();
                    break;
            }
        }

        void WaterUpdate()
        {
            // Tab animation
            if (++currentFrame >= TabAnimationLoops[page])
                currentFrame = 0;
            invalidateWidget(WIDX_TAB_4);
        }

        void WaterTextInput(WidgetIndex widgetIndex, int32_t value)
        {
            switch (widgetIndex)
            {
                case WIDX_WATER_LEVEL:
                    _settings.waterLevel = value;
                    break;
            }

            invalidate();
        }

        void WaterPrepareDraw()
        {
            setCheckboxValue(WIDX_ADD_BEACHES, _settings.beaches != 0);
        }

        void WaterDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            const auto textColour = colours[1];

            drawText(
                rt, windowPos + ScreenCoordsXY{ 10, widgets[WIDX_WATER_LEVEL].top + 1 }, STR_WATER_LEVEL_LABEL, { textColour });

            auto ft = Formatter();
            ft.Add<int32_t>(BaseZToMetres(_settings.waterLevel));
            drawText(
                rt, windowPos + ScreenCoordsXY{ widgets[WIDX_WATER_LEVEL].left + 1, widgets[WIDX_WATER_LEVEL].top + 1 },
                STR_RIDE_LENGTH_ENTRY, ft, { colours[1] });
        }

#pragma endregion

    public:
        void onOpen() override
        {
            number = 0;

            setPage(WINDOW_MAPGEN_PAGE_BASE);
            invalidate();
            initScrollWidgets();

            _heightmapLoaded = false;
        }

        void onClose() override
        {
            MapGenerator::UnloadHeightmapImage();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseMouseUp(widgetIndex);
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsMouseUp(widgetIndex);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainMouseUp(widgetIndex);
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterMouseUp(widgetIndex);
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseMouseDown(widgetIndex, &widgets[widgetIndex]);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainMouseDown(widgetIndex, &widgets[widgetIndex]);
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterMouseDown(widgetIndex, &widgets[widgetIndex]);
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsMouseDown(widgetIndex, &widgets[widgetIndex]);
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseDropdown(widgetIndex, selectedIndex);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainDropdown(widgetIndex, selectedIndex);
            }
        }

        void onUpdate() override
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseUpdate();
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsUpdate();
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterUpdate();
            }
        }

        void onPrepareDraw() override
        {
            bool isHeightMapImage = _settings.algorithm == MapGenerator::Algorithm::heightmapImage;
            setWidgetDisabled(WIDX_MAP_GENERATE, isHeightMapImage && !_heightmapLoaded);

            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BasePrepareDraw();
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsPrepareDraw();
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainPrepareDraw();
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterPrepareDraw();
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseDraw(rt);
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsDraw(rt);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainDraw(rt);
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterDraw(rt);
            }
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            auto strText = std::string(text);
            char* end;

            // Convert text to integer value
            int32_t value{};
            if (page == WINDOW_MAPGEN_PAGE_BASE && widgetIndex == WIDX_SIMPLEX_BASE_FREQ)
                value = 100 * strtof(strText.c_str(), &end);
            else
                value = strtol(strText.c_str(), &end, 10);

            if (*end != '\0')
                return;

            // Take care of unit conversion
            int32_t rawValue = value;
            if (page != WINDOW_MAPGEN_PAGE_BASE)
            {
                switch (Config::Get().general.measurementFormat)
                {
                    case MeasurementFormat::imperial:
                        value = FeetToMetres(value);
                        [[fallthrough]];

                    default:
                        value = std::clamp(MetresToBaseZ(value), kMinimumLandHeight, kMaximumLandHeight);
                        break;
                }
            }

            // Pass on to the actual properties
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    return BaseTextInput(widgetIndex, value);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return TerrainTextInput(widgetIndex, value);
                case WINDOW_MAPGEN_PAGE_WATER:
                    return WaterTextInput(widgetIndex, value);
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    return ForestsTextInput(widgetIndex, rawValue, value);
            }
        }

        void afterLoadingHeightMap(ModalResult result, const utf8* path)
        {
            if (result == ModalResult::ok)
            {
                if (!MapGenerator::LoadHeightmapImage(path))
                {
                    // TODO: Display error popup
                    Accessibility::ScreenReaderSpeak(Vocab::kMapGenImageFailed);
                    return;
                }

                // The window needs to be open while using the map
                _heightmapLoaded = true;
                _heightmapFilename = fs::u8path(path).filename().string();
                _settings.algorithm = MapGenerator::Algorithm::heightmapImage;
                setPage(WINDOW_MAPGEN_PAGE_BASE);
            }
        }

        void onResize() override
        {
            WindowSetResize(*this, kWindowSize, kWindowSize);
        }

#pragma region Accessibility
        // ---- graph accessibility recipe ----
        //
        // Each page as a form, its controls in the order they stand on screen, named with the game's
        // own captions, and Generate last on every page. Spinners: Left/Right press the game's own
        // arrows, Shift+Left/Right press them ten times (the settings live in this window, so each
        // press has landed before the next), and Enter types a value. Lists open with Enter. Heights
        // are read as the window shows them, then as the map cursor's elevation.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            // Re-derive the list state from reality every build - it can be dismissed behind our
            // back, and a stale flag would strand navigation inside a list that is gone.
            auto* windowMgr = GetWindowManager();
            if (_axDropdownOpen && (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr))
                _axDropdownOpen = false;

            onPrepareDraw(); // pressed, disabled and hidden states, and the source's caption

            if (_axDropdownOpen)
            {
                b.PushContext(axComboLabel(_axDropdownOwner), Vocab::kRoleDropdown);
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    const auto& item = gDropdown.items[i];
                    if (item.isSeparator())
                        continue;
                    // The source list carries words; the land and edge style lists are pictures, named
                    // by their tooltips, with the current style as the list's default item.
                    const bool image = item.type == Dropdown::ItemType::image;
                    NodeVtable vt;
                    vt.announcements.emplace_back(NodeAnnouncement::Static(image ? axText(item.tooltip) : axStrip(item.text)));
                    if (image ? i == gDropdown.defaultIndex : item.isChecked())
                        vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kSelected, AnnouncementKinds::kSelected));
                    vt.onActivate = [this, i]() { axCommitDropdown(i); };
                    // Keep the engine's drawn highlight on the keyboard's row; it clears that field from
                    // the mouse every tick, so it has to be re-asserted.
                    vt.onFocus = [i]() { Accessibility::SetKeyboardDropdownIndex(i); };
                    b.AddItem(ControlId::Structural("mg:dd:" + std::to_string(i)), std::move(vt));
                }
                b.PopContext();
                return;
            }

            b.PushContext(Vocab::kMapGenPages[page]);
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    axSpinner(b, WIDX_MAP_SIZE_X);
                    axSpinner(b, WIDX_MAP_SIZE_Y);
                    axCheckbox(b, WIDX_MAP_SIZE_LINK, Vocab::kMapKeepSquare);
                    axCombo(b, WIDX_HEIGHTMAP_SOURCE_DROPDOWN, WIDX_HEIGHTMAP_SOURCE);
                    if (widgets[WIDX_SIMPLEX_GROUP].isVisible())
                    {
                        b.PushContext(axText(STR_MAPGEN_SIMPLEX_NOISE));
                        axSpinner(b, WIDX_SIMPLEX_BASE_FREQ);
                        axSpinner(b, WIDX_SIMPLEX_OCTAVES);
                        b.PopContext();
                    }
                    if (widgets[WIDX_HEIGHTMAP_GROUP].isVisible())
                    {
                        b.PushContext(axText(STR_MAPGEN_SELECT_HEIGHTMAP));
                        axBrowse(b);
                        axCheckbox(b, WIDX_HEIGHTMAP_NORMALIZE);
                        axCheckbox(b, WIDX_HEIGHTMAP_SMOOTH_HEIGHTMAP);
                        axSpinner(b, WIDX_HEIGHTMAP_STRENGTH);
                        b.PopContext();
                    }
                    break;
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    axSpinner(b, WIDX_HEIGHTMAP_LOW);
                    axSpinner(b, WIDX_HEIGHTMAP_HIGH);
                    axCombo(b, WIDX_FLOOR_TEXTURE, WIDX_FLOOR_TEXTURE);
                    axCombo(b, WIDX_WALL_TEXTURE, WIDX_WALL_TEXTURE);
                    axCheckbox(b, WIDX_RANDOM_TERRAIN);
                    axCheckbox(b, WIDX_HEIGHTMAP_SMOOTH_TILE_EDGES);
                    break;
                case WINDOW_MAPGEN_PAGE_WATER:
                    axSpinner(b, WIDX_WATER_LEVEL);
                    axCheckbox(b, WIDX_ADD_BEACHES);
                    break;
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    axCheckbox(b, WIDX_FORESTS_PLACE_TREES);
                    axSpinner(b, WIDX_TREE_LAND_RATIO);
                    axSpinner(b, WIDX_TREE_ALTITUDE_MIN);
                    axSpinner(b, WIDX_TREE_ALTITUDE_MAX);
                    break;
            }
            axGenerate(b);
            b.PopContext();
        }

        // Tab/Shift+Tab: the next or previous page, through the game's own tabs, wrapping round.
        void AxChangePage(int32_t delta)
        {
            if (_axDropdownOpen)
                axCloseDropdown();
            const int32_t next = (page + (delta > 0 ? 1 : -1) + WINDOW_MAPGEN_PAGE_COUNT) % WINDOW_MAPGEN_PAGE_COUNT;
            onMouseUp(static_cast<WidgetIndex>(WIDX_TAB_1 + next));
        }

        std::string AxHelp() const
        {
            return Vocab::Help::MapGen(page);
        }

        // Escape closes an open list first; otherwise the navigator closes the window.
        bool AxEscape()
        {
            if (!_axDropdownOpen)
                return false;
            axCloseDropdown();
            axSuggestFocus(_axDropdownOwner);
            return true;
        }

    private:
        bool _axDropdownOpen = false;
        WidgetIndex _axDropdownOpener = 0; // the widget whose press opened the list, and hears the choice
        WidgetIndex _axDropdownOwner = 0;  // the control focus returns to

        // How a spinner's value is shown and typed. Heights are stored as base heights.
        enum class AxKind : uint8_t
        {
            count,
            frequency, // stored in hundredths, shown with two decimals
            height,
            percent,
        };

        struct AxSpec
        {
            AxKind kind;
            int32_t min; // the limits the game's own text prompt keeps to
            int32_t max;
            int32_t current;
        };

        // Node keys carry the page: widget indices are reused from page to page.
        std::string axKey(WidgetIndex w) const
        {
            return "mg:" + std::to_string(page) + ":" + std::to_string(w);
        }

        static std::string axStrip(const std::string& text)
        {
            std::string out;
            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == '{')
                {
                    const auto end = text.find('}', i);
                    if (end == std::string::npos)
                        break;
                    i = end;
                    continue;
                }
                out.push_back(text[i]);
            }
            return out;
        }

        // A game caption as speech: format codes and a trailing colon dropped.
        static std::string axText(StringId id)
        {
            if (id == kStringIdNone || id == kStringIdEmpty)
                return {};
            std::string out = axStrip(FormatStringID(id));
            while (!out.empty() && (out.back() == ':' || out.back() == ' '))
                out.pop_back();
            return out;
        }

        std::optional<Accessibility::Graph::GraphRect> axRect(WidgetIndex w) const
        {
            if (w >= widgets.size() || !widgets[w].isVisible())
                return std::nullopt;
            const auto& wd = widgets[w];
            return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1,
                                                    wd.height() + 1 };
        }

        // Read fresh each time: a press changes the window's settings at once, before the next build.
        std::string axUnavailable(WidgetIndex w)
        {
            onPrepareDraw();
            return isWidgetDisabled(w) ? Vocab::kUnavailable : "";
        }

        void axSuggestFocus(WidgetIndex w)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::mapgen).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural(axKey(w));
        }

        // ---- checkboxes ----

        // Enter does what a click does: the game handles some of these on the press and others on
        // the release.
        void axCheckbox(Accessibility::Graph::GraphBuilder& b, WidgetIndex w, const char* label = nullptr)
        {
            using namespace Accessibility::Graph;
            if (!widgets[w].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(label != nullptr ? std::string(label) : axText(widgets[w].text)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleCheckbox, AnnouncementKinds::kRole));
            vt.announcements.emplace_back(
                [this, w]() {
                    onPrepareDraw();
                    return std::string(Vocab::Checked(isWidgetPressed(w)));
                },
                true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, w]() { return axUnavailable(w); }, true);
            vt.onActivate = [this, w]() {
                if (isWidgetDisabled(w))
                {
                    Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                    return;
                }
                onMouseDown(w);
                onMouseUp(w);
            };
            vt.focusRect = [this, w]() { return axRect(w); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(w)), std::move(vt));
        }

        // ---- combo boxes ----

        std::string axComboLabel(WidgetIndex owner) const
        {
            if (page == WINDOW_MAPGEN_PAGE_BASE)
                return axText(STR_HEIGHTMAP_SOURCE);
            return owner == WIDX_FLOOR_TEXTURE ? Vocab::kMapGenLandStyle : Vocab::kMapGenEdgeStyle;
        }

        // The value the game shows: the source's caption, or the style's picture, by its name.
        std::string axComboValue(WidgetIndex owner)
        {
            if (page == WINDOW_MAPGEN_PAGE_BASE)
            {
                onPrepareDraw();
                return axText(widgets[WIDX_HEIGHTMAP_SOURCE].text);
            }
            auto& objManager = GetContext()->GetObjectManager();
            StringId name = kStringIdNone;
            if (owner == WIDX_FLOOR_TEXTURE)
            {
                if (const auto* obj = objManager.GetLoadedObject<TerrainSurfaceObject>(
                        static_cast<ObjectEntryIndex>(_settings.landTexture));
                    obj != nullptr)
                    name = obj->NameStringId;
            }
            else if (const auto* obj = objManager.GetLoadedObject<TerrainEdgeObject>(
                         static_cast<ObjectEntryIndex>(_settings.edgeTexture));
                     obj != nullptr)
                name = obj->NameStringId;
            const std::string text = axText(name);
            return text.empty() ? std::string(Vocab::kNothingChosen) : text;
        }

        // opener is the widget whose press shows the list (the chevron, or the style picture itself);
        // owner is the control it belongs to.
        void axCombo(Accessibility::Graph::GraphBuilder& b, WidgetIndex opener, WidgetIndex owner)
        {
            using namespace Accessibility::Graph;
            if (!widgets[opener].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axComboLabel(owner)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleDropdown, AnnouncementKinds::kRole));
            vt.announcements.emplace_back([this, owner]() { return axComboValue(owner); }, true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, opener]() { return axUnavailable(opener); }, true);
            vt.onActivate = [this, opener, owner]() {
                if (isWidgetDisabled(opener))
                {
                    Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                    return;
                }
                if (Accessibility::OpenWidgetDropdownFromKeyboard(*this, opener))
                {
                    _axDropdownOpen = true;
                    _axDropdownOpener = opener;
                    _axDropdownOwner = owner;
                }
            };
            vt.focusRect = [this, owner]() { return axRect(owner); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(owner)), std::move(vt));
        }

        void axCloseDropdown()
        {
            Accessibility::CloseWidgetDropdownFromKeyboard();
            _axDropdownOpen = false;
        }

        // As a click on a list item: the window hears the choice, then the list closes.
        void axCommitDropdown(int32_t idx)
        {
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator()
                && !gDropdown.items[idx].isDisabled();
            if (valid)
                onDropdown(_axDropdownOpener, idx);
            axCloseDropdown();
            axSuggestFocus(_axDropdownOwner);
        }

        // ---- spinners ----

        // What a spinner holds, and the limits the game's own prompt for it keeps to.
        std::optional<AxSpec> axSpec(WidgetIndex w) const
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    switch (w)
                    {
                        case WIDX_MAP_SIZE_X:
                            return AxSpec{ AxKind::count, kMinimumMapSizePractical, kMaximumMapSizePractical,
                                           _settings.mapSize.x - 2 };
                        case WIDX_MAP_SIZE_Y:
                            return AxSpec{ AxKind::count, kMinimumMapSizePractical, kMaximumMapSizePractical,
                                           _settings.mapSize.y - 2 };
                        case WIDX_SIMPLEX_BASE_FREQ:
                            return AxSpec{ AxKind::frequency, 0, 1000, _settings.simplex_base_freq };
                        case WIDX_SIMPLEX_OCTAVES:
                            return AxSpec{ AxKind::count, 1, 10, _settings.simplex_octaves };
                        case WIDX_HEIGHTMAP_STRENGTH:
                            return AxSpec{ AxKind::count, 1, 20, static_cast<int32_t>(_settings.smooth_strength) };
                    }
                    break;
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    if (w == WIDX_HEIGHTMAP_LOW)
                        return AxSpec{ AxKind::height, kMinimumLandHeight, kMaximumLandHeight, _settings.heightmapLow };
                    if (w == WIDX_HEIGHTMAP_HIGH)
                        return AxSpec{ AxKind::height, kMinimumLandHeight, kMaximumLandHeight, _settings.heightmapHigh };
                    break;
                case WINDOW_MAPGEN_PAGE_WATER:
                    if (w == WIDX_WATER_LEVEL)
                        return AxSpec{ AxKind::height, kMinimumWaterHeight, kMaximumWaterHeight, _settings.waterLevel };
                    break;
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    switch (w)
                    {
                        case WIDX_TREE_LAND_RATIO:
                            return AxSpec{ AxKind::percent, 1, 50, _settings.treeToLandRatio };
                        case WIDX_TREE_ALTITUDE_MIN:
                            return AxSpec{ AxKind::height, kMinimumLandHeight, kMaximumLandHeight, _settings.minTreeAltitude };
                        case WIDX_TREE_ALTITUDE_MAX:
                            return AxSpec{ AxKind::height, kMinimumLandHeight, kMaximumLandHeight, _settings.maxTreeAltitude };
                    }
                    break;
            }
            return std::nullopt;
        }

        // Spinner captions are drawn beside them, not held by a widget.
        std::string axSpinLabel(WidgetIndex w) const
        {
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    if (w == WIDX_MAP_SIZE_X)
                        return Vocab::kMapSizeX;
                    if (w == WIDX_MAP_SIZE_Y)
                        return Vocab::kMapSizeY;
                    if (w == WIDX_SIMPLEX_BASE_FREQ)
                        return axText(STR_MAPGEN_SIMPLEX_NOISE_BASE_FREQUENCY);
                    if (w == WIDX_SIMPLEX_OCTAVES)
                        return axText(STR_MAPGEN_SIMPLEX_NOISE_OCTAVES);
                    return axText(STR_MAPGEN_SMOOTH_STRENGTH);
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    return axText(w == WIDX_HEIGHTMAP_LOW ? STR_MAPGEN_MIN_LAND_HEIGHT : STR_MAPGEN_MAX_LAND_HEIGHT);
                case WINDOW_MAPGEN_PAGE_WATER:
                    return axText(STR_WATER_LEVEL_LABEL);
                default:
                    if (w == WIDX_TREE_LAND_RATIO)
                        return axText(STR_MAPGEN_TREE_TO_LAND_RATIO);
                    return axText(w == WIDX_TREE_ALTITUDE_MIN ? STR_MAPGEN_TREE_MIN_ALTITUDE : STR_MAPGEN_TREE_MAX_ALTITUDE);
            }
        }

        static bool axImperial()
        {
            return Config::Get().general.measurementFormat == MeasurementFormat::imperial;
        }

        // A height as the window draws it: metres or feet.
        static std::string axLength(int32_t baseZ)
        {
            return axStrip(
                FormatStringID(STR_RIDE_LENGTH_ENTRY, static_cast<int32_t>(BaseZToMetres(static_cast<int16_t>(baseZ)))));
        }

        // A value as the game draws it; heights also as the map cursor's elevation.
        static std::string axShown(AxKind kind, int32_t v, bool withElevation)
        {
            switch (kind)
            {
                case AxKind::count:
                    return std::to_string(v);
                case AxKind::frequency:
                {
                    char buffer[16];
                    std::snprintf(buffer, sizeof(buffer), "%d.%02d", v / 100, v % 100);
                    return buffer;
                }
                case AxKind::percent:
                    return axStrip(FormatStringID(STR_MAPGEN_TREE_TO_LAND_RATIO_PCT, static_cast<uint16_t>(v)));
                case AxKind::height:
                    if (!withElevation)
                        return axLength(v);
                    return axLength(v) + ", " + Vocab::ElevationPart(Accessibility::ElevationText(v));
            }
            return {};
        }

        std::string axSpinValue(WidgetIndex w) const
        {
            const auto spec = axSpec(w);
            return spec.has_value() ? axShown(spec->kind, spec->current, true) : std::string();
        }

        // The value as the player would type it, to start the prompt with.
        static std::string axEditable(AxKind kind, int32_t v)
        {
            switch (kind)
            {
                case AxKind::frequency:
                    return axShown(kind, v, false);
                case AxKind::height:
                {
                    const int32_t metres = BaseZToMetres(static_cast<int16_t>(v));
                    return std::to_string(axImperial() ? MetresToFeet(metres) : metres);
                }
                default:
                    return std::to_string(v);
            }
        }

        // Typed text as a number: units and percent signs are ignored, a comma counts as a decimal
        // point, and heights may be negative.
        static std::optional<double> axParseNumber(std::string_view text)
        {
            std::string number;
            for (const char c : text)
            {
                if ((c >= '0' && c <= '9') || c == '-')
                    number.push_back(c);
                else if (c == '.' || c == ',')
                    number.push_back('.');
            }
            if (number.empty())
                return std::nullopt;
            char* end = nullptr;
            const double value = std::strtod(number.c_str(), &end);
            if (end == number.c_str() || *end != '\0')
                return std::nullopt;
            return value;
        }

        // A typed number to the stored value. A height goes through the window's own conversion
        // (MetresToBaseZ: (metres / 1.5 + 7) * 2), to the nearest whole land step, the step its
        // arrows move by.
        static int32_t axStored(AxKind kind, double typed)
        {
            switch (kind)
            {
                case AxKind::frequency:
                    return static_cast<int32_t>(std::lround(typed * 100.0));
                case AxKind::height:
                {
                    const double metres = axImperial() ? FeetToMetres(static_cast<int32_t>(std::lround(typed))) : typed;
                    const double baseZ = (metres / 1.5 + 7.0) * 2.0;
                    return static_cast<int32_t>(std::lround(baseZ / 2.0)) * 2;
                }
                default:
                    return static_cast<int32_t>(std::lround(typed));
            }
        }

        void axSpinner(Accessibility::Graph::GraphBuilder& b, WidgetIndex w)
        {
            using namespace Accessibility::Graph;
            if (!widgets[w].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axSpinLabel(w)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleSpinner, AnnouncementKinds::kRole));
            vt.announcements.emplace_back([this, w]() { return axSpinValue(w); }, true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, w]() { return axUnavailable(w); }, true);
            vt.onAdjust = [this, w](int32_t sign, bool large) { axAdjust(w, sign, large); };
            vt.onActivate = [this, w]() { axOpenTyping(w); };
            vt.focusRect = [this, w]() { return axRect(w); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(w)), std::move(vt));
        }

        // The game's own arrow, once or ten times. Each press lands at once (the settings are this
        // window's), so ten in a row step ten times. The new value is read by the navigator; at a
        // limit nothing changes, so the value that stands is said instead of silence.
        void axAdjust(WidgetIndex w, int32_t sign, bool large)
        {
            if (isWidgetDisabled(w))
            {
                Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                return;
            }
            const auto arrow = static_cast<WidgetIndex>(sign > 0 ? w + 1 : w + 2); // up : down
            const std::string before = axSpinValue(w);
            for (int32_t i = 0; i < (large ? 10 : 1); i++)
                onMouseDown(arrow);
            if (axSpinValue(w) == before)
                Accessibility::ScreenReaderSpeak(before);
        }

        void axOpenTyping(WidgetIndex w)
        {
            if (isWidgetDisabled(w))
            {
                Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                return;
            }
            const auto spec = axSpec(w);
            if (!spec.has_value())
                return;
            const int32_t pageAtOpen = page;
            WindowTextInputOpen(
                axSpinLabel(w),
                Vocab::TypeValueRange(axShown(spec->kind, spec->min, false), axShown(spec->kind, spec->max, false)),
                axEditable(spec->kind, spec->current), 16,
                [w, pageAtOpen](std::string_view text) {
                    auto* windowMgr = GetWindowManager();
                    auto* self = windowMgr == nullptr ? nullptr
                                                      : static_cast<MapGenWindow*>(windowMgr->FindByClass(WindowClass::mapgen));
                    if (self != nullptr && self->page == pageAtOpen)
                        self->axApplyTyped(w, text);
                },
                []() { Accessibility::ScreenReaderSpeak(Vocab::kCancelled); });
        }

        // A typed value, kept within the limits; outside them, the range is said instead and nothing
        // changes. Inside them it goes to the window's own text handler, which applies it as typing
        // into the game's prompt does (keeping minimum and maximum heights in order, square maps
        // square).
        void axApplyTyped(WidgetIndex w, std::string_view text)
        {
            const auto spec = axSpec(w);
            if (!spec.has_value())
                return;
            const auto typed = axParseNumber(text);
            if (!typed.has_value())
            {
                Accessibility::ScreenReaderSpeak(Vocab::kNotANumber);
                return;
            }
            const int32_t v = axStored(spec->kind, *typed);
            if (v < spec->min || v > spec->max)
            {
                Accessibility::ScreenReaderSpeak(Vocab::ValueOutOfRange(
                    axSpinLabel(w), axShown(spec->kind, spec->min, false), axShown(spec->kind, spec->max, false)));
                return;
            }
            switch (page)
            {
                case WINDOW_MAPGEN_PAGE_BASE:
                    if (w == WIDX_MAP_SIZE_X)
                        _resizeDirection = ResizeDirection::x;
                    else if (w == WIDX_MAP_SIZE_Y)
                        _resizeDirection = ResizeDirection::y;
                    BaseTextInput(w, v);
                    break;
                case WINDOW_MAPGEN_PAGE_TERRAIN:
                    TerrainTextInput(w, v);
                    break;
                case WINDOW_MAPGEN_PAGE_WATER:
                    WaterTextInput(w, v);
                    break;
                case WINDOW_MAPGEN_PAGE_FORESTS:
                    ForestsTextInput(w, v, v); // the ratio reads the first, the altitudes the second
                    break;
            }
        }

        // ---- buttons ----

        // Browse opens the file browser for a height map image, with its first file selected, as
        // opening it from the title menu does.
        void axBrowse(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            if (!widgets[WIDX_HEIGHTMAP_BROWSE].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axText(STR_BROWSE)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleButton, AnnouncementKinds::kRole));
            vt.announcements.emplace_back(
                [this]() {
                    const char* file = _heightmapLoaded ? _heightmapFilename.c_str() : LanguageGetString(STR_MAPGEN_NONE_SELECTED);
                    return axStrip(FormatStringID(STR_MAPGEN_CURRENT_HEIGHTMAP_FILE, file));
                },
                true, AnnouncementKinds::kValue);
            vt.onActivate = [this]() {
                onMouseUp(WIDX_HEIGHTMAP_BROWSE);
                if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                    if (auto* browser = windowMgr->FindByClass(WindowClass::loadsave); browser != nullptr)
                        browser->onAccessibilityAction(AccessibilityAction::moveDown);
            };
            vt.focusRect = [this]() { return axRect(WIDX_HEIGHTMAP_BROWSE); };
            b.AddItem(ControlId::Structural(axKey(WIDX_HEIGHTMAP_BROWSE)), std::move(vt));
        }

        // Generate runs the game's own Generate, which replaces the whole map at once, then says
        // what it made. While the map already has a park entrance or guest starts, the button says
        // that it replaces everything.
        void axGenerate(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axText(STR_MAPGEN_ACTION_GENERATE)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleButton, AnnouncementKinds::kRole));
            vt.announcements.emplace_back(
                []() {
                    const auto& gameState = getGameState();
                    const bool built = !gameState.park.entrances.empty() || !gameState.peepSpawns.empty();
                    return std::string(built ? Vocab::kMapGenReplacesMap : "");
                },
                true);
            vt.announcements.emplace_back([this]() { return axUnavailable(WIDX_MAP_GENERATE); }, true);
            vt.onActivate = [this]() {
                if (isWidgetDisabled(WIDX_MAP_GENERATE))
                {
                    // Disabled only while the source is an image and none is loaded.
                    Accessibility::ScreenReaderSpeak(Vocab::kMapGenNeedImage);
                    return;
                }
                onMouseUp(WIDX_MAP_GENERATE);
                Accessibility::ResetMapCursorForNewMap();
                Accessibility::ScreenReaderSpeak(axSummary());
            };
            vt.focusRect = [this]() { return axRect(WIDX_MAP_GENERATE); };
            b.AddItem(ControlId::Structural("mg:" + std::to_string(page) + ":generate"), std::move(vt));
        }

        // What a sighted player sees of the new map at a glance: its size, the lowest and highest
        // land (as the map cursor's elevation), how much is under water and how many trees it has.
        static std::string axSummary()
        {
            const auto& gameState = getGameState();
            int32_t lowest = std::numeric_limits<int32_t>::max();
            int32_t highest = std::numeric_limits<int32_t>::min();
            int64_t tiles = 0;
            int64_t water = 0;
            int64_t trees = 0;
            for (int32_t y = 1; y < gameState.mapSize.y - 1; y++)
            {
                for (int32_t x = 1; x < gameState.mapSize.x - 1; x++)
                {
                    const TileCoordsXY tile{ x, y };
                    const auto* surface = MapGetSurfaceElementAt(tile);
                    if (surface == nullptr)
                        continue;
                    tiles++;
                    lowest = std::min<int32_t>(lowest, surface->baseHeight);
                    highest = std::max<int32_t>(highest, surface->baseHeight);
                    if (surface->getWaterHeight() > 0)
                        water++;
                    for (const auto* el : TileElementsView<SmallSceneryElement>(tile.ToCoordsXY()))
                        if (!el->isGhost())
                            trees++;
                }
            }
            if (tiles == 0)
                return axText(STR_MAPGEN_ACTION_GENERATE);
            return Vocab::MapGenerated(
                gameState.mapSize.x - 2, gameState.mapSize.y - 2, Accessibility::ElevationText(lowest),
                Accessibility::ElevationText(highest), water, (water * 100) / tiles, trees);
        }
#pragma endregion
    };

    void RegisterMapGenGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::mapgen;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<MapGenWindow&>(w).BuildAccessGraph(b); };
        screen.onTabKey = [](WindowBase& w, int32_t dir) {
            static_cast<MapGenWindow&>(w).AxChangePage(dir);
            return true;
        };
        screen.onEscape = [](WindowBase& w) { return static_cast<MapGenWindow&>(w).AxEscape(); };
        screen.screenName = []() { return std::string(Vocab::kMapGenerator); };
        screen.helpText = [](WindowBase& w) { return static_cast<MapGenWindow&>(w).AxHelp(); };
        // A form: Left/Right belong to the spinners. Pages are Tab/Shift+Tab.
        screen.sideArrowsChangePage = false;
        RegisterGraphScreen(std::move(screen));
    }

    WindowBase* MapgenOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<MapGenWindow>(
            WindowClass::mapgen, kWindowSize,
            { WindowFlag::higherContrastOnPress, WindowFlag::autoPosition, WindowFlag::centreScreen });
    }

    static void HeightmapLoadsaveCallback(ModalResult result, const utf8* path)
    {
        auto* w = static_cast<MapGenWindow*>(MapgenOpen());
        w->afterLoadingHeightMap(result, path);
    }
} // namespace OpenRCT2::Ui::Windows
