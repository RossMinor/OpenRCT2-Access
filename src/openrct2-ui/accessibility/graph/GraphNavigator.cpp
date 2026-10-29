/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GraphNavigator.h"

#include "../MapNavigation.h"
#include "../ScreenReader.h"
#include "GraphAnnouncer.h"
#include "GraphScreens.h"
#include "GraphVocab.h"
#include "KeyGraph.h"

#include <SDL.h>
#include <cctype>
#include <openrct2-ui/UiContext.h>
#include <openrct2-ui/input/ShortcutManager.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Diagnostic.h>
#include <openrct2/config/Config.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/ui/WindowManager.h>
#include <unordered_set>
#include <utility>

namespace OpenRCT2::Ui::Accessibility
{
    using namespace Graph;

    // ---- navigator state (the differ memory, the live-watch baseline, the attachment) ----

    static WindowClass _attachedClass = WindowClass::null;

    // The frame differ's memory: the identity last spoken. Comparison is by ControlId equality
    // (structural key only) - a node whose backing object was swapped under an unchanged key is
    // NOT re-announced (live parts cover content changes); a tier-1-recovered node (same object,
    // new key - it moved) IS re-announced.
    static ControlId _lastSpoken;
    static bool _lastSpokenValid = false;

    // The key we consumed on key-down, so the matching key-up is swallowed too.
    static uint32_t _lastHandledKey = 0;

    // Live watch: the focused node's live parts' last-resolved texts.
    static ControlId _liveNodeId;
    static std::vector<std::string> _liveBaseline;
    static bool _liveValid = false;

    // The focused node's screen rect, for the sighted-user focus box.
    static std::optional<GraphRect> _focusRect;

    // Fault isolation: log a recipe failure once per attachment, not once per frame.
    static std::unordered_set<uint8_t> _loggedThisAttach;

    static void EnsureAnnouncerHooks()
    {
        static bool installed = false;
        if (installed)
            return;
        installed = true;
        GraphAnnouncer::PositionText = [](int32_t index, int32_t count) { return Vocab::Position(index, count); };
        GraphAnnouncer::ExpandedStateText = [](bool expanded) { return Vocab::ExpandedState(expanded); };
        // No PartFilter yet: everything speaks (per-kind verbosity settings are a later port).
    }

    static void LogRecipeFailure(WindowClass cls, const char* what)
    {
        if (_loggedThisAttach.insert(static_cast<uint8_t>(cls)).second)
            LOG_ERROR("Accessibility graph recipe failed for window class %d: %s", static_cast<int>(cls), what);
    }

    // Build the attached screen's render, exception-isolated: a throwing recipe logs once and
    // renders nothing this frame - focus state survives, and the next good render reconciles back.
    static std::unique_ptr<GraphRender> BuildForClass(WindowClass cls)
    {
        const auto* screen = GraphScreenForClass(cls);
        auto* windowMgr = GetWindowManager();
        auto* w = windowMgr != nullptr ? windowMgr->FindByClass(cls) : nullptr;
        if (screen == nullptr || w == nullptr || !screen->build)
            return nullptr;
        try
        {
            GraphBuilder builder(&GraphStateForClass(cls).expanded);
            screen->build(builder, *w);
            return builder.Build();
        }
        catch (const std::exception& ex)
        {
            LogRecipeFailure(cls, ex.what());
            return nullptr;
        }
        catch (...)
        {
            LogRecipeFailure(cls, "unknown exception");
            return nullptr;
        }
    }

    static KeyGraph MakeGraph(WindowClass cls)
    {
        return KeyGraph([cls]() { return BuildForClass(cls); }, &GraphStateForClass(cls));
    }

    static std::string GuardedText(const std::function<std::string()>& fn)
    {
        try
        {
            return fn ? fn() : std::string();
        }
        catch (...)
        {
            return {};
        }
    }

    // Called on every focus landing, so it is also where a node's out-of-graph focus side effect
    // runs (NodeVtable::onFocus).
    static void UpdateFocusRect(const GraphNode* node)
    {
        _focusRect.reset();
        if (node == nullptr)
            return;
        if (node->vtable.focusRect)
        {
            try
            {
                _focusRect = node->vtable.focusRect();
            }
            catch (...)
            {
            }
        }
        if (node->vtable.onFocus)
        {
            try
            {
                node->vtable.onFocus();
            }
            catch (...)
            {
            }
        }
    }

    std::optional<GraphRect> GraphFocusScreenRect()
    {
        return _attachedClass != WindowClass::null ? _focusRect : std::nullopt;
    }

    // ---- live watch (§7.6) ----

    static std::vector<std::string> ResolveLiveParts(const GraphNode* node)
    {
        std::vector<std::string> texts;
        for (const auto& a : GraphAnnouncer::EffectiveAnnouncements(node))
        {
            if (a.live)
                texts.push_back(GuardedText(a.text));
        }
        return texts;
    }

    // Rebaseline silently: a focus landing already spoke the initial state.
    static void ResetLiveBaseline(const GraphNode* node)
    {
        _liveValid = node != nullptr;
        if (node == nullptr)
            return;
        _liveNodeId = node->id;
        _liveBaseline = ResolveLiveParts(node);
    }

    // The live parts that differ from the baseline, taking them into it. Empty when nothing
    // changed (or the baseline was for another node, or the part list changed shape - both
    // rebaseline silently rather than mis-pair old and new values).
    static std::vector<std::string> TakeLiveChanges(const GraphNode* node)
    {
        std::vector<std::string> changed;
        if (!_liveValid || !(_liveNodeId == node->id))
        {
            ResetLiveBaseline(node);
            return changed;
        }
        auto texts = ResolveLiveParts(node);
        if (texts.size() != _liveBaseline.size())
        {
            _liveBaseline = std::move(texts);
            return changed;
        }
        for (size_t i = 0; i < texts.size(); i++)
        {
            if (texts[i] != _liveBaseline[i])
            {
                if (!texts[i].empty())
                    changed.push_back(texts[i]);
                _liveBaseline[i] = texts[i];
            }
        }
        return changed;
    }

    static void LiveWatchTick(const GraphNode* node)
    {
        for (const auto& text : TakeLiveChanges(node))
            ScreenReaderSpeak(text, false); // passive change: queued (A7)
    }

    // ---- speech (the ONLY announce paths - A4) ----

    // The game's own tooltip for the widget under a control's focus box: what the mouse would show,
    // through the window's own onTooltip, so dynamic tooltips come out formatted as on screen. Not
    // while a dropdown is open - its rows sit over the window's widgets.
    static std::string WidgetTooltipText(const GraphNode* node, WindowBase& w)
    {
        auto* windowMgr = GetWindowManager();
        if (node == nullptr || !node->vtable.focusRect || windowMgr == nullptr
            || windowMgr->FindByClass(WindowClass::dropdown) != nullptr)
            return {};
        try
        {
            const auto rect = node->vtable.focusRect();
            if (!rect.has_value())
                return {};
            const ScreenCoordsXY centre{ rect->x + rect->width / 2, rect->y + rect->height / 2 };
            const WidgetIndex widx = windowMgr->FindWidgetFromPoint(w, centre);
            if (widx == kWidgetIndexNull || widx >= w.widgets.size())
                return {};
            const auto& widget = w.widgets[widx];
            if (widget.flags.has(WidgetFlag::tooltipIsString))
                return widget.sztooltip != nullptr ? std::string(widget.sztooltip) : std::string();
            auto result = w.onTooltip(widx, widget.tooltip);
            if (result.str == kStringIdNone)
                return {};
            return FormatStringIDLegacy(result.str, result.args.Data());
        }
        catch (...)
        {
            LogRecipeFailure(w.classification, "tooltip threw");
            return {};
        }
    }

    // Letters and digits only, lower-cased: for asking whether one text already says another.
    static std::string Squashed(const std::string& text)
    {
        std::string out;
        for (unsigned char c : text)
            if (std::isalnum(c))
                out += static_cast<char>(std::tolower(c));
        return out;
    }

    // A landing reads the control's tooltip after it, so nothing waits behind a key (Ross's choice;
    // §7.2 would keep it behind a tooltip verb). Only on arriving at a control - not when a press at
    // an edge re-reads the one already focused - and not when the line already says it, as many of
    // the game's tooltips just repeat the button's name. The "Automatically read tooltips" option
    // turns this off; Shift+Enter reads the tooltip either way.
    static std::string WithTooltip(std::string line, const GraphNode* from, const GraphNode* to)
    {
        if (!Config::Get().sound.accessibilityAutoReadTooltips)
            return line;
        if (to == nullptr || (from != nullptr && from->id == to->id))
            return line;
        auto* w = FrontNavigableWindow();
        if (w == nullptr || w->classification != _attachedClass)
            return line;
        const std::string tip = WidgetTooltipText(to, *w);
        const std::string squashedTip = Squashed(tip);
        if (squashedTip.empty() || Squashed(line).find(squashedTip) != std::string::npos)
            return line;
        return line.empty() ? tip : line + ". " + tip;
    }

    // A keypress-driven landing: speak (interrupting) and pre-seed the differ so it stays silent.
    static void AnnounceLanding(KeyGraph& kg, const GraphNode* from, const GraphNode* to, const std::string& label = {})
    {
        if (to == nullptr)
            return;
        auto line = WithTooltip(GraphAnnouncer::Compose(from, to, label), from, to);
        if (!line.empty())
            ScreenReaderSpeak(line, true);
        _lastSpoken = to->id;
        _lastSpokenValid = true;
        ResetLiveBaseline(to);
        UpdateFocusRect(to);
    }

    static void AnnounceMove(KeyGraph& kg, const MoveResult& r)
    {
        // Unmoved (to == from) composes to just the leaf readout - re-speaking the current
        // control at a hard edge, so an arrow press always gives feedback.
        AnnounceLanding(kg, r.from, r.to, r.transitionLabel);
    }

    // After an activation/adjust: if the action moved focus (a sub-list opened/closed), announce
    // the landing; otherwise speak the node's synchronous state line (§7.5), rebaselining the
    // live watch so the same change isn't spoken twice. With no state line, speak whichever live
    // parts the keypress has already changed - a click that takes effect at once (a toggle, a sort)
    // - as the keypress's answer. That last step is this mod's extension beyond §7.5, which leaves
    // synchronous changes to StateText alone: it makes every control answer its press, including
    // ones whose recipe never declared a state line. A queued game action's change has not landed yet, so its part is
    // still unchanged here and stays in the baseline as the old value, for the watch to speak.
    // `spokenBefore` is ScreenReaderSpokenCount() from before the keypress ran: a recipe that spoke
    // its own confirmation ("Sound muted") has answered, so the changed parts are taken silently.
    static void SpeakStateFeedback(KeyGraph& kg, uint32_t spokenBefore)
    {
        // Taken first so a note never outlives the keypress it was written for.
        std::string note;
        if (auto* state = kg.State(); state != nullptr)
            note = std::exchange(state->nextLandingNote, std::string());
        if (!kg.Rerender())
            return;
        auto* node = kg.CurrentNode();
        if (node == nullptr)
            return;
        if (!_lastSpokenValid || !(_lastSpoken == node->id))
        {
            auto* from = _lastSpokenValid ? kg.Current()->NodeAt(_lastSpoken) : nullptr;
            AnnounceLanding(kg, from, node, note);
            return;
        }
        std::string t = node->vtable.stateText ? GuardedText(node->vtable.stateText) : std::string();
        if (!note.empty())
            t = t.empty() ? note : note + ", " + t;
        if (!t.empty())
        {
            ScreenReaderSpeak(t, true);
            ResetLiveBaseline(node);
        }
        else if (ScreenReaderSpokenCount() != spokenBefore)
        {
            ResetLiveBaseline(node);
        }
        else
        {
            SpeechBuilder changed;
            for (const auto& text : TakeLiveChanges(node))
                changed.add(text);
            if (!changed.empty())
                ScreenReaderSpeak(changed.str(), true);
        }
        UpdateFocusRect(node);
    }

    // ---- attachment (the screen manager's focus side) ----

    static void AttachTo(WindowClass cls)
    {
        if (cls == _attachedClass)
            return;
        _attachedClass = cls;
        _lastSpokenValid = false; // differ memory resets on attach: the restored landing announces
        _liveValid = false;
        _focusRect.reset();
        _loggedThisAttach.clear();

        if (cls != WindowClass::null)
        {
            const auto* screen = GraphScreenForClass(cls);
            if (screen != nullptr && screen->screenName)
            {
                auto name = GuardedText(screen->screenName);
                if (!name.empty())
                    ScreenReaderSpeak(name, false); // screen name queued; the differ then announces the landing
            }
        }
    }

    // ---- key operations ----

    static void DoTabKey(KeyGraph& kg, const GraphScreen& screen, WindowBase& w, int32_t dir)
    {
        bool switched = false;
        try
        {
            switched = screen.onTabKey(w, dir);
        }
        catch (const std::exception& ex)
        {
            LogRecipeFailure(screen.windowClass, ex.what());
        }
        catch (...)
        {
            LogRecipeFailure(screen.windowClass, "unknown exception");
        }
        if (!switched)
            return;
        // Announce the landing on the new page (keypress path: interrupting). The old page's
        // nodes are gone, so the path diff naturally reads the new page context + landing.
        if (!kg.Rerender())
            return;
        auto* node = kg.CurrentNode();
        if (node == nullptr)
            return;
        auto* from = _lastSpokenValid ? kg.Current()->NodeAt(_lastSpoken) : nullptr;
        AnnounceLanding(kg, from, node);
    }

    // The window's name: the screen's own, else the window's caption as it is drawn - plain text, or a
    // string id formatted without arguments (the caption draw passes none).
    static std::string WindowNameText(const GraphScreen& screen, const WindowBase& w)
    {
        if (screen.screenName)
            return GuardedText(screen.screenName);
        for (const auto& widget : w.widgets)
        {
            if (widget.type != WidgetType::caption)
                continue;
            if (widget.flags.has(WidgetFlag::textIsString))
                return widget.string != nullptr ? std::string(widget.string) : std::string();
            if (widget.text != kStringIdNone)
                return FormatStringID(widget.text);
        }
        return {};
    }

    // F1: the window's title, then its own help, or the list's while one of its combo box lists is
    // open, else the shared texts. The help texts leave the window's name out, as the title says it.
    // A help with more than five commands opens the help window instead.
    static void SpeakWindowHelp(const GraphScreen& screen, WindowBase& w)
    {
        auto* windowMgr = GetWindowManager();
        const bool listOpen = windowMgr != nullptr && windowMgr->FindByClass(WindowClass::dropdown) != nullptr;
        std::string text;
        try
        {
            if (listOpen)
                text = screen.dropdownHelpText ? screen.dropdownHelpText(w) : std::string(Vocab::Help::kWindow);
            else if (screen.helpText)
                text = screen.helpText(w);
        }
        catch (...)
        {
            LogRecipeFailure(screen.windowClass, "helpText threw");
        }
        if (text.empty())
            text = Vocab::Help::kWindow;
        const std::string title = WindowNameText(screen, w);
        if (Windows::AccessibilityHelpOpenIfLong(title, text))
            return;
        ScreenReaderSpeak(title.empty() ? text : title + ". " + text, true);
    }

    static void AnnounceTree(KeyGraph& kg, const KeyGraph::TreeResult& tr)
    {
        switch (tr.kind)
        {
            case KeyGraph::TreeMove::expanded:
                ScreenReaderSpeak(Vocab::ExpandedState(true), true);
                break;
            case KeyGraph::TreeMove::collapsed:
                ScreenReaderSpeak(Vocab::ExpandedState(false), true);
                break;
            case KeyGraph::TreeMove::emptyGroup:
                ScreenReaderSpeak(Vocab::kEmptyGroup, true);
                break;
            case KeyGraph::TreeMove::descended:
            case KeyGraph::TreeMove::ascended:
                AnnounceMove(kg, tr.move);
                break;
            default:
                break; // leaf/none: consumed silently
        }
    }

    // First-letter type-ahead: jump to the next control in the focused node's Tab-stop whose
    // search text starts with the letter (declaration order, wrapping). Letters are consumed
    // while a graph screen is focused, so stray presses never leak map verbs into a menu.
    static bool TypeAhead(KeyGraph& kg, char letter)
    {
        if (!kg.Rerender())
            return true;
        auto* cur = kg.CurrentNode();
        auto* render = kg.Current();
        if (cur == nullptr)
            return true;

        std::vector<GraphNode*> stopNodes;
        for (auto* n : render->order)
        {
            if (n->stopKey == cur->stopKey && !n->vtable.excludeFromSearch)
                stopNodes.push_back(n);
        }
        size_t curIdx = 0;
        for (size_t i = 0; i < stopNodes.size(); i++)
        {
            if (stopNodes[i] == cur)
            {
                curIdx = i;
                break;
            }
        }
        for (size_t i = 1; i <= stopNodes.size(); i++)
        {
            auto* cand = stopNodes[(curIdx + i) % stopNodes.size()];
            auto text = cand->vtable.searchText ? GuardedText(cand->vtable.searchText)
                                                : GraphAnnouncer::FirstPartText(cand);
            if (text.empty())
                continue;
            const char first = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
            if (first != letter)
                continue;

            // Focus() re-renders (freeing cur/cand) - carry ids across, then announce.
            const ControlId fromId = cur->id;
            const ControlId toId = cand->id;
            if (!kg.Focus(toId))
                return true;
            auto* to = kg.Current()->NodeAt(toId);
            auto* from = kg.Current()->NodeAt(fromId);
            AnnounceLanding(kg, from, to);
            return true;
        }
        return true; // no match: still consumed
    }

    static bool ProcessKey(KeyGraph& kg, const GraphScreen& screen, WindowBase& w, uint32_t key, uint32_t modifiers)
    {
        const bool shift = (modifiers & KMOD_SHIFT) != 0;
        const bool ctrl = (modifiers & KMOD_CTRL) != 0;
        const bool alt = (modifiers & KMOD_ALT) != 0;

        switch (key)
        {
            case SDLK_UP:
            case SDLK_DOWN:
            {
                const auto dir = key == SDLK_UP ? GraphDir::up : GraphDir::down;
                auto r = kg.Move(dir);
                if (!r.moved && r.to != nullptr && screen.wrapArrows)
                    r = kg.MoveToEdge(dir == GraphDir::up ? GraphDir::down : GraphDir::up);
                AnnounceMove(kg, r);
                return true;
            }
            case SDLK_LEFT:
            case SDLK_RIGHT:
            {
                const int32_t sign = key == SDLK_RIGHT ? 1 : -1;
                // Evaluation order (§7.2): adjust -> move -> tree semantics -> page switch. Shift
                // asks for a large adjust; a control without a large step treats it as a normal one.
                const uint32_t spokenBefore = ScreenReaderSpokenCount();
                if (kg.TryAdjust(sign, shift))
                {
                    SpeakStateFeedback(kg, spokenBefore);
                    return true;
                }
                auto r = kg.Move(key == SDLK_RIGHT ? GraphDir::right : GraphDir::left);
                if (r.moved)
                {
                    AnnounceMove(kg, r);
                    return true;
                }
                auto* n = kg.CurrentNode();
                if (n != nullptr && KeyGraph::InTree(n))
                {
                    auto tr = key == SDLK_RIGHT ? kg.TreeRight() : kg.TreeLeft();
                    AnnounceTree(kg, tr);
                    return true;
                }
                // Legacy parity: in paged LIST windows Left/Right also switch pages (as the mod's
                // list windows always have). Form screens opt out - see sideArrowsChangePage.
                if (screen.onTabKey && screen.sideArrowsChangePage)
                {
                    DoTabKey(kg, screen, w, sign);
                    return true;
                }
                // Nothing to adjust and nowhere to go: re-read the focused control. An unmoved
                // result composes to just the leaf readout, so the key still answers rather than
                // going silent - and on a form screen that is what Left/Right means on a checkbox
                // or a button.
                AnnounceMove(kg, r);
                return true; // the focused control's chords never leak to the game
            }
            case SDLK_TAB:
                if (screen.onTabKey)
                {
                    DoTabKey(kg, screen, w, shift ? -1 : 1);
                    return true;
                }
                if (screen.tabMovesBetweenItems)
                {
                    const auto dir = shift ? GraphDir::up : GraphDir::down;
                    auto r = kg.Move(dir);
                    if (!r.moved && r.to != nullptr && screen.wrapArrows)
                        r = kg.MoveToEdge(shift ? GraphDir::down : GraphDir::up);
                    AnnounceMove(kg, r);
                    return true;
                }
                {
                    // Zones (§4.3, §7.7): a screen built as several Tab-stops gets its Tab cycling
                    // here, wrapping, landing where each stop was left (else its selected member,
                    // else its first control). With one stop there is nowhere to go: Tab stays in
                    // the window (§7.7, and §8 - only one model may act on a key) and re-reads the
                    // focused control, so the press still answers. It used to fall through to the map
                    // cursor, which opened the toolbar behind the window.
                    auto r = kg.MoveStop(shift ? -1 : 1, true);
                    AnnounceMove(kg, r);
                    return true;
                }
            case SDLK_F1:
                // F1 is the window's title, then its help. Shift+F1 is the game's land tool and backtick
                // its console, here as on the map: tooltips are read on landing instead.
                if (shift || ctrl || alt)
                    return false;
                if (screen.f1Closes)
                {
                    if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                        windowMgr->CloseByClass(screen.windowClass);
                    return true;
                }
                SpeakWindowHelp(screen, w);
                return true;
            case SDLK_BACKSPACE:
            {
                // The secondary activation (§7.2). Claimed only by a control that has one: the
                // game binds Backspace to "close the top window", and that stays everywhere else.
                if (shift || ctrl || alt)
                    return false;
                const uint32_t spokenBefore = ScreenReaderSpokenCount();
                if (!kg.Secondary())
                    return false;
                SpeakStateFeedback(kg, spokenBefore);
                return true;
            }
            case SDLK_HOME:
            case SDLK_END:
            {
                const bool first = key == SDLK_HOME;
                if (!kg.Rerender())
                    return true;
                auto* n = kg.CurrentNode();
                MoveResult r;
                if (n != nullptr && KeyGraph::InTree(n))
                    r = kg.MoveToSiblingEdge(first);
                else
                    r = kg.MoveToEdge(first ? GraphDir::up : GraphDir::down);
                AnnounceMove(kg, r);
                return true;
            }
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            {
                const uint32_t spokenBefore = ScreenReaderSpokenCount();
                bool acted;
                if (shift)
                {
                    // A control's own Shift+Enter (following a guest, say) comes first; everywhere
                    // else Shift+Enter reads the focused control's tooltip.
                    acted = kg.ActivateShift();
                    if (!acted)
                    {
                        const std::string tip = WidgetTooltipText(kg.CurrentNode(), w);
                        ScreenReaderSpeak(tip.empty() ? std::string(Vocab::kNoTooltip) : tip, true);
                        return true;
                    }
                }
                else if (ctrl)
                    acted = kg.ActivateCtrl();
                else
                    acted = kg.Activate();
                if (acted)
                    SpeakStateFeedback(kg, spokenBefore);
                return true; // actionless activate is consumed silently (§3.4)
            }
            case SDLK_ESCAPE:
            {
                const uint32_t spokenBefore = ScreenReaderSpokenCount();
                bool handled = false;
                if (screen.onEscape)
                {
                    try
                    {
                        handled = screen.onEscape(w);
                    }
                    catch (...)
                    {
                        LogRecipeFailure(screen.windowClass, "onEscape threw");
                    }
                }
                if (handled)
                {
                    // A sub-list closed; announce where focus landed (keypress path).
                    SpeakStateFeedback(kg, spokenBefore);
                }
                else
                {
                    auto* windowMgr = GetWindowManager();
                    if (windowMgr != nullptr)
                        windowMgr->CloseByClass(screen.windowClass);
                    // Returning to the toolbar menu: re-announce the item we land on (legacy
                    // parity). Other reveals/close cues come from the tick.
                    ReannounceToolbarItemIfMenuMode();
                }
                return true;
            }
            default:
                if (key >= SDLK_a && key <= SDLK_z && !ctrl && !alt)
                    return TypeAhead(kg, static_cast<char>(key));
                return false;
        }
    }

    // ---- public entry points ----

    bool HandleGraphNavigationKey(const InputEvent& e)
    {
        if (e.deviceKind != InputDeviceKind::keyboard)
            return false;
        EnsureGraphScreensRegistered();
        EnsureAnnouncerHooks();

        // While a shortcut rebind is pending (the Change Shortcut window is open, e.g. from the
        // graph-owned shortcut-keys window), the next key press is captured by the shortcut manager
        // as the new binding. Let every key fall through to it, except Escape, which cancels the
        // rebind (Escape is otherwise a valid binding).
        if (auto& sm = GetShortcutManager(); sm.isPendingShortcutChange())
        {
            if (e.button == SDLK_ESCAPE)
            {
                if (e.state == InputEventState::down)
                {
                    if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                        windowMgr->CloseByClass(WindowClass::changeKeyboardShortcut);
                    ScreenReaderSpeak("Rebind cancelled");
                }
                _lastHandledKey = e.button;
                return true;
            }
            return false;
        }

        auto* front = FrontNavigableWindow();
        if (front == nullptr || !GraphScreenHoldsKeyboard(front->classification))
            return false;

        // Key-up: swallow the key we consumed on key-down.
        if (e.state != InputEventState::down)
        {
            if (e.button == _lastHandledKey)
            {
                _lastHandledKey = 0;
                return true;
            }
            return false;
        }

        AttachTo(front->classification); // keys can arrive before the tick

        const auto* screen = GraphScreenForClass(_attachedClass);
        if (screen == nullptr)
            return false;
        auto kg = MakeGraph(_attachedClass);
        const bool handled = ProcessKey(kg, *screen, *front, e.button, e.modifiers) || screen->holdsEveryKey;
        _lastHandledKey = handled ? e.button : 0;
        return handled;
    }

    void TickGraphScreens()
    {
        EnsureGraphScreensRegistered();
        EnsureAnnouncerHooks();
        DropGraphStatesForClosedWindows();

        auto* front = FrontNavigableWindow();
        const bool graphFront = front != nullptr && GraphScreenHoldsKeyboard(front->classification);

        if (!graphFront)
        {
            if (_attachedClass != WindowClass::null)
                AttachTo(WindowClass::null);
            return;
        }

        AttachTo(front->classification);

        // The frame differ (A4): rebuild + reconcile, then speak iff the focused identity
        // differs from the identity last spoken. Landings that arrive here queue - they follow
        // the screen name or the keypress feedback that caused them.
        auto kg = MakeGraph(_attachedClass);
        if (!kg.Rerender())
            return; // nothing declared this frame: keep focus state for the next good render
        auto* node = kg.CurrentNode();
        if (node == nullptr)
            return;
        if (!_lastSpokenValid || !(_lastSpoken == node->id))
        {
            auto* from = _lastSpokenValid ? kg.Current()->NodeAt(_lastSpoken) : nullptr;
            auto line = WithTooltip(GraphAnnouncer::Compose(from, node), from, node);
            if (!line.empty())
                ScreenReaderSpeak(line, false); // differ path queues (A7)
            _lastSpoken = node->id;
            _lastSpokenValid = true;
            ResetLiveBaseline(node);
            UpdateFocusRect(node);
        }
        else
        {
            LiveWatchTick(node);
            UpdateFocusRect(node);
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
