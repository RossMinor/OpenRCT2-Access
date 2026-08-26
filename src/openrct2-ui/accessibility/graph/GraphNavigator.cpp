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
#include <openrct2/Diagnostic.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/ui/WindowManager.h>
#include <unordered_set>

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

    static void UpdateFocusRect(const GraphNode* node)
    {
        _focusRect.reset();
        if (node != nullptr && node->vtable.focusRect)
        {
            try
            {
                _focusRect = node->vtable.focusRect();
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

    static void LiveWatchTick(const GraphNode* node)
    {
        if (!_liveValid || !(_liveNodeId == node->id))
        {
            ResetLiveBaseline(node);
            return;
        }
        auto texts = ResolveLiveParts(node);
        if (texts.size() != _liveBaseline.size())
        {
            // A rebuild grew or shrank the part list - rebaseline silently rather than mis-pair
            // old and new values.
            _liveBaseline = std::move(texts);
            return;
        }
        for (size_t i = 0; i < texts.size(); i++)
        {
            if (texts[i] != _liveBaseline[i])
            {
                if (!texts[i].empty())
                    ScreenReaderSpeak(texts[i], false); // passive change: queued (A7)
                _liveBaseline[i] = texts[i];
            }
        }
    }

    // ---- speech (the ONLY announce paths - A4) ----

    // A keypress-driven landing: speak (interrupting) and pre-seed the differ so it stays silent.
    static void AnnounceLanding(KeyGraph& kg, const GraphNode* from, const GraphNode* to, const std::string& label = {})
    {
        if (to == nullptr)
            return;
        auto line = GraphAnnouncer::Compose(from, to, label);
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
    // live watch so the same change isn't spoken twice.
    static void SpeakStateFeedback(KeyGraph& kg)
    {
        if (!kg.Rerender())
            return;
        auto* node = kg.CurrentNode();
        if (node == nullptr)
            return;
        if (!_lastSpokenValid || !(_lastSpoken == node->id))
        {
            auto* from = _lastSpokenValid ? kg.Current()->NodeAt(_lastSpoken) : nullptr;
            AnnounceLanding(kg, from, node);
            return;
        }
        if (node->vtable.stateText)
        {
            auto t = GuardedText(node->vtable.stateText);
            if (!t.empty())
                ScreenReaderSpeak(t, true);
        }
        ResetLiveBaseline(node);
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
                // Evaluation order (§7.2): adjust -> move -> tree semantics -> page switch.
                if (kg.TryAdjust(sign, false))
                {
                    SpeakStateFeedback(kg);
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
                // Legacy parity: in paged windows Left/Right also switch pages (as the mod's
                // list windows always have).
                if (screen.onTabKey)
                    DoTabKey(kg, screen, w, sign);
                return true; // the focused control's chords never leak to the game
            }
            case SDLK_TAB:
                if (screen.onTabKey)
                {
                    DoTabKey(kg, screen, w, shift ? -1 : 1);
                    return true;
                }
                return false; // no pages: fall through (Tab still opens the tools menu)
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
                bool acted;
                if (shift)
                    acted = kg.ActivateShift();
                else if (ctrl)
                    acted = kg.ActivateCtrl();
                else
                    acted = kg.Activate();
                if (acted)
                    SpeakStateFeedback(kg);
                return true; // actionless activate is consumed silently (§3.4)
            }
            case SDLK_ESCAPE:
            {
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
                    SpeakStateFeedback(kg);
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

        auto* front = FrontNavigableWindow();
        if (front == nullptr || !GraphOwnsWindowClass(front->classification))
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
        const bool handled = ProcessKey(kg, *screen, *front, e.button, e.modifiers);
        _lastHandledKey = handled ? e.button : 0;
        return handled;
    }

    void TickGraphScreens()
    {
        EnsureGraphScreensRegistered();
        EnsureAnnouncerHooks();
        DropGraphStatesForClosedWindows();

        auto* front = FrontNavigableWindow();
        const bool graphFront = front != nullptr && GraphOwnsWindowClass(front->classification);

        if (!graphFront)
        {
            if (_attachedClass != WindowClass::null)
            {
                AttachTo(WindowClass::null);
                // A graph window closed revealing a legacy window: poke its announce so the
                // player hears where focus went (the legacy dispatcher only announces on its own
                // keypresses).
                if (front != nullptr)
                    front->onAccessibilityAction(AccessibilityAction::announce);
            }
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
            auto line = GraphAnnouncer::Compose(from, node);
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
