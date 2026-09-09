/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessKitText.h"

#include <openrct2/Diagnostic.h>
#include <string>
#include <vector>

#ifdef _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <SDL.h>
    #include <SDL_syswm.h>
    #include <windows.h>

    #include "accesskit/accesskit.h"

namespace OpenRCT2::Ui::Accessibility
{
    namespace
    {
        // accesskit.dll is loaded with LoadLibrary and every entry point resolved by hand, for the
        // same reason prism.dll is (see prism/README.md): a link-time dependency on a DLL that is
        // missing stops openrct2.exe from starting at all, and the mod's own updater copies files
        // over existing installs where a stale folder is possible. A blind player is far better
        // served by a game that runs with plain text boxes than one that will not launch.
        using AkNodeNewFn = accesskit_node* (*)(accesskit_role);
        using AkNodeFreeFn = void (*)(accesskit_node*);
        using AkNodeSetLabelFn = void (*)(accesskit_node*, const char*);
        using AkNodeSetValueFn = void (*)(accesskit_node*, const char*);
        using AkNodeAddActionFn = void (*)(accesskit_node*, accesskit_action);
        using AkNodePushChildFn = void (*)(accesskit_node*, accesskit_node_id);
        using AkNodeSetCharacterLengthsFn = void (*)(accesskit_node*, size_t, const uint8_t*);
        using AkNodeSetTextSelectionFn = void (*)(accesskit_node*, accesskit_text_selection);
        using AkTreeInfoNewFn = accesskit_tree_info* (*)(accesskit_node_id);
        using AkTreeUpdateWithCapacityAndFocusFn = accesskit_tree_update* (*)(size_t, accesskit_node_id);
        using AkTreeUpdateSetTreeInfoFn = void (*)(accesskit_tree_update*, accesskit_tree_info*);
        using AkTreeUpdatePushNodeFn = void (*)(accesskit_tree_update*, accesskit_node_id, accesskit_node*);
        using AkActionRequestFreeFn = void (*)(accesskit_action_request*);
        using AkSubclassingAdapterNewFn = accesskit_windows_subclassing_adapter* (*)(
            HWND, accesskit_activation_handler_callback, void*, accesskit_action_handler_callback, void*);
        using AkSubclassingAdapterFreeFn = void (*)(accesskit_windows_subclassing_adapter*);
        using AkSubclassingAdapterUpdateIfActiveFn = accesskit_windows_queued_events* (*)(
            accesskit_windows_subclassing_adapter*, accesskit_tree_update_factory, void*);
        using AkQueuedEventsRaiseFn = void (*)(accesskit_windows_queued_events*);

        HMODULE _lib = nullptr;
        AkNodeNewFn _nodeNew = nullptr;
        AkNodeFreeFn _nodeFree = nullptr;
        AkNodeSetLabelFn _nodeSetLabel = nullptr;
        AkNodeSetValueFn _nodeSetValue = nullptr;
        AkNodeAddActionFn _nodeAddAction = nullptr;
        AkNodePushChildFn _nodePushChild = nullptr;
        AkNodeSetCharacterLengthsFn _nodeSetCharacterLengths = nullptr;
        AkNodeSetTextSelectionFn _nodeSetTextSelection = nullptr;
        AkTreeInfoNewFn _treeInfoNew = nullptr;
        AkTreeUpdateWithCapacityAndFocusFn _treeUpdateNew = nullptr;
        AkTreeUpdateSetTreeInfoFn _treeUpdateSetTreeInfo = nullptr;
        AkTreeUpdatePushNodeFn _treeUpdatePushNode = nullptr;
        AkActionRequestFreeFn _actionRequestFree = nullptr;
        AkSubclassingAdapterNewFn _adapterNew = nullptr;
        AkSubclassingAdapterFreeFn _adapterFree = nullptr;
        AkSubclassingAdapterUpdateIfActiveFn _adapterUpdateIfActive = nullptr;
        AkQueuedEventsRaiseFn _queuedEventsRaise = nullptr;

        accesskit_windows_subclassing_adapter* _adapter = nullptr;

        constexpr accesskit_node_id kWindowId = 0;
        constexpr accesskit_node_id kFieldId = 1;
        constexpr accesskit_node_id kRunId = 2;

        // The whole published state. Only ever touched on the thread that owns the window: the
        // activation handler is documented to run there, and update_if_active builds its tree
        // synchronously inside our own call. The action handler is the one callback that may arrive
        // on another thread, and it touches none of this.
        struct FieldState
        {
            bool active = false;
            std::string label;
            std::string text;
            // Byte length of each UTF-8 character in `text`. AccessKit indexes text by CHARACTER,
            // and this is how it maps those indices back onto the bytes.
            std::vector<uint8_t> characterLengths;
            size_t caretCharacter = 0;
        };
        FieldState _field;

        template<typename T>
        bool Resolve(const char* name, T& out)
        {
            out = reinterpret_cast<T>(GetProcAddress(_lib, name));
            return out != nullptr;
        }

        // Splits UTF-8 into per-character byte lengths, and converts a byte offset into the
        // character index that lands on. A malformed byte is treated as one character rather than
        // rejected, because a half-typed multi-byte sequence must not make the field unreadable.
        void MeasureText(std::string_view text, size_t caretBytes, std::vector<uint8_t>& lengths, size_t& caretCharacter)
        {
            lengths.clear();
            caretCharacter = 0;

            size_t i = 0;
            while (i < text.size())
            {
                const auto lead = static_cast<unsigned char>(text[i]);
                size_t len = 1;
                if ((lead & 0xE0) == 0xC0)
                    len = 2;
                else if ((lead & 0xF0) == 0xE0)
                    len = 3;
                else if ((lead & 0xF8) == 0xF0)
                    len = 4;
                len = std::min(len, text.size() - i);

                if (i < caretBytes)
                    caretCharacter = lengths.size() + 1;

                lengths.push_back(static_cast<uint8_t>(len));
                i += len;
            }

            caretCharacter = std::min(caretCharacter, lengths.size());
        }

        // Builds the whole tree from _field. Cheap enough to rebuild on every change: the tree is
        // three nodes, and a text field is only ever open while someone is typing into it.
        accesskit_tree_update* BuildTree(void*)
        {
            accesskit_node* window = _nodeNew(ACCESSKIT_ROLE_WINDOW);
            _nodeSetLabel(window, "OpenRCT2");

            const size_t nodeCount = _field.active ? 3 : 1;
            accesskit_tree_update* update = _treeUpdateNew(nodeCount, _field.active ? kFieldId : kWindowId);
            _treeUpdateSetTreeInfo(update, _treeInfoNew(kWindowId));

            if (_field.active)
            {
                _nodePushChild(window, kFieldId);

                // The text lives in a TEXT_RUN child rather than on the input itself. That is the
                // shape AccessKit expects, and it is what lets a reader address individual
                // characters - which is the whole point of doing this rather than announcing the
                // field's contents as a sentence.
                accesskit_node* run = _nodeNew(ACCESSKIT_ROLE_TEXT_RUN);
                _nodeSetValue(run, _field.text.c_str());
                _nodeSetCharacterLengths(run, _field.characterLengths.size(), _field.characterLengths.data());

                accesskit_node* field = _nodeNew(ACCESSKIT_ROLE_TEXT_INPUT);
                _nodeSetLabel(field, _field.label.c_str());
                _nodeAddAction(field, ACCESSKIT_ACTION_FOCUS);
                _nodePushChild(field, kRunId);

                // No selected range, just a caret: anchor and focus are the same position.
                const accesskit_text_position caret{ kRunId, _field.caretCharacter };
                _nodeSetTextSelection(field, accesskit_text_selection{ caret, caret });

                _treeUpdatePushNode(update, kRunId, run);
                _treeUpdatePushNode(update, kFieldId, field);
            }

            _treeUpdatePushNode(update, kWindowId, window);
            return update;
        }

        void HandleAction(accesskit_action_request* request, void*)
        {
            // Nothing is driven from the accessibility side yet - the keyboard already owns every
            // text field. The request still has to be freed, or the caller leaks it.
            _actionRequestFree(request);
        }

        void PushUpdate()
        {
            if (_adapter == nullptr)
                return;
            if (auto* events = _adapterUpdateIfActive(_adapter, BuildTree, nullptr); events != nullptr)
                _queuedEventsRaise(events);
        }
    } // namespace

    void AccessKitInit(void* sdlWindow)
    {
        if (_adapter != nullptr || sdlWindow == nullptr)
            return;

        _lib = LoadLibraryW(L"accesskit.dll");
        if (_lib == nullptr)
        {
            LOG_WARNING("Accessibility: accesskit.dll not found, text fields will not be exposed to screen readers");
            return;
        }

        if (!Resolve("accesskit_node_new", _nodeNew) || !Resolve("accesskit_node_free", _nodeFree)
            || !Resolve("accesskit_node_set_label", _nodeSetLabel) || !Resolve("accesskit_node_set_value", _nodeSetValue)
            || !Resolve("accesskit_node_add_action", _nodeAddAction)
            || !Resolve("accesskit_node_push_child", _nodePushChild)
            || !Resolve("accesskit_node_set_character_lengths", _nodeSetCharacterLengths)
            || !Resolve("accesskit_node_set_text_selection", _nodeSetTextSelection)
            || !Resolve("accesskit_tree_info_new", _treeInfoNew)
            || !Resolve("accesskit_tree_update_with_capacity_and_focus", _treeUpdateNew)
            || !Resolve("accesskit_tree_update_set_tree_info", _treeUpdateSetTreeInfo)
            || !Resolve("accesskit_tree_update_push_node", _treeUpdatePushNode)
            || !Resolve("accesskit_action_request_free", _actionRequestFree)
            || !Resolve("accesskit_windows_subclassing_adapter_new", _adapterNew)
            || !Resolve("accesskit_windows_subclassing_adapter_free", _adapterFree)
            || !Resolve("accesskit_windows_subclassing_adapter_update_if_active", _adapterUpdateIfActive)
            || !Resolve("accesskit_windows_queued_events_raise", _queuedEventsRaise))
        {
            LOG_WARNING("Accessibility: accesskit.dll is missing expected entry points, text fields not exposed");
            FreeLibrary(_lib);
            _lib = nullptr;
            return;
        }

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(static_cast<SDL_Window*>(sdlWindow), &wmInfo))
        {
            LOG_WARNING("Accessibility: could not get the native window handle, text fields not exposed");
            FreeLibrary(_lib);
            _lib = nullptr;
            return;
        }

        // Aborts if the window is already visible - see the header. UiContext::CreateWindow keeps
        // the window hidden until this has run.
        _adapter = _adapterNew(wmInfo.info.win.window, BuildTree, nullptr, HandleAction, nullptr);
        if (_adapter == nullptr)
        {
            LOG_WARNING("Accessibility: AccessKit adapter could not be created, text fields not exposed");
            FreeLibrary(_lib);
            _lib = nullptr;
            return;
        }

        LOG_INFO("Accessibility: AccessKit attached, text fields exposed to screen readers");
    }

    void AccessKitShutdown()
    {
        if (_adapter != nullptr)
        {
            _adapterFree(_adapter);
            _adapter = nullptr;
        }
        if (_lib != nullptr)
        {
            FreeLibrary(_lib);
            _lib = nullptr;
        }
    }

    void AccessKitTextFieldOpen(std::string_view label, std::string_view text, size_t caretBytes)
    {
        if (_adapter == nullptr)
            return;
        _field.active = true;
        _field.label.assign(label);
        _field.text.assign(text);
        MeasureText(_field.text, caretBytes, _field.characterLengths, _field.caretCharacter);
        PushUpdate();
    }

    void AccessKitTextFieldUpdate(std::string_view text, size_t caretBytes)
    {
        if (_adapter == nullptr || !_field.active)
            return;
        _field.text.assign(text);
        MeasureText(_field.text, caretBytes, _field.characterLengths, _field.caretCharacter);
        PushUpdate();
    }

    void AccessKitTextFieldClose()
    {
        if (_adapter == nullptr || !_field.active)
            return;
        _field = {};
        PushUpdate();
    }
} // namespace OpenRCT2::Ui::Accessibility

#else

namespace OpenRCT2::Ui::Accessibility
{
    void AccessKitInit(void*)
    {
    }

    void AccessKitShutdown()
    {
    }

    void AccessKitTextFieldOpen(std::string_view, std::string_view, size_t)
    {
    }

    void AccessKitTextFieldUpdate(std::string_view, size_t)
    {
    }

    void AccessKitTextFieldClose()
    {
    }
} // namespace OpenRCT2::Ui::Accessibility

#endif
