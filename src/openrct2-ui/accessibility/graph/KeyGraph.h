/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "GraphTypes.h"

#include <functional>
#include <memory>
#include <vector>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    // The outcome of a navigation operation, for the caller (navigator) to announce. The kernel
    // never speaks - it returns what happened.
    //
    // LIFETIME: `from`/`to` point into the engine's current render and are valid ONLY until the
    // next operation - the next rebuild frees them. Carry a ControlId, not a pointer, across
    // operations.
    struct MoveResult
    {
        bool moved = false;
        GraphNode* from = nullptr; // node before the operation (null on first landing)
        GraphNode* to = nullptr;   // node after (== from when at an edge; null when the graph is empty)
        std::string transitionLabel; // the crossed edge's spoken line, when it had one
    };

    // The navigation engine: a directed graph of controls rebuilt from a render callback on EVERY
    // operation, with focus persisting in an external GraphState. Transcribed from the Graph A11y
    // Kernel reference (lineage: Factorio Access key-graph.lua). Two invariants:
    //
    // - Down-right total order (ComputeOrder): from the start node, go right until stuck, queueing
    //   each down - visits a planar UI in reading order; unreached nodes (later Tab-stops) append
    //   in declaration order, keeping the order total.
    // - Focus recovery on rebuild (Reconcile): if the focused control vanished, land on the
    //   nearest survivor rather than jumping to the start - following the backing object that
    //   moved (tier 1, with the shared-reference tie-break) or the logical control whose backing
    //   object was rebuilt (tier 2) first.
    class KeyGraph
    {
    public:
        using RenderCallback = std::function<std::unique_ptr<GraphRender>()>;

        KeyGraph(RenderCallback renderCallback, GraphState* state)
            : _renderCallback(std::move(renderCallback))
            , _state(state)
        {
        }

        GraphState* State()
        {
            return _state;
        }

        // The most recently built render, or null if not yet rendered / empty.
        GraphRender* Current()
        {
            return _current.get();
        }

        // The focused node in the current render, or null.
        GraphNode* CurrentNode()
        {
            return _current != nullptr ? _current->NodeAt(_state->curKey) : nullptr;
        }

        // Rebuild the render and reconcile focus into it. False when the callback produced nothing
        // (the caller should treat the graph as closed/empty; focus state is left intact for the
        // next good render).
        bool Rerender();

        // Move focus from the cached curKey to a valid control in `render`, then recompute the
        // traversal order.
        static void Reconcile(const GraphRender& render, GraphState& state);

        // The down-right total order (see class comment).
        static std::vector<ControlId> ComputeOrder(const GraphRender& render);

        // ---- navigation operations (each re-renders first, acting on current reality) ----

        // One step in `dir`. Not moved (at an edge / empty) -> to == from.
        MoveResult Move(GraphDir dir);

        // As far as possible in `dir`.
        MoveResult MoveToEdge(GraphDir dir);

        // Cycle to the next/previous Tab-stop (declaration order), landing per StopLanding.
        MoveResult MoveStop(int32_t dir, bool wrap);

        // Jump to the next/previous region within the current stop, landing on its first node.
        MoveResult MoveRegion(int32_t dir);

        // Programmatic focus (re-renders, then lands on the id). False when it isn't present.
        bool Focus(const ControlId& id);

        // Tier-1 focus sync from the game: if a node's backing object is `reference`, move focus
        // there. Re-renders first, like every operation. True if focus changed nodes.
        bool FocusByReference(const void* reference);

        // Where focus lands when entering a stop with no active cursor: the remembered position
        // (validated to still belong to the stop), else the SELECTED member, else the first node.
        GraphNode* StopLanding(const std::string& stopKey);
        static GraphNode* StopLanding(const GraphRender& render, const GraphState& state, const std::string& stopKey);

        // The first node in a stop carrying a non-empty selected-kind part, or null. The probe
        // resolves announcement closures (reads live application state); it is guarded - a
        // throwing part reads as not-selected.
        static GraphNode* SelectedNodeInStop(const GraphRender& render, const std::string& stopKey);

        // ---- tree operations (Right/Left semantics for expandable groups) ----

        enum class TreeMove : uint8_t
        {
            none,       // not applicable here - caller decides consume/bubble
            expanded,   // the focused group expanded (focus unchanged; speak its new state)
            collapsed,  // the focused group collapsed (focus unchanged; speak its new state)
            emptyGroup, // expanding found no children - auto-recollapsed (speak "no details")
            descended,  // moved to the group's first child (announce as a move)
            ascended,   // moved to the nearest focusable ancestor (announce as a move)
            leaf,       // Right on a non-group inside a tree - consumed, nothing to descend into
        };

        struct TreeResult
        {
            TreeMove kind = TreeMove::none;
            MoveResult move; // valid for descended/ascended
        };

        // Is this node part of an expandable structure (itself a group, or under one)?
        static bool InTree(const GraphNode* node);

        // Right on a group: expand (auto-recollapse when empty), or descend into an expanded one.
        // Right elsewhere in a tree: leaf (consume).
        TreeResult TreeRight();

        // Left on an expanded group: collapse. Left elsewhere in a tree: ascend to the nearest
        // focusable ancestor.
        TreeResult TreeLeft();

        // Home/End at the current tree depth: the first/last node sharing the focused node's
        // parent AND stop (root-level nodes all share the null parent; without the stop filter
        // this would scan every stop, which arrows can never do).
        MoveResult MoveToSiblingEdge(bool first);

        // ---- behavior invokers (the caller announces fallbacks / state) ----

        bool Activate();
        bool Secondary();
        bool ActivateShift();
        bool ActivateCtrl();
        bool Tooltip();

        // If the focused control adjusts horizontally (a slider), adjust and return true;
        // false = the caller should navigate instead.
        bool TryAdjust(int32_t sign, bool large);

    private:
        RenderCallback _renderCallback;
        GraphState* _state;
        std::unique_ptr<GraphRender> _current;

        void SetCurrent(GraphNode* node);
        void SetExpanded(GraphNode* group, bool expanded);
        GraphNode* FirstChildOf(const GraphNode* group);
        std::vector<std::string> StopOrder() const;
        static void RememberStop(const GraphRender& render, GraphState& state, const ControlId& key);
    };
} // namespace OpenRCT2::Ui::Accessibility::Graph
