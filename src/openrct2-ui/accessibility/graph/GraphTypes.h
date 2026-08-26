/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "ControlId.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The Graph A11y Kernel data model - engine-neutral. Transcribed from the reference implementation
// (RTAccess, Draft 2 of the spec). Everything here is pure data + closures over live application
// state; nothing depends on the game engine, which is what keeps the kernel testable standalone.
namespace OpenRCT2::Ui::Accessibility::Graph
{
    // The four navigable directions between graph nodes (explicit edges). Tab-stop cycling and
    // region jumps are OPERATIONS over node metadata (GraphNode::stopKey / regionKey), not edges -
    // they carry per-stop remembered positions, which a static edge can't express.
    enum class GraphDir : uint8_t
    {
        up,
        right,
        down,
        left,
    };

    // The well-known announcement-part kinds. A part's kind is its identity for control-type
    // ordering, node-over-type overriding, and (future) per-kind announcement settings.
    namespace AnnouncementKinds
    {
        inline constexpr const char* kLabel = "label";
        inline constexpr const char* kRole = "role";
        inline constexpr const char* kValue = "value";
        // Semantic, not cosmetic: `selected` is where initial focus and Tab landings resolve.
        // Marking a merely-notable node reroutes initial focus past every node declared before it.
        inline constexpr const char* kSelected = "selected";
        inline constexpr const char* kEnabled = "enabled";
        inline constexpr const char* kTooltip = "tooltip";
        inline constexpr const char* kPosition = "position";
        inline constexpr const char* kReason = "reason";
    } // namespace AnnouncementKinds

    // One part of a control's spoken focus readout ("Ride music" / "toggle" / "on"), resolved live
    // at speak time. Empty at speak time = the part stays silent this time. A part may be a
    // build-time snapshot (the rebuild keeps it fresh) - unless `live` is set, in which case it
    // MUST re-read state on every call: a LIVE part is watched while its node is focused, and when
    // its resolved text changes the navigator speaks just that part.
    struct NodeAnnouncement
    {
        std::function<std::string()> text;
        bool live = false;
        std::string kind; // empty = a custom one-off part (never ordered, never overridable)

        NodeAnnouncement() = default;
        NodeAnnouncement(std::function<std::string()> textFn, bool isLive = false, std::string kindName = {})
            : text(std::move(textFn))
            , live(isLive)
            , kind(std::move(kindName))
        {
        }

        static NodeAnnouncement Static(std::string value, std::string kindName = {})
        {
            return NodeAnnouncement([value = std::move(value)]() { return value; }, false, std::move(kindName));
        }
    };

    // A CONTROL TYPE - "button", "toggle", "slider" - as a registry value rather than a class.
    // A type owns the speak ORDER of its announcement kinds and the parts COMMON to every control
    // of the type (the role word); nodes contribute their specific parts, overriding a common part
    // of the same kind. Keep ONE registry per host: the key is a settings key, and key collisions
    // across definitions are an error. Give every type an explicit order - a type with common
    // parts and no order speaks role-before-label in the merge.
    struct ControlType
    {
        std::string key;
        std::vector<std::string> order;
        std::function<std::vector<NodeAnnouncement>()> common;
    };

    // A rectangle in screen coordinates, kept as plain ints so the kernel stays engine-neutral.
    // The navigator converts to the engine's rect type when drawing the visual focus box.
    struct GraphRect
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;
    };

    // The behaviors of a control, as data. `announcements` is required (at least one part; the
    // FIRST declared part is the control's label by convention - search, dedupe, and path-diffing
    // rely on this). The rest are optional - a null slot means the chord is consumed silently
    // while the control keeps ownership of it (the focused control's chords never leak to the
    // game).
    struct NodeVtable
    {
        // Required, >= 1 part: the spoken focus readout.
        std::vector<NodeAnnouncement> announcements;

        // The control's type (registry value) - supplies the role word and the speak order.
        // Null = an untyped one-off.
        const ControlType* controlType = nullptr;

        // Primary activation - the left-click equivalent (Enter).
        std::function<void()> onActivate;

        // Secondary activation - the right-click equivalent.
        std::function<void()> onSecondary;

        // Modified activations (Shift+Enter / Ctrl+Enter).
        std::function<void()> onActivateShift;
        std::function<void()> onActivateCtrl;

        // Read/open the control's detail. The action owns the whole behavior so the kernel stays
        // application-agnostic.
        std::function<void()> onTooltip;

        // Horizontal value adjust (sliders, dropdown value cycling): sign is -1/+1, `large`
        // requests a coarse step. When set, Left/Right do NOT navigate.
        std::function<void(int32_t sign, bool large)> onAdjust;

        // The control's state line, spoken immediately (interrupting) after an activation/adjust
        // that changes state - the SYNCHRONOUS feedback path (survives rapid key repeats).
        // Asynchronous/game-driven changes ride `live` parts instead.
        std::function<std::string()> stateText;

        // Type-ahead matching text; null = the first announcement part (the label).
        std::function<std::string()> searchText;

        // If true, type-ahead never matches this control.
        bool excludeFromSearch = false;

        // Host tag: where this control sits on screen, for the visual focus box drawn for sighted
        // users. Optional; kept as plain ints so the kernel stays engine-neutral.
        std::function<std::optional<GraphRect>()> focusRect;

        // Expandable groups: override HOW expansion state changes. When null the engine mutates
        // the persistent expansion set (GraphState::expanded).
        std::function<void()> onExpand;
        std::function<void()> onCollapse;

        // Set when the node's own parts already include that information, so the announcer does
        // not append it twice.
        bool speaksOwnExpansion = false;
        bool speaksOwnPosition = false;
    };

    // A directed edge to another node, with an optional spoken transition line (a "lane change").
    struct Transition
    {
        ControlId destination;
        std::string label; // spoken only while crossing this edge; empty = silent edge

        Transition() = default;
        explicit Transition(ControlId dest, std::string lbl = {})
            : destination(std::move(dest))
            , label(std::move(lbl))
        {
        }
    };

    // A control: identity, behaviors, directional transitions, and structural metadata.
    struct GraphNode
    {
        ControlId id;
        NodeVtable vtable;
        std::unordered_map<GraphDir, Transition> transitions;

        // The node's structural parent within THIS render, or null at screen level. The parent
        // chain IS the presentation hierarchy the announcer prefix-diffs. A parent may be
        // non-focusable pure structure (a labeled panel - `focusable` false, never in
        // nodes/order) or a real control (a tree group header). Raw pointer into the same
        // render; valid only for the render's lifetime.
        GraphNode* parent = nullptr;

        // False for a pure-structure parent (a labeled panel): exists only on parent chains for
        // announcements - never navigable, never in nodes/order.
        bool focusable = true;

        // This node is a group that can expand/collapse (a tree section header).
        bool expandable = false;

        // An expandable group's state AT THIS RENDER (stamped by the builder).
        bool expanded = false;

        // The Tab-stop this node belongs to. Nodes sharing a stopKey form one stop; stop cycling
        // is in first-appearance order, landing on the stop's remembered position.
        std::string stopKey;

        // The region (within a stop) this node belongs to, or empty. Region jumps go between
        // regions in first-appearance order.
        std::string regionKey;

        // Auto-stamped sibling position (1-based) and count, from the builder - "3 of 10" among
        // the siblings arrows actually reach. 0 = none.
        int32_t positionIndex = 0;
        int32_t positionCount = 0;

        // On a parent (context/group) node: its direct children get NO auto position - for
        // log-like streams where "37 of 200" is noise.
        bool suppressChildPositions = false;
    };

    // One built snapshot of a graph: the nodes (keyed by structural identity), their declaration
    // order, and where focus starts when there is no prior position. Rebuilt per operation and
    // thrown away - live state belongs in the node callbacks, not here.
    //
    // LIFETIME (non-GC port): the render OWNS every node, including non-focusable context parents
    // (which appear only on parent chains, never in nodes/order). Any GraphNode* handed out
    // (MoveResult, CurrentNode) points into this render and is valid only until the next
    // operation - the next rebuild frees it. Carry a ControlId, not a pointer, across rebuilds.
    struct GraphRender
    {
        ControlId startKey;
        std::unordered_map<ControlId, GraphNode*> nodes;
        std::vector<GraphNode*> order; // declaration order - drives stop/region cycling and search

        // Ownership of every node in this render (incl. pure-structure parents).
        std::vector<std::unique_ptr<GraphNode>> owned;

        GraphNode* NodeAt(const ControlId& key) const
        {
            if (key.IsEmpty())
                return nullptr;
            auto it = nodes.find(key);
            return it != nodes.end() ? it->second : nullptr;
        }
    };

    // The persistent cursor for a graph - the only thing that survives between renders. One per
    // live screen.
    struct GraphState
    {
        // The focused control's id (carries its reference for tier-1 recovery). Empty until the
        // first render.
        ControlId curKey;

        // The down-right total order from the previous render (for nearest-survivor recovery).
        std::vector<ControlId> keyOrder;

        // If set, focus jumps here on the next render when present (consumed either way).
        ControlId nextSuggestedMove;

        // Remembered position per Tab-stop: where stop cycling lands when returning to a stop.
        std::unordered_map<std::string, ControlId> stopMemory;

        // The expanded groups (by id). The builder consults this for groups declared without an
        // explicit state; the engine's expand/collapse operations mutate it. Screens hold NO
        // expansion state of their own.
        std::unordered_set<ControlId> expanded;
    };
} // namespace OpenRCT2::Ui::Accessibility::Graph
