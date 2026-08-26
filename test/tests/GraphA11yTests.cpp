/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// Conformance tests for the Graph A11y Kernel port (core subset of the spec's 50-test suite):
// builder wiring, reconciliation tiers, order computation, tree semantics, and the announcer's
// path-diff/dedupe/merge behavior. The kernel is host-free, so these run without a game.

#include <gtest/gtest.h>
#include <openrct2-ui/accessibility/graph/GraphAnnouncer.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/KeyGraph.h>

using namespace OpenRCT2::Ui::Accessibility::Graph;

namespace
{
    NodeVtable Vt(const std::string& label)
    {
        NodeVtable vt;
        vt.announcements.push_back(NodeAnnouncement::Static(label));
        return vt;
    }

    NodeVtable VtSelected(const std::string& label, bool selected)
    {
        auto vt = Vt(label);
        if (selected)
            vt.announcements.push_back(NodeAnnouncement::Static("selected", AnnouncementKinds::kSelected));
        return vt;
    }

    ControlId Id(const std::string& key)
    {
        return ControlId::Structural(key);
    }

    const std::string& KeyOf(const GraphNode* n)
    {
        return n->id.StructuralKey();
    }

    // The destination key of a node's edge, or "" when absent.
    std::string EdgeOf(const GraphRender& r, const std::string& from, GraphDir dir)
    {
        auto* n = r.NodeAt(Id(from));
        if (n == nullptr)
            return {};
        auto it = n->transitions.find(dir);
        return it != n->transitions.end() ? it->second.destination.StructuralKey() : std::string();
    }
} // namespace

// ---- builder wiring ----

TEST(GraphA11yBuilder, SingleColumnWiresVertically)
{
    GraphBuilder b;
    b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::down), "b");
    EXPECT_EQ(EdgeOf(*r, "b", GraphDir::down), "c");
    EXPECT_EQ(EdgeOf(*r, "b", GraphDir::up), "a");
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::up), "");
    EXPECT_EQ(EdgeOf(*r, "c", GraphDir::down), "");
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::left), "");
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::right), "");
}

TEST(GraphA11yBuilder, RowWiresHorizontally)
{
    GraphBuilder b;
    b.StartRow().AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C")).EndRow();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::right), "b");
    EXPECT_EQ(EdgeOf(*r, "b", GraphDir::right), "c");
    EXPECT_EQ(EdgeOf(*r, "b", GraphDir::left), "a");
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::down), "");
}

TEST(GraphA11yBuilder, SharedRowKeysPreserveColumns)
{
    GraphBuilder b;
    b.StartRow("grid").AddItem(Id("a1"), Vt("A1")).AddItem(Id("a2"), Vt("A2")).EndRow();
    b.StartRow("grid").AddItem(Id("b1"), Vt("B1")).AddItem(Id("b2"), Vt("B2")).EndRow();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(EdgeOf(*r, "a2", GraphDir::down), "b2"); // column preserved
    EXPECT_EQ(EdgeOf(*r, "b2", GraphDir::up), "a2");
}

TEST(GraphA11yBuilder, UnkeyedRowsLandOnFirstItem)
{
    GraphBuilder b;
    b.StartRow().AddItem(Id("a1"), Vt("A1")).AddItem(Id("a2"), Vt("A2")).EndRow();
    b.StartRow().AddItem(Id("b1"), Vt("B1")).AddItem(Id("b2"), Vt("B2")).EndRow();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(EdgeOf(*r, "a2", GraphDir::down), "b1"); // no key: first item
}

TEST(GraphA11yBuilder, ArrowsNeverCrossStops)
{
    GraphBuilder b;
    b.AddItem(Id("a"), Vt("A"));
    b.BeginStop("second");
    b.AddItem(Id("b"), Vt("B"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(EdgeOf(*r, "a", GraphDir::down), "");
    EXPECT_EQ(EdgeOf(*r, "b", GraphDir::up), "");
}

TEST(GraphA11yBuilder, DuplicateIdThrows)
{
    GraphBuilder b;
    b.AddItem(Id("a"), Vt("A"));
    EXPECT_THROW(b.AddItem(Id("a"), Vt("A again")), std::logic_error);
}

TEST(GraphA11yBuilder, EmptyBuildReturnsNull)
{
    GraphBuilder b;
    EXPECT_EQ(b.Build(), nullptr);
}

TEST(GraphA11yBuilder, PositionsStampedForVerticalSiblings)
{
    GraphBuilder b;
    b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->NodeAt(Id("b"))->positionIndex, 2);
    EXPECT_EQ(r->NodeAt(Id("b"))->positionCount, 3);
}

TEST(GraphA11yBuilder, MultiItemRowPositionsWithinRow)
{
    GraphBuilder b;
    b.StartRow().AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).EndRow();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->NodeAt(Id("a"))->positionIndex, 1);
    EXPECT_EQ(r->NodeAt(Id("a"))->positionCount, 2);
}

TEST(GraphA11yBuilder, InterleavedRawBreaksChainAndStitches)
{
    GraphBuilder b;
    b.AddItem(Id("m1"), Vt("M1"));
    b.AddNode(Id("raw"), Vt("Raw"));
    b.AddItem(Id("m2"), Vt("M2"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    // The menu chain must not skip over the raw block...
    EXPECT_NE(EdgeOf(*r, "m1", GraphDir::down), "m2");
    // ...and the stitcher wires the seams so nothing is an island.
    EXPECT_EQ(EdgeOf(*r, "m1", GraphDir::down), "raw");
    EXPECT_EQ(EdgeOf(*r, "raw", GraphDir::up), "m1");
    EXPECT_EQ(EdgeOf(*r, "raw", GraphDir::down), "m2");
    EXPECT_EQ(EdgeOf(*r, "m2", GraphDir::up), "raw");
    // Position runs are segmented at the break: neither m1 nor m2 counts the other.
    EXPECT_EQ(r->NodeAt(Id("m1"))->positionCount, 0);
    EXPECT_EQ(r->NodeAt(Id("m2"))->positionCount, 0);
}

// ---- reconciliation ----

TEST(GraphA11yReconcile, Tier2FollowsStructuralKeyAcrossRebuild)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    kg.Move(GraphDir::down);
    EXPECT_EQ(state.curKey.StructuralKey(), "b");
    // Rebuild (new node instances): focus stays on "b" by structural identity.
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "b");
}

TEST(GraphA11yReconcile, Tier1FollowsMovedReference)
{
    static int backing; // opaque token
    GraphState state;
    bool moved = false;
    auto build = [&moved]() {
        GraphBuilder b;
        if (!moved)
            b.AddItem(ControlId::Referenced(&backing, "slot1"), Vt("Item"));
        else
        {
            b.AddItem(Id("other"), Vt("Other"));
            b.AddItem(ControlId::Referenced(&backing, "slot2"), Vt("Item"));
        }
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "slot1");
    moved = true; // the backing object moved to a different structural slot
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "slot2"); // tier 1 followed the object
}

TEST(GraphA11yReconcile, Tier1TieBreakPrefersStructuralAgreement)
{
    static int backing;
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        // Two nodes legitimately share one backing object (a row primary and its cell).
        b.AddItem(ControlId::Referenced(&backing, "row"), Vt("Row"));
        b.AddItem(ControlId::Referenced(&backing, "cell"), Vt("Cell"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    kg.Move(GraphDir::down); // onto "cell"
    EXPECT_EQ(state.curKey.StructuralKey(), "cell");
    // Rebuild: without the tie-break, tier 1 could bounce focus back to "row".
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "cell");
}

TEST(GraphA11yReconcile, NearestSurvivorWhenFocusedVanishes)
{
    GraphState state;
    bool full = true;
    auto build = [&full]() {
        GraphBuilder b;
        b.AddItem(Id("a"), Vt("A"));
        b.AddItem(Id("b"), Vt("B"));
        if (full)
            b.AddItem(Id("c"), Vt("C"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    kg.Move(GraphDir::down);
    kg.Move(GraphDir::down);
    EXPECT_EQ(state.curKey.StructuralKey(), "c");
    full = false; // "c" vanishes
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "b"); // nearest survivor walking backward
}

TEST(GraphA11yReconcile, InitialFocusLandsOnSelectedMember)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.AddItem(Id("a"), VtSelected("A", false));
        b.AddItem(Id("b"), VtSelected("B", true));
        b.AddItem(Id("c"), VtSelected("C", false));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "b"); // the checked item, not the top
}

TEST(GraphA11yReconcile, SuggestedMoveIsAdoptedAndConsumed)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    state.nextSuggestedMove = Id("c");
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(state.curKey.StructuralKey(), "c");
    EXPECT_TRUE(state.nextSuggestedMove.IsEmpty()); // consumed
}

// ---- order ----

TEST(GraphA11yOrder, DownRightOrderThenLaterStops)
{
    GraphBuilder b;
    b.StartRow().AddItem(Id("a1"), Vt("A1")).AddItem(Id("a2"), Vt("A2")).EndRow();
    b.AddItem(Id("b"), Vt("B"));
    b.BeginStop("second");
    b.AddItem(Id("z"), Vt("Z"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    auto order = KeyGraph::ComputeOrder(*r);
    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0].StructuralKey(), "a1");
    EXPECT_EQ(order[1].StructuralKey(), "a2");
    EXPECT_EQ(order[2].StructuralKey(), "b");
    EXPECT_EQ(order[3].StructuralKey(), "z"); // unreachable stop appended, order stays total
}

// ---- movement / stops ----

TEST(GraphA11yMove, MoveAndEdgeSemantics)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    auto r = kg.Move(GraphDir::down);
    EXPECT_TRUE(r.moved);
    EXPECT_EQ(KeyOf(r.to), "b");
    r = kg.MoveToEdge(GraphDir::down);
    EXPECT_TRUE(r.moved);
    EXPECT_EQ(KeyOf(r.to), "c");
    r = kg.Move(GraphDir::down); // at the edge: not moved, to == from
    EXPECT_FALSE(r.moved);
    EXPECT_EQ(r.to, r.from);
}

TEST(GraphA11yMove, StopLandingRemembersPosition)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.BeginStop("one");
        b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B"));
        b.BeginStop("two");
        b.AddItem(Id("x"), Vt("X")).AddItem(Id("y"), Vt("Y"));
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    kg.Move(GraphDir::down); // onto "b" (remembered for stop "one")
    auto r = kg.MoveStop(1, true);
    EXPECT_TRUE(r.moved);
    EXPECT_EQ(KeyOf(r.to), "x");
    kg.Move(GraphDir::down); // onto "y" (remembered for stop "two")
    r = kg.MoveStop(1, true); // wrap back to stop "one"
    EXPECT_EQ(KeyOf(r.to), "b"); // remembered position, not the stop's first node
}

// ---- trees ----

TEST(GraphA11yTree, ExpandDescendCollapse)
{
    GraphState state;
    auto build = [&state]() {
        GraphBuilder b(&state.expanded);
        b.BeginGroup(Id("grp"), Vt("Group"));
        b.AddItem(Id("child1"), Vt("Child 1"));
        b.AddItem(Id("child2"), Vt("Child 2"));
        b.EndGroup();
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    EXPECT_EQ(kg.Current()->nodes.size(), 1u); // collapsed: children suppressed

    auto tr = kg.TreeRight();
    EXPECT_EQ(tr.kind, KeyGraph::TreeMove::expanded);
    EXPECT_EQ(kg.Current()->nodes.size(), 3u);

    tr = kg.TreeRight(); // expanded group: descend
    EXPECT_EQ(tr.kind, KeyGraph::TreeMove::descended);
    EXPECT_EQ(state.curKey.StructuralKey(), "child1");

    tr = kg.TreeLeft(); // child: ascend
    EXPECT_EQ(tr.kind, KeyGraph::TreeMove::ascended);
    EXPECT_EQ(state.curKey.StructuralKey(), "grp");

    tr = kg.TreeLeft(); // expanded group: collapse (focus stays by identity)
    EXPECT_EQ(tr.kind, KeyGraph::TreeMove::collapsed);
    EXPECT_EQ(state.curKey.StructuralKey(), "grp");
    EXPECT_EQ(kg.Current()->nodes.size(), 1u);
}

TEST(GraphA11yTree, EmptyGroupAutoRecollapses)
{
    GraphState state;
    auto build = [&state]() {
        GraphBuilder b(&state.expanded);
        b.BeginGroup(Id("grp"), Vt("Group"));
        b.EndGroup(); // no children materialize
        return b.Build();
    };
    KeyGraph kg(build, &state);
    auto tr = kg.TreeRight();
    EXPECT_EQ(tr.kind, KeyGraph::TreeMove::emptyGroup);
    EXPECT_EQ(state.expanded.count(Id("grp")), 0u); // never left silently expanded
}

TEST(GraphA11yTree, SiblingEdgeFiltersByParentAndStop)
{
    GraphState state;
    auto build = []() {
        GraphBuilder b;
        b.BeginStop("one");
        b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B")).AddItem(Id("c"), Vt("C"));
        b.BeginStop("two");
        b.AddItem(Id("z"), Vt("Z")); // same null parent, DIFFERENT stop
        return b.Build();
    };
    KeyGraph kg(build, &state);
    ASSERT_TRUE(kg.Rerender());
    auto r = kg.MoveToSiblingEdge(false); // End
    EXPECT_TRUE(r.moved);
    // Root-level nodes all share the null parent; without the stop filter this would land on "z".
    EXPECT_EQ(KeyOf(r.to), "c");
}

// ---- announcer ----

TEST(GraphA11yAnnouncer, PathDiffEntersContextOutermostFirst)
{
    GraphBuilder b;
    b.AddItem(Id("outside"), Vt("Outside"));
    b.PushContext("Settings", "list");
    b.AddItem(Id("inner"), Vt("Volume"));
    b.PopContext();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);

    auto* outside = r->NodeAt(Id("outside"));
    auto* inner = r->NodeAt(Id("inner"));
    EXPECT_EQ(GraphAnnouncer::Compose(outside, inner), "Settings, list, Volume");
    // Ascending back out re-announces only the landing control.
    EXPECT_EQ(GraphAnnouncer::Compose(inner, outside), "Outside");
}

TEST(GraphA11yAnnouncer, SiblingMoveSpeaksOnlyTheControl)
{
    GraphBuilder b;
    b.PushContext("Settings");
    b.AddItem(Id("a"), Vt("First"));
    b.AddItem(Id("b"), Vt("Second"));
    b.PopContext();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(GraphAnnouncer::Compose(r->NodeAt(Id("a")), r->NodeAt(Id("b"))), "Second");
}

TEST(GraphA11yAnnouncer, DedupeSkipsLevelDuplicatingNextLabel)
{
    GraphBuilder b;
    b.AddItem(Id("outside"), Vt("Outside"));
    b.PushContext("Game difficulty"); // a section wrapping the same-named control
    b.AddItem(Id("ctl"), Vt("Game difficulty"));
    b.PopContext();
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(GraphAnnouncer::Compose(r->NodeAt(Id("outside")), r->NodeAt(Id("ctl"))), "Game difficulty");
}

TEST(GraphA11yAnnouncer, TypeCommonPartsMergeAndOrder)
{
    ControlType toggleType;
    toggleType.key = "toggle";
    toggleType.order = { AnnouncementKinds::kLabel, AnnouncementKinds::kRole, AnnouncementKinds::kValue };
    toggleType.common = []() {
        std::vector<NodeAnnouncement> parts;
        parts.push_back(NodeAnnouncement::Static("toggle", AnnouncementKinds::kRole));
        return parts;
    };

    GraphBuilder b;
    NodeVtable vt;
    vt.controlType = &toggleType;
    vt.announcements.push_back(NodeAnnouncement::Static("Sound", AnnouncementKinds::kLabel));
    vt.announcements.push_back(NodeAnnouncement::Static("on", AnnouncementKinds::kValue));
    b.AddItem(Id("a"), std::move(vt));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    // The common role word merges in and the type's kind order applies: label, role, value.
    EXPECT_EQ(GraphAnnouncer::LeafText(r->NodeAt(Id("a"))), "Sound, toggle, on");
}

TEST(GraphA11yAnnouncer, AutoPositionAppendsThroughHook)
{
    GraphAnnouncer::PositionText = [](int32_t i, int32_t n) {
        return std::to_string(i) + " of " + std::to_string(n);
    };
    GraphBuilder b;
    b.AddItem(Id("a"), Vt("A")).AddItem(Id("b"), Vt("B"));
    auto r = b.Build();
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(GraphAnnouncer::LeafText(r->NodeAt(Id("b"))), "B, 2 of 2");
    GraphAnnouncer::PositionText = nullptr;
}
