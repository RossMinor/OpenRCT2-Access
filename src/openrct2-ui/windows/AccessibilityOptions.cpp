/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// Settings window for the blind-accessibility mod. Opened with Ctrl+F1. It is drawn like a native
// OpenRCT2 window (so sighted helpers can read it) and is fully keyboard/screen-reader navigable.
// It exposes: a slider for the accessibility sound-cue volume (in 5% increments), a control for how
// often the map-cursor step sounds play (every step / on change / off), and a button that opens the
// author's Patreon page.

#include <algorithm>
#include <cmath>
#include <optional>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/StringIds.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>
#include <string>

namespace OpenRCT2::Ui::Windows
{
    static constexpr ScreenSize kWindowSize = { 400, 204 };
    static constexpr int32_t kVolumeStep = 5;
    static constexpr const char* kPatreonUrl = "https://www.patreon.com/rossminor";
    static constexpr uint8_t kStepSoundModeCount = 3;
    static constexpr uint8_t kTileSpeechModeCount = 3;
    static constexpr uint8_t kTileReadingOrderCount = 2;

    // Curated high-contrast choices for the visible focus indicator. Values are Drawing::Colour enum
    // values (see drawing/Colour.h); stored in config as sound.accessibilityFocusColour.
    struct FocusColourOption
    {
        uint8_t value;
        const char* name;
    };
    static constexpr FocusColourOption kFocusColours[] = {
        { 18, "Yellow" },      { 2, "White" },        { 28, "Bright red" },
        { 14, "Bright green" }, { 30, "Bright pink" }, { 7, "Light blue" },
        { 5, "Bright purple" }, { 20, "Light orange" }, { 0, "Black" },
    };
    static constexpr int32_t kFocusColourCount = static_cast<int32_t>(std::size(kFocusColours));

    enum WindowAccessibilityOptionsWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_VOLUME_SLIDER,
        WIDX_STEP_MODE,
        WIDX_TILE_MODE,
        WIDX_TILE_ORDER,
        WIDX_FOCUS_COLOUR,
        WIDX_MENU_MUSIC,
        WIDX_MENU_SOUND,
        WIDX_SUPPORT_BUTTON,
    };

    // clang-format off
    static constexpr auto kWidgets = makeWidgets(
        makeWindowShim(kStringIdNone, kWindowSize),
        makeWidget({ 180, 24 }, { 150, 13 },                    WidgetType::scroll, WindowColour::secondary, SCROLL_HORIZONTAL), // Cue volume slider
        makeWidget({ 150, 44 }, { 180, 14 },                    WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Step sound mode (cycles on click)
        makeWidget({ 150, 64 }, { 180, 14 },                    WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Tile reading mode (cycles on click)
        makeWidget({ 150, 84 }, { 180, 14 },                    WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Tile reading order (cycles on click)
        makeWidget({ 150, 104 }, { 180, 14 },                   WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Focus indicator colour (cycles on click)
        makeWidget({ 150, 124 }, { 180, 14 },                   WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Menu music volume (adjusts on click)
        makeWidget({ 150, 144 }, { 180, 14 },                   WidgetType::button, WindowColour::secondary, kStringIdNone     ), // Menu sound volume (adjusts on click)
        makeWidget({   8, 170 }, { kWindowSize.width - 16, 20 }, WidgetType::button, WindowColour::secondary, kStringIdNone     )  // Support Ross button
    );
    // clang-format on

    static const char* stepModeName(uint8_t mode)
    {
        switch (mode)
        {
            case 0:
                return "Every step";
            case 1:
                return "On change";
            default:
                return "Off";
        }
    }

    static const char* tileModeName(uint8_t mode)
    {
        switch (mode)
        {
            case 0:
                return "Every tile";
            case 1:
                return "On change";
            default:
                return "Off";
        }
    }

    static const char* tileReadingOrderName(uint8_t order)
    {
        return order == 1 ? "Highest to lowest" : "Lowest to highest";
    }

    // Index of the current focus-indicator colour within kFocusColours (0 if the stored value isn't
    // one of the presets, e.g. after an older config).
    static int32_t focusColourIndex(uint8_t value)
    {
        for (int32_t i = 0; i < kFocusColourCount; i++)
            if (kFocusColours[i].value == value)
                return i;
        return 0;
    }

    static const char* focusColourName(uint8_t value)
    {
        return kFocusColours[focusColourIndex(value)].name;
    }

    class AccessibilityOptionsWindow final : public Window
    {
    private:
        // Focused control: 0 = cue volume, 1 = step sounds, 2 = tile reading, 3 = tile reading order,
        // 4 = focus colour, 5 = menu music volume, 6 = menu sound volume, 7 = support button.
        int32_t _accessIndex = 0;

        // The support button is a two-step confirm: the first Enter reads the thank-you message and
        // arms it; the second Enter actually opens the browser. Reset whenever focus moves away.
        bool _supportArmed = false;

        // Backing strings for the mode-button captions (setString keeps only a pointer).
        std::string _stepCaption;
        std::string _tileCaption;
        std::string _tileOrderCaption;
        std::string _focusCaption;
        std::string _menuMusicCaption;
        std::string _menuSoundCaption;

        // Backing storage for the caption / button captions (setString stores a pointer, so the
        // strings must outlive the window's draws).
        static const char* getTitleText()
        {
            static const std::string s = "Accessibility Sounds Volume";
            return s.c_str();
        }
        static const char* getSupportText()
        {
            static const std::string s = "Support Ross's work (opens Patreon)";
            return s.c_str();
        }

    public:
        void onOpen() override
        {
            setWidgets(kWidgets);
            widgets[WIDX_TITLE].setString(getTitleText());
            widgets[WIDX_SUPPORT_BUTTON].setString(getSupportText());
            initScrollWidgets();

            _accessIndex = 0;
            initialiseScrollPosition(WIDX_VOLUME_SLIDER, 0, Config::Get().sound.accessibilityCueVolume);

            Accessibility::ScreenReaderSpeak("Accessibility Sounds Volume. " + focusText());
        }

        void onPrepareDraw() override
        {
            // Refresh the mode buttons' captions from the current settings.
            _stepCaption = stepModeName(Config::Get().sound.accessibilityStepSoundMode);
            widgets[WIDX_STEP_MODE].setString(_stepCaption.c_str());
            _tileCaption = tileModeName(Config::Get().sound.accessibilityTileSpeechMode);
            widgets[WIDX_TILE_MODE].setString(_tileCaption.c_str());
            _tileOrderCaption = tileReadingOrderName(Config::Get().sound.accessibilityTileReadingOrder);
            widgets[WIDX_TILE_ORDER].setString(_tileOrderCaption.c_str());
            _focusCaption = focusColourName(Config::Get().sound.accessibilityFocusColour);
            widgets[WIDX_FOCUS_COLOUR].setString(_focusCaption.c_str());
            _menuMusicCaption = std::to_string(Config::Get().sound.titleMusicVolume) + "%";
            widgets[WIDX_MENU_MUSIC].setString(_menuMusicCaption.c_str());
            _menuSoundCaption = std::to_string(Config::Get().sound.titleSoundVolume) + "%";
            widgets[WIDX_MENU_SOUND].setString(_menuSoundCaption.c_str());
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_STEP_MODE:
                    cycleStepMode(1); // clicking advances to the next mode
                    break;
                case WIDX_TILE_MODE:
                    cycleTileMode(1);
                    break;
                case WIDX_TILE_ORDER:
                    cycleTileOrder(1);
                    break;
                case WIDX_FOCUS_COLOUR:
                    cycleFocusColour(1);
                    break;
                case WIDX_MENU_MUSIC:
                    adjustMenuMusicVolume(1); // clicking nudges it up
                    break;
                case WIDX_MENU_SOUND:
                    adjustMenuSoundVolume(1); // clicking nudges it up
                    break;
                case WIDX_SUPPORT_BUTTON:
                    openSupportPage();
                    break;
            }
        }

        void onUpdate() override
        {
            // Fold any mouse-drag of the slider back into the config, snapped to 5% increments.
            const uint8_t pct = snapVolume(GetScrollPercentage(widgets[WIDX_VOLUME_SLIDER], scrolls[0]));
            if (pct != Config::Get().sound.accessibilityCueVolume)
            {
                Config::Get().sound.accessibilityCueVolume = pct;
                Config::Save();
                invalidateWidget(WIDX_VOLUME_SLIDER);
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);

            // Slider label.
            const auto labelPos = windowPos + ScreenCoordsXY{ 8, 27 };
            drawText(rt, labelPos, "Accessibility sounds volume", { colours[1] });

            // Current percentage, just right of the slider.
            const auto pctText = std::to_string(Config::Get().sound.accessibilityCueVolume) + "%";
            const auto pctPos = windowPos + ScreenCoordsXY{ widgets[WIDX_VOLUME_SLIDER].right + 6, 27 };
            drawText(rt, pctPos, pctText, { colours[1] });

            // Step-sound mode label (the value is drawn on the button itself).
            const auto stepLabelPos = windowPos + ScreenCoordsXY{ 8, 47 };
            drawText(rt, stepLabelPos, "Step sounds", { colours[1] });

            // Tile-reading mode label.
            const auto tileLabelPos = windowPos + ScreenCoordsXY{ 8, 67 };
            drawText(rt, tileLabelPos, "Tile reading", { colours[1] });

            // Tile-reading order label (the value is drawn on the button itself).
            const auto tileOrderLabelPos = windowPos + ScreenCoordsXY{ 8, 87 };
            drawText(rt, tileOrderLabelPos, "Tile reading order", { colours[1] });

            // Focus-indicator colour label.
            const auto focusLabelPos = windowPos + ScreenCoordsXY{ 8, 107 };
            drawText(rt, focusLabelPos, "Focus colour", { colours[1] });

            // Menu-music volume label (the percentage is drawn on the button itself).
            const auto musicLabelPos = windowPos + ScreenCoordsXY{ 8, 127 };
            drawText(rt, musicLabelPos, "Menu music volume", { colours[1] });

            // Menu-sound volume label (the percentage is drawn on the button itself).
            const auto soundLabelPos = windowPos + ScreenCoordsXY{ 8, 147 };
            drawText(rt, soundLabelPos, "Menu sound volume", { colours[1] });
        }

        bool onAccessibilityAction(AccessibilityAction action) override
        {
            switch (action)
            {
                case AccessibilityAction::moveUp:
                    _supportArmed = false; // moving off the support button cancels the pending confirm
                    _accessIndex = std::max(0, _accessIndex - 1);
                    announceFocus();
                    return true;
                case AccessibilityAction::moveDown:
                    _supportArmed = false;
                    _accessIndex = std::min(7, _accessIndex + 1);
                    announceFocus();
                    return true;
                case AccessibilityAction::moveLeft:
                case AccessibilityAction::moveRight:
                {
                    _supportArmed = false;
                    const int32_t delta = (action == AccessibilityAction::moveRight) ? 1 : -1;
                    if (_accessIndex == 0)
                    {
                        const int32_t pct = std::clamp<int32_t>(
                            Config::Get().sound.accessibilityCueVolume + delta * kVolumeStep, 0, 100);
                        setVolume(static_cast<uint8_t>(pct));
                    }
                    else if (_accessIndex == 1)
                    {
                        cycleStepMode(delta);
                    }
                    else if (_accessIndex == 2)
                    {
                        cycleTileMode(delta);
                    }
                    else if (_accessIndex == 3)
                    {
                        cycleTileOrder(delta);
                    }
                    else if (_accessIndex == 4)
                    {
                        cycleFocusColour(delta);
                    }
                    else if (_accessIndex == 5)
                    {
                        adjustMenuMusicVolume(delta);
                    }
                    else if (_accessIndex == 6)
                    {
                        adjustMenuSoundVolume(delta);
                    }
                    announceValue();
                    return true;
                }
                case AccessibilityAction::activate:
                    if (_accessIndex == 1)
                    {
                        cycleStepMode(1); // Enter advances the step mode too
                        announceValue();
                    }
                    else if (_accessIndex == 2)
                    {
                        cycleTileMode(1);
                        announceValue();
                    }
                    else if (_accessIndex == 3)
                    {
                        cycleTileOrder(1); // Enter advances the reading order too
                        announceValue();
                    }
                    else if (_accessIndex == 4)
                    {
                        cycleFocusColour(1); // Enter advances the focus colour too
                        announceValue();
                    }
                    else if (_accessIndex == 7)
                    {
                        if (_supportArmed)
                        {
                            _supportArmed = false;
                            openSupportPage();
                        }
                        else
                        {
                            // First Enter: read the thank-you message and wait for a confirming Enter.
                            _supportArmed = true;
                            Accessibility::ScreenReaderSpeak(
                                "Thank you for the support! It allows me to keep paying for Claude Pro, run "
                                "the Accessibility Gaming Wiki server, and continue working in the gaming "
                                "industry. Press Enter to continue.");
                        }
                    }
                    else
                    {
                        announceFocus();
                    }
                    return true;
                case AccessibilityAction::cancel:
                    _supportArmed = false;
                    close();
                    return true;
                case AccessibilityAction::announce:
                    announceFocus();
                    return true;
                default:
                    return false;
            }
        }

        bool onAccessibilityTypeahead(uint32_t /*letterKey*/) override
        {
            // This is a settings dialog, not an item list; swallow letters so they don't leak to the
            // toolbar behind the window.
            return true;
        }

        std::optional<ScreenRect> getAccessibilityFocusRect() override
        {
            WidgetIndex w = WIDX_VOLUME_SLIDER;
            if (_accessIndex == 1)
                w = WIDX_STEP_MODE;
            else if (_accessIndex == 2)
                w = WIDX_TILE_MODE;
            else if (_accessIndex == 3)
                w = WIDX_TILE_ORDER;
            else if (_accessIndex == 4)
                w = WIDX_FOCUS_COLOUR;
            else if (_accessIndex == 5)
                w = WIDX_MENU_MUSIC;
            else if (_accessIndex == 6)
                w = WIDX_MENU_SOUND;
            else if (_accessIndex == 7)
                w = WIDX_SUPPORT_BUTTON;
            const auto& widget = widgets[w];
            const auto tl = windowPos + ScreenCoordsXY{ widget.left, widget.top };
            const auto br = windowPos + ScreenCoordsXY{ widget.right, widget.bottom };
            return ScreenRect{ tl, br };
        }

    private:
        void openSupportPage()
        {
            GetContext()->GetUiContext().OpenURL(kPatreonUrl);
            Accessibility::ScreenReaderSpeak("Opening the Patreon page in your browser.");
        }

        // The category label of the focused control (spoken when it is first focused).
        const char* labelText() const
        {
            switch (_accessIndex)
            {
                case 0:
                    return "Accessibility sounds volume";
                case 1:
                    return "Step sounds";
                case 2:
                    return "Tile reading";
                case 3:
                    return "Tile reading order";
                case 4:
                    return "Focus colour";
                case 5:
                    return "Menu music volume";
                case 6:
                    return "Menu sound volume";
                default:
                    return "Support Ross's work, button";
            }
        }

        // The current value of the focused control, with no label (spoken while adjusting the value).
        std::string valueText() const
        {
            switch (_accessIndex)
            {
                case 0:
                    return std::to_string(Config::Get().sound.accessibilityCueVolume) + " percent";
                case 1:
                    return stepModeName(Config::Get().sound.accessibilityStepSoundMode);
                case 2:
                    return tileModeName(Config::Get().sound.accessibilityTileSpeechMode);
                case 3:
                    return tileReadingOrderName(Config::Get().sound.accessibilityTileReadingOrder);
                case 4:
                    return focusColourName(Config::Get().sound.accessibilityFocusColour);
                case 5:
                    return std::to_string(Config::Get().sound.titleMusicVolume) + " percent";
                case 6:
                    return std::to_string(Config::Get().sound.titleSoundVolume) + " percent";
                default:
                    return {};
            }
        }

        std::string focusText()
        {
            const std::string value = valueText();
            return value.empty() ? std::string(labelText()) : std::string(labelText()) + ", " + value;
        }

        void cycleStepMode(int32_t delta)
        {
            int32_t m = Config::Get().sound.accessibilityStepSoundMode;
            m = (m + delta + kStepSoundModeCount) % kStepSoundModeCount;
            Config::Get().sound.accessibilityStepSoundMode = static_cast<uint8_t>(m);
            Config::Save();
            invalidate();
        }

        void cycleTileMode(int32_t delta)
        {
            int32_t m = Config::Get().sound.accessibilityTileSpeechMode;
            m = (m + delta + kTileSpeechModeCount) % kTileSpeechModeCount;
            Config::Get().sound.accessibilityTileSpeechMode = static_cast<uint8_t>(m);
            Config::Save();
            invalidate();
        }

        void cycleTileOrder(int32_t delta)
        {
            int32_t m = Config::Get().sound.accessibilityTileReadingOrder;
            m = (m + delta + kTileReadingOrderCount) % kTileReadingOrderCount;
            Config::Get().sound.accessibilityTileReadingOrder = static_cast<uint8_t>(m);
            Config::Save();
            invalidate();
        }

        void cycleFocusColour(int32_t delta)
        {
            int32_t i = focusColourIndex(Config::Get().sound.accessibilityFocusColour);
            i = (i + delta + kFocusColourCount) % kFocusColourCount;
            Config::Get().sound.accessibilityFocusColour = kFocusColours[i].value;
            Config::Save();
            invalidate();
        }

        void adjustMenuMusicVolume(int32_t delta)
        {
            const int32_t pct = std::clamp<int32_t>(Config::Get().sound.titleMusicVolume + delta * kVolumeStep, 0, 100);
            if (pct == Config::Get().sound.titleMusicVolume)
                return;
            Config::Get().sound.titleMusicVolume = static_cast<uint8_t>(pct);
            Config::Save();
            invalidate();
        }

        void adjustMenuSoundVolume(int32_t delta)
        {
            const int32_t pct = std::clamp<int32_t>(Config::Get().sound.titleSoundVolume + delta * kVolumeStep, 0, 100);
            if (pct == Config::Get().sound.titleSoundVolume)
                return;
            Config::Get().sound.titleSoundVolume = static_cast<uint8_t>(pct);
            Config::Save();
            // Read live by the mixer while on the title screen, so this takes effect immediately.
            invalidate();
        }

        void announceFocus()
        {
            Accessibility::ScreenReaderSpeak(focusText());
        }

        // Speaks only the value (no label) while the value is being changed, so the category is not
        // repeated on every step. Falls back to the full focus read for controls with no value.
        void announceValue()
        {
            const std::string value = valueText();
            if (value.empty())
                announceFocus();
            else
                Accessibility::ScreenReaderSpeak(value);
        }

        void setVolume(uint8_t pct)
        {
            if (pct == Config::Get().sound.accessibilityCueVolume)
                return;
            Config::Get().sound.accessibilityCueVolume = pct;
            Config::Save();
            initialiseScrollPosition(WIDX_VOLUME_SLIDER, 0, pct);
            invalidate();
        }

        static uint8_t snapVolume(int32_t pct)
        {
            pct = std::clamp(pct, 0, 100);
            return static_cast<uint8_t>(((pct + kVolumeStep / 2) / kVolumeStep) * kVolumeStep);
        }

        // Mirrors the audio-slider helpers in Options.cpp: read/write a scroll widget as a 0-100%
        // value, with a fixed content width of 500.
        ScreenSize onScrollGetSize(int32_t /*scrollIndex*/) override
        {
            return { 500, 0 };
        }

        uint8_t GetScrollPercentage(const Widget& widget, const ScrollArea& scroll)
        {
            const uint8_t w = widget.width() - 2;
            return static_cast<uint8_t>(static_cast<float>(scroll.contentOffsetX) / (scroll.contentWidth - w) * 100);
        }

        void initialiseScrollPosition(WidgetIndex widgetIndex, int32_t scrollId, uint8_t volume)
        {
            const auto& widget = widgets[widgetIndex];
            auto& scroll = scrolls[scrollId];

            const int32_t widgetSize = scroll.contentWidth - (widget.width() - 2);
            scroll.contentOffsetX = static_cast<int32_t>(std::ceil(volume / 100.0f * widgetSize));

            widgetScrollUpdateThumbs(*this, widgetIndex);
        }
    };

    WindowBase* AccessibilityOptionsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<AccessibilityOptionsWindow>(
            WindowClass::accessibilityOptions, kWindowSize, WindowFlag::centreScreen);
    }
} // namespace OpenRCT2::Ui::Windows
