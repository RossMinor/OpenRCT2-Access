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

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    // Builds a GraphRender. Two construction styles, freely mixable in one build:
    //
    // MENU MODE - rows of controls, wired automatically: Left/Right within a row, Up/Down between
    // consecutive rows of the same Tab-stop (two rows sharing a non-empty row key get
    // column-preserving vertical navigation). Items added outside an explicit row become
    // single-item rows (a plain vertical menu).
    //
    // RAW MODE - AddNode + Connect for arbitrary topologies (computed adjacency).
    //
    // Orthogonal to both: BeginStop groups nodes into Tab-stops (arrows never cross a stop),
    // SetRegion tags nodes for coarse jumps, and the PARENT STACK builds the presentation
    // hierarchy: PushContext pushes a non-focusable structural level ("Display settings, list" -
    // announced when focus enters from outside), while BeginGroup pushes a focusable, EXPANDABLE
    // group header (a tree section) whose children only emit while it is expanded - expansion
    // state lives in the persistent set the builder is constructed with (GraphState::expanded),
    // so screens hold no tree state of their own. Nesting recurses; a collapsed ancestor
    // suppresses everything beneath it.
    //
    // Misuse (duplicate ids, unclosed/empty rows, stop/group inside an open row) throws
    // std::logic_error - the navigator's fault isolation catches and logs it, so a bad recipe
    // never crashes the frame tick.
    class GraphBuilder
    {
    public:
        explicit GraphBuilder(const std::unordered_set<ControlId>* expansion = nullptr);

        // ---- stops / regions ----

        // Start a new Tab-stop; nodes added from here belong to it. `key` must be stable across
        // rebuilds (it keys the stop's remembered position); empty auto-assigns by index, which
        // is stable when the screen builds its stops in a fixed order.
        GraphBuilder& BeginStop(const std::string& key = {});

        // Tag nodes added from here with a region (coarse jump target) within the current stop;
        // empty clears. Region keys must be stable across rebuilds.
        GraphBuilder& SetRegion(const std::string& key);

        // ---- the parent stack: contexts + groups ----

        // Push one NON-FOCUSABLE level of presentation hierarchy ("Display settings", "list")
        // onto nodes added from here - pure structure: never navigable, announced when focus
        // enters from outside. Close with PopContext. Note: two sibling contexts with the same
        // label collide (the synthetic id is label-pathed) - the announcer then treats them as
        // one level and goes silent when focus crosses between them.
        GraphBuilder& PushContext(const std::string& label, const std::string& role = {}, bool positions = true);
        GraphBuilder& PopContext();

        // Push a FOCUSABLE, expandable group header (a tree section): the header emits as a
        // navigable node, and children declared before EndGroup emit only while the group is
        // expanded. Expansion state: `expanded` when given, else the persistent expansion set,
        // else `defaultExpanded`.
        GraphBuilder& BeginGroup(
            const ControlId& id, NodeVtable vtable, std::optional<bool> expanded = std::nullopt,
            bool defaultExpanded = false);
        GraphBuilder& EndGroup();

        // Whether a group id is expanded in the persistent set - for screens that must avoid even
        // BUILDING a collapsed group's children (a lazy hierarchy).
        bool IsExpanded(const ControlId& id) const;

        // Focus starts here when the graph has no prior position (defaults to the first node).
        GraphBuilder& SetStart(const ControlId& id);

        // ---- menu mode ----

        // Open a horizontal row. Rows sharing a non-empty `rowKey` with the row above/below get
        // column-preserving vertical navigation.
        GraphBuilder& StartRow(const std::string& rowKey = {});
        GraphBuilder& EndRow();

        // Add a control - into the open row, or as its own single-item row. A no-op inside a
        // collapsed group's subtree.
        GraphBuilder& AddItem(const ControlId& id, NodeVtable vtable);

        // Add a read-only line (label only; no actions).
        GraphBuilder& AddLabel(const ControlId& id, std::function<std::string()> label);

        // ---- raw mode ----

        // Add a node with no automatic wiring (wire with Connect). A no-op inside a collapsed
        // group's subtree.
        GraphBuilder& AddNode(const ControlId& id, NodeVtable vtable);

        // Directed edge from -> to, with an optional spoken transition line ("lane change").
        // Edges to/from undeclared nodes are dropped at build. Raw edges are not stop-checked -
        // do not wire across Tab-stops.
        GraphBuilder& Connect(const ControlId& from, GraphDir dir, const ControlId& to, const std::string& label = {});

        // ---- build ----

        // Finalize into a render, or null when nothing was declared (the caller treats the screen
        // as closed/empty and leaves focus state intact for the next good render). Single-use:
        // the builder is spent after Build.
        std::unique_ptr<GraphRender> Build();

    private:
        struct Row
        {
            std::vector<GraphNode*> items;
            std::string key;     // empty = no column pairing
            std::string stopKey;
        };

        struct RawEdge
        {
            ControlId from;
            GraphDir dir;
            ControlId to;
            std::string label;
        };

        struct ParentFrame
        {
            GraphNode* node = nullptr; // the parent node (context, or the group header)
            bool suppressed = false;   // this frame's subtree is swallowed
        };

        const std::unordered_set<ControlId>* _expansion;

        // Menu mode.
        std::vector<std::unique_ptr<Row>> _rows;
        Row* _currentRow = nullptr;

        // Raw mode.
        std::vector<GraphNode*> _rawNodes;
        std::vector<RawEdge> _rawEdges;

        // Every node in DECLARATION order regardless of mode - the render's node order (and so
        // the Tab-stop cycle) must interleave menu rows and raw nodes as the screen declared them.
        std::vector<GraphNode*> _declared;

        // The menu row each menu-mode node belongs to (absent for raw nodes) - for stitching the
        // vertical gap where a stop mixes menu rows with raw content.
        std::unordered_map<const GraphNode*, Row*> _rowOf;

        // Ownership of every node created during the build (incl. context parents and headers);
        // transferred to the render at Build.
        std::vector<std::unique_ptr<GraphNode>> _owned;

        std::unordered_set<ControlId> _ids;
        ControlId _start;

        std::string _stopKey;
        int32_t _stopAuto = 1;
        std::string _regionKey;

        std::vector<ParentFrame> _parents;

        GraphNode* CurrentParent() const;
        bool Suppressed() const;
        GraphNode* MakeNode(const ControlId& id, NodeVtable vtable);
        void WireMenuEdges();
        void StitchModeBoundaries();
        void StampPositions();
        static void Stamp(std::vector<GraphNode*>& siblings);
        static ControlId VerticalTarget(const Row& from, const Row& to, size_t pos);
    };
} // namespace OpenRCT2::Ui::Accessibility::Graph
