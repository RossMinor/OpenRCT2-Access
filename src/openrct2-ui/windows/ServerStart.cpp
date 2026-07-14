/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_NETWORK

    #include "../interface/Theme.h"

    #include <openrct2-ui/accessibility/ListNavigation.h>
    #include <openrct2-ui/accessibility/ScreenReader.h>
    #include <openrct2-ui/interface/Widget.h>
    #include <openrct2-ui/windows/Windows.h>
    #include <openrct2/Context.h>
    #include <openrct2/Game.h>
    #include <openrct2/ParkImporter.h>
    #include <openrct2/config/Config.h>
    #include <openrct2/core/String.hpp>
    #include <openrct2/drawing/Drawing.h>
    #include <openrct2/drawing/Text.h>
    #include <openrct2/interface/Chat.h>
    #include <openrct2/network/Network.h>
    #include <openrct2/ui/WindowManager.h>
    #include <openrct2/windows/Intent.h>
    #include <optional>
    #include <string>

namespace OpenRCT2::Ui::Windows
{
    enum WindowServerStartWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PORT_INPUT,
        WIDX_NAME_INPUT,
        WIDX_DESCRIPTION_INPUT,
        WIDX_GREETING_INPUT,
        WIDX_PASSWORD_INPUT,
        WIDX_MAXPLAYERS,
        WIDX_MAXPLAYERS_INCREASE,
        WIDX_MAXPLAYERS_DECREASE,
        WIDX_ADVERTISE_CHECKBOX,
        WIDX_START_SERVER,
        WIDX_LOAD_SERVER
    };

    static constexpr ScreenSize kWindowSize = { 300, 154 };

    // clang-format off
    static constexpr auto _windowServerStartWidgets = makeWidgets(
        makeWindowShim(STR_START_SERVER, kWindowSize),
        makeWidget({ 120, 20 }, { 173, 13 }, WidgetType::textBox, WindowColour::secondary), // port text box
        makeWidget({ 120, 36 }, { 173, 13 }, WidgetType::textBox, WindowColour::secondary), // name text box
        makeWidget({ 120, 52 }, { 173, 13 }, WidgetType::textBox, WindowColour::secondary), // description text box
        makeWidget({ 120, 68 }, { 173, 13 }, WidgetType::textBox, WindowColour::secondary), // greeting text box
        makeWidget({ 120, 84 }, { 173, 13 }, WidgetType::textBox, WindowColour::secondary), // password text box
        makeSpinnerWidgets({ 120, 100 }, { 173, 12 }, WidgetType::spinner, WindowColour::secondary,kStringIdEmpty), // max players (3 widgets)
        makeWidget({ 6, 117 }, { 287, 14 }, WidgetType::checkbox, WindowColour::secondary, STR_ADVERTISE,STR_ADVERTISE_SERVER_TIP), // advertise checkbox
        makeWidget({ 6, kWindowSize.height - 6 - 13 }, { 101, 14 }, WidgetType::button, WindowColour::secondary,STR_NEW_GAME), // start server button
        makeWidget({ 112, kWindowSize.height - 6 - 13 }, { 101, 14 }, WidgetType::button, WindowColour::secondary, STR_LOAD_GAME) // None
    );
    // clang-format on

    class ServerStartWindow final : public Window
    {
        u8string _maxPlayersCaption{};

    public:
        void onOpen() override
        {
            setWidgets(_windowServerStartWidgets);
            widgets[WIDX_PORT_INPUT].string = _port;
            widgets[WIDX_NAME_INPUT].string = _name;
            widgets[WIDX_DESCRIPTION_INPUT].string = _description;
            widgets[WIDX_GREETING_INPUT].string = _greeting;
            widgets[WIDX_PASSWORD_INPUT].string = _password;

            initScrollWidgets();
            WindowSetResize(*this, { width, height }, { width, height });

            currentFrame = 0;
            page = 0;
            listInformationType = 0;

            snprintf(_port, 7, "%u", Config::Get().network.defaultPort);
            String::safeUtf8Copy(_name, Config::Get().network.serverName.c_str(), sizeof(_name));
            String::safeUtf8Copy(_description, Config::Get().network.serverDescription.c_str(), sizeof(_description));
            String::safeUtf8Copy(_greeting, Config::Get().network.serverGreeting.c_str(), sizeof(_greeting));
        }
        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_PORT_INPUT:
                    WindowStartTextbox(*this, widgetIndex, _port, 6);
                    break;
                case WIDX_NAME_INPUT:
                    WindowStartTextbox(*this, widgetIndex, _name, 64);
                    break;
                case WIDX_DESCRIPTION_INPUT:
                    WindowStartTextbox(*this, widgetIndex, _description, Network::kMaxServerDescriptionLength);
                    break;
                case WIDX_GREETING_INPUT:
                    WindowStartTextbox(*this, widgetIndex, _greeting, kChatInputSize);
                    break;
                case WIDX_PASSWORD_INPUT:
                    WindowStartTextbox(*this, widgetIndex, _password, 32);
                    break;
                case WIDX_MAXPLAYERS_INCREASE:
                    if (Config::Get().network.maxplayers < 255)
                    {
                        Config::Get().network.maxplayers++;
                    }
                    Config::Save();
                    invalidate();
                    break;
                case WIDX_MAXPLAYERS_DECREASE:
                    if (Config::Get().network.maxplayers > 1)
                    {
                        Config::Get().network.maxplayers--;
                    }
                    Config::Save();
                    invalidate();
                    break;
                case WIDX_ADVERTISE_CHECKBOX:
                    Config::Get().network.advertise = !Config::Get().network.advertise;
                    Config::Save();
                    invalidate();
                    break;
                case WIDX_START_SERVER:
                    Network::SetPassword(_password);
                    ScenarioselectOpen(ScenarioSelectCallback);
                    break;
                case WIDX_LOAD_SERVER:
                    Network::SetPassword(_password);
                    auto intent = Intent(WindowClass::loadsave);
                    intent.PutEnumExtra<LoadSaveAction>(INTENT_EXTRA_LOADSAVE_ACTION, LoadSaveAction::load);
                    intent.PutEnumExtra<LoadSaveType>(INTENT_EXTRA_LOADSAVE_TYPE, LoadSaveType::park);
                    intent.PutExtra(INTENT_EXTRA_CALLBACK, reinterpret_cast<CloseCallback>(LoadSaveCallback));
                    ContextOpenIntent(&intent);
                    break;
            }
        }
        void onPrepareDraw() override
        {
            ColourSchemeUpdateByClass(this, WindowClass::serverList);

            setCheckboxValue(WIDX_ADVERTISE_CHECKBOX, Config::Get().network.advertise);

            _maxPlayersCaption = std::to_string(Config::Get().network.maxplayers);
            widgets[WIDX_MAXPLAYERS].setString(_maxPlayersCaption.c_str());
        }
        void onUpdate() override
        {
            if (GetCurrentTextBox().window.classification == classification && GetCurrentTextBox().window.number == number)
            {
                WindowUpdateTextboxCaret();
                invalidateWidget(WIDX_NAME_INPUT);
                invalidateWidget(WIDX_DESCRIPTION_INPUT);
                invalidateWidget(WIDX_GREETING_INPUT);
                invalidateWidget(WIDX_PASSWORD_INPUT);
            }
        }
        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            std::string temp = static_cast<std::string>(text);
            int tempPort = 0;

            switch (widgetIndex)
            {
                case WIDX_PORT_INPUT:
                    if (strcmp(_port, temp.c_str()) == 0)
                        return;

                    String::safeUtf8Copy(_port, temp.c_str(), sizeof(_port));

                    // Don't allow negative/zero for port number
                    tempPort = atoi(_port);
                    if (tempPort > 0)
                    {
                        Config::Get().network.defaultPort = tempPort;
                        Config::Save();
                    }

                    invalidateWidget(WIDX_PORT_INPUT);
                    break;
                case WIDX_NAME_INPUT:
                    if (strcmp(_name, temp.c_str()) == 0)
                        return;

                    String::safeUtf8Copy(_name, temp.c_str(), sizeof(_name));

                    // Don't allow empty server names
                    if (_name[0] != '\0')
                    {
                        Config::Get().network.serverName = _name;
                        Config::Save();
                    }

                    invalidateWidget(WIDX_NAME_INPUT);
                    break;
                case WIDX_DESCRIPTION_INPUT:
                    if (strcmp(_description, temp.c_str()) == 0)
                        return;

                    String::safeUtf8Copy(_description, temp.c_str(), sizeof(_description));
                    Config::Get().network.serverDescription = _description;
                    Config::Save();

                    invalidateWidget(WIDX_DESCRIPTION_INPUT);
                    break;
                case WIDX_GREETING_INPUT:
                    if (strcmp(_greeting, temp.c_str()) == 0)
                        return;

                    String::safeUtf8Copy(_greeting, temp.c_str(), sizeof(_greeting));
                    Config::Get().network.serverGreeting = _greeting;
                    Config::Save();

                    invalidateWidget(WIDX_GREETING_INPUT);
                    break;
                case WIDX_PASSWORD_INPUT:
                    if (strcmp(_password, temp.c_str()) == 0)
                        return;

                    String::safeUtf8Copy(_password, temp.c_str(), sizeof(_password));

                    invalidateWidget(WIDX_PASSWORD_INPUT);
                    break;
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_PORT_INPUT].top }, STR_PORT, { colours[1] });
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_NAME_INPUT].top }, STR_SERVER_NAME, { colours[1] });
            drawText(
                rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_DESCRIPTION_INPUT].top }, STR_SERVER_DESCRIPTION,
                { colours[1] });
            drawText(
                rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_GREETING_INPUT].top }, STR_SERVER_GREETING, { colours[1] });
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_PASSWORD_INPUT].top }, STR_PASSWORD, { colours[1] });
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_MAXPLAYERS].top }, STR_MAX_PLAYERS, { colours[1] });
        }

        // Accessible keyboard navigation. The window is a flat form, so we present its controls as a
        // single up/down list of fields, each spoken as "label, value, position of total". Left/right
        // adjusts the value where that makes sense (max-players spinner, advertise checkbox); Enter
        // edits a text field or presses a button.
        bool onAccessibilityAction(AccessibilityAction action) override
        {
            switch (action)
            {
                case AccessibilityAction::moveDown:
                case AccessibilityAction::moveUp:
                {
                    const int32_t delta = (action == AccessibilityAction::moveDown) ? 1 : -1;
                    _accessField = Accessibility::ListNav::wrap(_accessField, delta, kAxFieldCount);
                    announceField();
                    invalidate();
                    return true;
                }
                case AccessibilityAction::moveLeft:
                case AccessibilityAction::moveRight:
                    adjustField(action == AccessibilityAction::moveRight);
                    return true;
                case AccessibilityAction::activate:
                    activateField();
                    return true;
                case AccessibilityAction::cancel:
                    close();
                    return true;
                case AccessibilityAction::announce:
                    announceField();
                    return true;
                default:
                    return false;
            }
        }

        std::optional<ScreenRect> getAccessibilityFocusRect() override
        {
            if (_accessField < 0 || _accessField >= kAxFieldCount)
                return std::nullopt;
            const auto& wdg = widgets[widgetForField(static_cast<AxField>(_accessField))];
            return ScreenRect{ { windowPos.x + wdg.left, windowPos.y + wdg.top },
                               { windowPos.x + wdg.right, windowPos.y + wdg.bottom } };
        }

    private:
        // Fields in top-to-bottom reading order, matching the on-screen layout.
        enum class AxField : int32_t
        {
            port,
            name,
            description,
            greeting,
            password,
            maxPlayers,
            advertise,
            startServer,
            loadServer,
        };
        static constexpr int32_t kAxFieldCount = 9;
        int32_t _accessField = -1;

        static WidgetIndex widgetForField(AxField f)
        {
            switch (f)
            {
                case AxField::port:        return WIDX_PORT_INPUT;
                case AxField::name:        return WIDX_NAME_INPUT;
                case AxField::description: return WIDX_DESCRIPTION_INPUT;
                case AxField::greeting:    return WIDX_GREETING_INPUT;
                case AxField::password:    return WIDX_PASSWORD_INPUT;
                case AxField::maxPlayers:  return WIDX_MAXPLAYERS;
                case AxField::advertise:   return WIDX_ADVERTISE_CHECKBOX;
                case AxField::startServer: return WIDX_START_SERVER;
                case AxField::loadServer:  return WIDX_LOAD_SERVER;
            }
            return WIDX_PORT_INPUT;
        }

        static std::string_view labelText(AxField f)
        {
            switch (f)
            {
                case AxField::port:        return "Port";
                case AxField::name:        return "Server name";
                case AxField::description: return "Description";
                case AxField::greeting:    return "Greeting";
                case AxField::password:    return "Password";
                case AxField::maxPlayers:  return "Maximum players";
                case AxField::advertise:   return "Advertise server publicly";
                case AxField::startServer: return "Start server";
                case AxField::loadServer:  return "Load a saved game and start server";
            }
            return {};
        }

        // The value only, no label - spoken while adjusting a field (matching the slider convention in
        // the accessibility options window, where left/right speaks just the new value).
        std::string valueText(AxField f) const
        {
            switch (f)
            {
                case AxField::port:        return _port[0] ? _port : "not set";
                case AxField::name:        return _name[0] ? _name : "not set";
                case AxField::description: return _description[0] ? _description : "not set";
                case AxField::greeting:    return _greeting[0] ? _greeting : "not set";
                // Never speak the password itself; just whether one is set.
                case AxField::password:    return _password[0] ? "set" : "not set";
                case AxField::maxPlayers:  return std::to_string(Config::Get().network.maxplayers);
                case AxField::advertise:   return Config::Get().network.advertise ? "on" : "off";
                case AxField::startServer:
                case AxField::loadServer:  return {};
            }
            return {};
        }

        // Label plus value - spoken when moving onto a field.
        std::string fieldText(AxField f) const
        {
            if (f == AxField::startServer || f == AxField::loadServer)
                return std::string(labelText(f)) + ", button";
            return Accessibility::JoinSpeech({ labelText(f), valueText(f) });
        }

        void announceField()
        {
            if (_accessField < 0 || _accessField >= kAxFieldCount)
                return;
            Accessibility::ScreenReaderSpeakItem(
                fieldText(static_cast<AxField>(_accessField)), _accessField, kAxFieldCount);
        }

        // Left/right on the two adjustable fields; a no-op elsewhere.
        void adjustField(bool increase)
        {
            if (_accessField < 0 || _accessField >= kAxFieldCount)
                return;
            const auto f = static_cast<AxField>(_accessField);
            switch (f)
            {
                case AxField::maxPlayers:
                    onMouseUp(increase ? WIDX_MAXPLAYERS_INCREASE : WIDX_MAXPLAYERS_DECREASE);
                    Accessibility::ScreenReaderSpeak(valueText(f));
                    break;
                case AxField::advertise:
                    onMouseUp(WIDX_ADVERTISE_CHECKBOX);
                    Accessibility::ScreenReaderSpeak(valueText(f));
                    break;
                default:
                    break;
            }
        }

        // Opens the shared modal text-input window (the same one banners, signs and the group-rename
        // use). This is the accessible path: the modal is screen-reader aware and, crucially, the
        // InputManager routes keys to it directly, so the confirming Enter is consumed by the modal
        // and cannot leak back to re-activate the field - unlike the inline widget text box, which
        // the mouse path uses and which would trap keyboard focus.
        void openTextEditor(AxField f)
        {
            switch (f)
            {
                case AxField::port:
                    WindowTextInputRawOpen(this, WIDX_PORT_INPUT, STR_PORT, kStringIdEmpty, {}, _port, 6);
                    break;
                case AxField::name:
                    WindowTextInputRawOpen(this, WIDX_NAME_INPUT, STR_SERVER_NAME, kStringIdEmpty, {}, _name, 64);
                    break;
                case AxField::description:
                    WindowTextInputRawOpen(
                        this, WIDX_DESCRIPTION_INPUT, STR_SERVER_DESCRIPTION, kStringIdEmpty, {}, _description,
                        Network::kMaxServerDescriptionLength);
                    break;
                case AxField::greeting:
                    WindowTextInputRawOpen(
                        this, WIDX_GREETING_INPUT, STR_SERVER_GREETING, kStringIdEmpty, {}, _greeting, kChatInputSize);
                    break;
                case AxField::password:
                    WindowTextInputRawOpen(this, WIDX_PASSWORD_INPUT, STR_PASSWORD, kStringIdEmpty, {}, _password, 32);
                    break;
                default:
                    break;
            }
        }

        void activateField()
        {
            if (_accessField < 0 || _accessField >= kAxFieldCount)
                return;
            const auto f = static_cast<AxField>(_accessField);
            switch (f)
            {
                case AxField::port:
                case AxField::name:
                case AxField::description:
                case AxField::greeting:
                case AxField::password:
                    openTextEditor(f);
                    break;
                case AxField::advertise:
                    onMouseUp(WIDX_ADVERTISE_CHECKBOX);
                    Accessibility::ScreenReaderSpeak(valueText(f));
                    break;
                case AxField::maxPlayers:
                    announceField();
                    break;
                case AxField::startServer:
                case AxField::loadServer:
                    onMouseUp(widgetForField(f));
                    break;
            }
        }

        char _port[7];
        char _name[65];
        char _description[Network::kMaxServerDescriptionLength];
        char _greeting[kChatInputSize];
        char _password[33];
        static void ScenarioSelectCallback(const utf8* path)
        {
            GameNotifyMapChange();
            if (GetContext()->LoadParkFromFile(path, false, true))
            {
                Network::BeginServer(Config::Get().network.defaultPort, Config::Get().network.listenAddress);
            }
        }

        static void LoadSaveCallback(ModalResult result, const utf8* path)
        {
            if (result == ModalResult::ok)
            {
                GameNotifyMapChange();
                GetContext()->LoadParkFromFile(path);
                Network::BeginServer(Config::Get().network.defaultPort, Config::Get().network.listenAddress);
            }
        }
    };

    WindowBase* ServerStartOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<ServerStartWindow>(WindowClass::serverStart, kWindowSize, WindowFlag::centreScreen);
    }
} // namespace OpenRCT2::Ui::Windows

#endif
