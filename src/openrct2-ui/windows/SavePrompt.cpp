/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <iterator>
#include <string>
#include <vector>
#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/Diagnostic.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/network/Network.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/windows/Intent.h>

namespace OpenRCT2::Ui::Windows
{
    static constexpr ScreenSize kWindowSizeSave = { 260, 54 };
    static constexpr ScreenSize kWindowSizeQuit = { 177, 38 };

    enum WindowSavePromptWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LABEL,
        WIDX_SAVE,
        WIDX_DONT_SAVE,
        WIDX_CANCEL
    };

    // clang-format off
    static constexpr auto _savePromptWidgets = makeWidgets(
        makeWindowShim(kStringIdNone, kWindowSizeSave),
        makeWidget({  2, 19}, {256, 12}, WidgetType::labelCentred, WindowColour::primary, kStringIdEmpty           ), // question/label
        makeWidget({  8, 35}, { 78, 14}, WidgetType::button,       WindowColour::primary, STR_SAVE_PROMPT_SAVE     ), // save
        makeWidget({ 91, 35}, { 78, 14}, WidgetType::button,       WindowColour::primary, STR_SAVE_PROMPT_DONT_SAVE), // don't save
        makeWidget({174, 35}, { 78, 14}, WidgetType::button,       WindowColour::primary, STR_SAVE_PROMPT_CANCEL   ) // cancel
    );
    // clang-format on

    enum WindowQuitPromptWidgetIdx
    {
        WQIDX_BACKGROUND,
        WQIDX_TITLE,
        WQIDX_CLOSE,
        WQIDX_OK,
        WQIDX_CANCEL
    };

    // clang-format off
    static constexpr auto _quitPromptWidgets = makeWidgets(
        makeWindowShim(STR_QUIT_GAME_PROMPT_TITLE, kWindowSizeQuit),
        makeWidget({ 8, 19}, {78, 14}, WidgetType::button, WindowColour::primary, STR_OK    ), // ok
        makeWidget({91, 19}, {78, 14}, WidgetType::button, WindowColour::primary, STR_CANCEL)  // cancel
    );
    // clang-format on

    static constexpr StringId window_save_prompt_labels[][2] = {
        { STR_LOAD_GAME_PROMPT_TITLE, STR_SAVE_BEFORE_LOADING },
        { STR_QUIT_GAME_PROMPT_TITLE, STR_SAVE_BEFORE_QUITTING },
        { STR_QUIT_GAME_2_PROMPT_TITLE, STR_SAVE_BEFORE_QUITTING_2 },
        { STR_NEW_GAME, STR_SAVE_BEFORE_QUITTING },
    };

    static void WindowSavePromptCallback(ModalResult result, const utf8* path)
    {
        if (result == ModalResult::ok)
        {
            GameLoadOrQuitNoSavePrompt();
        }
    }

    class SavePromptWindow final : public Window
    {
    private:
        PromptMode _promptMode;
        bool _canSave = true;            // save layout (Save/Don't save/Cancel) vs quit (OK/Cancel)
        int32_t _accessIndex = 0;        // keyboard focus over the prompt buttons

    public:
        SavePromptWindow(PromptMode promptMode)
            : _promptMode(promptMode)
        {
        }

        void onOpen() override
        {
            bool canSave = !(isInTrackDesignerOrManager());
            _canSave = canSave;
            _accessIndex = 0;

            if (canSave)
                setWidgets(_savePromptWidgets);
            else
                setWidgets(_quitPromptWidgets);

            initScrollWidgets();

            // Pause the game if not network play.
            if (Network::GetMode() == Network::Mode::none)
            {
                gGamePaused |= GAME_PAUSED_MODAL;
                Audio::StopAll();
            }

            auto* windowMgr = GetWindowManager();
            windowMgr->InvalidateByClass(WindowClass::topToolbar);

            if (canSave)
            {
                StringId stringId = window_save_prompt_labels[EnumValue(_promptMode)][0];
                if (stringId == STR_LOAD_GAME_PROMPT_TITLE && gLegacyScene == LegacyScene::scenarioEditor)
                {
                    stringId = STR_LOAD_LANDSCAPE_PROMPT_TITLE;
                }
                else if (stringId == STR_QUIT_GAME_PROMPT_TITLE && gLegacyScene == LegacyScene::scenarioEditor)
                {
                    stringId = STR_QUIT_SCENARIO_EDITOR;
                }
                widgets[WIDX_TITLE].text = stringId;
                widgets[WIDX_LABEL].text = window_save_prompt_labels[EnumValue(_promptMode)][1];
            }

            announceAccessibilityPrompt();
        }

        std::vector<WidgetIndex> getPromptButtons() const
        {
            if (_canSave)
                return { WIDX_SAVE, WIDX_DONT_SAVE, WIDX_CANCEL };
            return { WQIDX_OK, WQIDX_CANCEL };
        }

        void announceAccessibilityPrompt()
        {
            std::string text = OpenRCT2::FormatStringID(widgets[WIDX_TITLE].text);
            if (_canSave)
                text += ". " + OpenRCT2::FormatStringID(widgets[WIDX_LABEL].text);
            text += ". Options: ";
            const auto buttons = getPromptButtons();
            for (size_t i = 0; i < buttons.size(); i++)
            {
                if (i != 0)
                    text += ", ";
                text += OpenRCT2::FormatStringID(widgets[buttons[i]].text);
            }
            Accessibility::ScreenReaderSpeak(text);
        }

        bool onAccessibilityAction(AccessibilityAction action) override
        {
            const auto buttons = getPromptButtons();
            const int32_t count = static_cast<int32_t>(buttons.size());

            switch (action)
            {
                case AccessibilityAction::moveUp:
                case AccessibilityAction::moveLeft:
                case AccessibilityAction::moveDown:
                case AccessibilityAction::moveRight:
                {
                    const bool forward = (action == AccessibilityAction::moveDown
                                          || action == AccessibilityAction::moveRight);
                    _accessIndex = forward ? (_accessIndex + 1) % count : (_accessIndex - 1 + count) % count;
                    Accessibility::ScreenReaderSpeakItem(
                        OpenRCT2::FormatStringID(widgets[buttons[_accessIndex]].text), _accessIndex, count);
                    return true;
                }

                case AccessibilityAction::activate:
                    // Cancel is always the last button; treat Enter on it like Escape so focus
                    // returns to the menu cleanly.
                    if (_accessIndex == count - 1)
                    {
                        close();
                        Accessibility::ReannounceToolbarItemIfMenuMode();
                    }
                    else
                    {
                        onMouseUp(buttons[_accessIndex]); // Save / Don't save / OK
                    }
                    return true;

                case AccessibilityAction::cancel:
                    close();
                    Accessibility::ReannounceToolbarItemIfMenuMode();
                    return true;

                default:
                    return false;
            }
        }

        void onClose() override
        {
            // Unpause the game
            if (Network::GetMode() == Network::Mode::none)
            {
                gGamePaused &= ~GAME_PAUSED_MODAL;
                Audio::Resume();
            }

            auto* windowMgr = GetWindowManager();
            windowMgr->InvalidateByClass(WindowClass::topToolbar);
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            if (gLegacyScene == LegacyScene::titleSequence || gLegacyScene == LegacyScene::trackDesigner
                || gLegacyScene == LegacyScene::trackDesignsManager)
            {
                switch (widgetIndex)
                {
                    case WQIDX_OK:
                        GameLoadOrQuitNoSavePrompt();
                        break;
                    case WQIDX_CLOSE:
                    case WQIDX_CANCEL:
                        close();
                        break;
                }
                return;
            }

            switch (widgetIndex)
            {
                case WIDX_SAVE:
                {
                    std::unique_ptr<Intent> intent;

                    if (isInEditorMode())
                    {
                        intent = std::make_unique<Intent>(WindowClass::loadsave);
                        intent->PutEnumExtra<LoadSaveAction>(INTENT_EXTRA_LOADSAVE_ACTION, LoadSaveAction::save);
                        intent->PutEnumExtra<LoadSaveType>(INTENT_EXTRA_LOADSAVE_TYPE, LoadSaveType::landscape);
                        intent->PutExtra(INTENT_EXTRA_PATH, getGameState().scenarioOptions.name);
                    }
                    else
                    {
                        intent = CreateSaveGameAsIntent();
                    }
                    close();
                    intent->PutExtra(INTENT_EXTRA_CALLBACK, reinterpret_cast<CloseCallback>(WindowSavePromptCallback));
                    ContextOpenIntent(intent.get());
                    break;
                }
                case WIDX_DONT_SAVE:
                    GameLoadOrQuitNoSavePrompt();
                    return;
                case WIDX_CLOSE:
                case WIDX_CANCEL:
                    close();
                    return;
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
        }
    };

    WindowBase* SavePromptOpen()
    {
        PromptMode prompt_mode = gSavePromptMode;
        if (prompt_mode == PromptMode::quit)
        {
            prompt_mode = PromptMode::saveBeforeQuit;
        }

        // do not show save prompt if we're in the title demo and click on load game
        if (gLegacyScene == LegacyScene::titleSequence)
        {
            GameLoadOrQuitNoSavePrompt();
            return nullptr;
        }

        if (!Config::Get().general.confirmationPrompt)
        {
            /* game_load_or_quit_no_save_prompt() will exec requested task and close this window
             * immediately again.
             * TODO restructure these functions when we're sure game_load_or_quit_no_save_prompt()
             * and game_load_or_quit() are not called by the original binary anymore.
             */

            if (gScreenAge < 3840 && Network::GetMode() == Network::Mode::none)
            {
                GameLoadOrQuitNoSavePrompt();
                return nullptr;
            }
        }

        auto* windowMgr = GetWindowManager();

        // Check if window is already open
        auto* window = windowMgr->BringToFrontByClass(WindowClass::savePrompt);
        if (window != nullptr)
        {
            windowMgr->Close(*window);
        }

        if (EnumValue(prompt_mode) >= std::size(window_save_prompt_labels))
        {
            LOG_WARNING("Invalid save prompt mode %u", prompt_mode);
            return nullptr;
        }

        auto windowSize = kWindowSizeSave;
        if (isInTrackDesignerOrManager())
        {
            windowSize = kWindowSizeQuit;
        }

        auto savePromptWindow = std::make_unique<SavePromptWindow>(prompt_mode);
        return windowMgr->Create(
            std::move(savePromptWindow), WindowClass::savePrompt, {}, windowSize,
            { WindowFlag::transparent, WindowFlag::stickToFront, WindowFlag::centreScreen, WindowFlag::autoPosition });
    }
} // namespace OpenRCT2::Ui::Windows
