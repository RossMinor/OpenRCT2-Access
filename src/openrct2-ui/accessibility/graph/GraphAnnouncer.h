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
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility::Graph::GraphAnnouncer
{
    // Composes the spoken line for a focus change by diffing the old and new focus PATHS - each
    // node's ancestor chain (GraphNode::parent) plus the node itself, compared by identity.
    // Newly-entered levels read outermost-first, then the landing control ("Display settings,
    // list, Fullscreen, toggle, on"). Sibling moves share the whole prefix and read just the
    // control; ascends likewise; and descending from a group onto its own child re-announces
    // nothing but the child - the group is on the child's chain AND is the from-node, so the
    // prefix swallows it.

    // The line for landing on `to` having come from `from` (null = from nothing: the full path
    // reads). `transitionLabel` is the crossed edge's spoken line, when it had one. Empty when
    // there is nothing to say.
    std::string Compose(const GraphNode* from, const GraphNode* to, const std::string& transitionLabel = {});

    // The full readout for a landing with no prior focus (screen entry, focus restore).
    std::string ComposeFull(const GraphNode* to);

    // A node's EFFECTIVE announcement parts: the control type's common parts (the role word)
    // merged with the node's own - a node part overrides a common part of the same kind (as an
    // all-or-nothing drop judged against the node's declared list; a common part with an empty
    // kind can never be overridden) - sorted by the type's kind order with a STABLE sort
    // (unknown/kindless parts keep declaration order, after the ordered kinds), then filtered by
    // the installed part filter. This single list feeds both readouts and the live watch.
    std::vector<NodeAnnouncement> EffectiveAnnouncements(const GraphNode* node);

    // A node's own readout: its effective parts resolved live, non-empty ones joined with ", " -
    // plus, for an expandable group that doesn't speak its own expansion, the expanded/collapsed
    // state word - plus the auto-stamped "n of m" position (unless the node carries its own).
    // Empty when there is nothing to say.
    std::string LeafText(const GraphNode* node);

    // The first announcement part's text (the label) - for dedupe and search fallbacks.
    std::string FirstPartText(const GraphNode* node);

    // ---- host hooks, installed once at boot; null-safe (null = everything speaks, no auto
    // positions, no state words - which is what keeps the kernel testable standalone) ----

    // Per-part filter (consults user settings). Returning false drops the part from readouts AND
    // the live watch. The auto-stamped position is routed through this as a synthetic probe part
    // of kind `position`, so a per-kind toggle governs a part that appears in no node's list.
    extern std::function<bool(const ControlType*, const NodeAnnouncement&)> PartFilter;

    // Pluggable "n of m" wording (localized by the host); null = no auto positions.
    extern std::function<std::string(int32_t index, int32_t count)> PositionText;

    // Pluggable expanded/collapsed wording for group headers; null = groups don't speak state.
    extern std::function<std::string(bool expanded)> ExpandedStateText;
} // namespace OpenRCT2::Ui::Accessibility::Graph::GraphAnnouncer
