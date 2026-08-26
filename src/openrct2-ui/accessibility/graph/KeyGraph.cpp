/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "KeyGraph.h"

#include <algorithm>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    bool KeyGraph::Rerender()
    {
        _current = _renderCallback();
        if (_current == nullptr || _current->nodes.empty())
        {
            _current = nullptr;
            return false;
        }
        Reconcile(*_current, *_state);
        return true;
    }

    void KeyGraph::Reconcile(const GraphRender& render, GraphState& state)
    {
        // Honor a pending suggested move first, if its target still exists (consumed either way).
        if (!state.nextSuggestedMove.IsEmpty())
        {
            auto it = render.nodes.find(state.nextSuggestedMove);
            if (it != render.nodes.end())
                state.curKey = it->second->id;
            state.nextSuggestedMove = ControlId();
        }

        const ControlId old = state.curKey;
        ControlId resolved;

        if (!old.IsEmpty())
        {
            // Tier 1: the same backing object, even if its structural key changed (it moved).
            // Several nodes can legitimately share one backing object (a row primary and its
            // cells): prefer the candidate whose structural key ALSO matches, else the first in
            // declaration order - a hash-order pick pins focus to an arbitrary sharer and every
            // move away bounces back.
            if (old.Reference() != nullptr)
            {
                GraphNode* byRef = nullptr;
                for (auto* n : render.order)
                {
                    if (!n->id.ReferenceMatches(old.Reference()))
                        continue;
                    if (n->id == old)
                    {
                        byRef = n;
                        break;
                    }
                    if (byRef == nullptr)
                        byRef = n;
                }
                if (byRef != nullptr)
                    resolved = byRef->id;
            }

            // Tier 2: the same structural key, even if the backing object was rebuilt.
            if (resolved.IsEmpty())
            {
                auto it = render.nodes.find(old);
                if (it != render.nodes.end())
                    resolved = it->second->id;
            }

            // Fallback: nearest survivor walking the previous order backward.
            if (resolved.IsEmpty() && !state.keyOrder.empty())
            {
                auto oldIt = std::find(state.keyOrder.begin(), state.keyOrder.end(), old);
                if (oldIt != state.keyOrder.end())
                {
                    for (auto i = static_cast<ptrdiff_t>(oldIt - state.keyOrder.begin()); i >= 0; i--)
                    {
                        auto it = render.nodes.find(state.keyOrder[static_cast<size_t>(i)]);
                        if (it != render.nodes.end())
                        {
                            resolved = it->second->id;
                            break;
                        }
                    }
                }
            }
        }

        // Nothing matched (or first render): the start node - but prefer the SELECTED member of
        // its stop (initial focus lands on the checked radio/current tab, not the top of a list).
        if (resolved.IsEmpty())
        {
            GraphNode* startNode = render.NodeAt(render.startKey);
            GraphNode* sel = startNode != nullptr ? SelectedNodeInStop(render, startNode->stopKey) : nullptr;
            if (sel != nullptr)
                resolved = sel->id;
            else if (startNode != nullptr)
                resolved = startNode->id;
            else
                resolved = render.startKey;
        }

        state.curKey = resolved;
        RememberStop(render, state, resolved);
        state.keyOrder = ComputeOrder(render);
    }

    std::vector<ControlId> KeyGraph::ComputeOrder(const GraphRender& render)
    {
        std::vector<ControlId> order;
        std::unordered_set<ControlId> seen;
        std::vector<ControlId> downFringe{ render.startKey };

        size_t i = 0;
        while (i < downFringe.size())
        {
            ControlId k = downFringe[i];
            while (seen.count(k) == 0)
            {
                seen.insert(k);
                order.push_back(k);

                auto it = render.nodes.find(k);
                if (it == render.nodes.end())
                    break;
                auto* n = it->second;

                auto d = n->transitions.find(GraphDir::down);
                if (d != n->transitions.end())
                    downFringe.push_back(d->second.destination);
                auto r = n->transitions.find(GraphDir::right);
                if (r == n->transitions.end())
                    break;
                k = r->second.destination;
            }
            i++;
        }

        for (auto* node : render.order)
        {
            if (seen.insert(node->id).second)
                order.push_back(node->id);
        }
        return order;
    }

    void KeyGraph::RememberStop(const GraphRender& render, GraphState& state, const ControlId& key)
    {
        auto* node = render.NodeAt(key);
        if (node != nullptr && !node->stopKey.empty())
            state.stopMemory[node->stopKey] = key;
    }

    void KeyGraph::SetCurrent(GraphNode* node)
    {
        _state->curKey = node->id;
        if (!node->stopKey.empty())
            _state->stopMemory[node->stopKey] = node->id;
    }

    // ---- navigation operations ----

    MoveResult KeyGraph::Move(GraphDir dir)
    {
        MoveResult result;
        if (!Rerender())
            return result;

        auto* node = CurrentNode();
        result.from = node;
        result.to = node;
        if (node == nullptr)
            return result;

        auto t = node->transitions.find(dir);
        GraphNode* dest = t != node->transitions.end() ? _current->NodeAt(t->second.destination) : nullptr;
        if (dest == nullptr || dest == node)
            return result;

        SetCurrent(dest);
        result.to = dest;
        result.moved = true;
        result.transitionLabel = t->second.label;
        return result;
    }

    MoveResult KeyGraph::MoveToEdge(GraphDir dir)
    {
        MoveResult result;
        if (!Rerender())
            return result;

        auto* node = CurrentNode();
        result.from = node;
        result.to = node;
        if (node == nullptr)
            return result;

        auto* cur = node;
        while (true)
        {
            auto t = cur->transitions.find(dir);
            if (t == cur->transitions.end())
                break;
            auto* next = _current->NodeAt(t->second.destination);
            if (next == nullptr || next == cur)
                break;
            cur = next;
        }

        if (cur != node)
        {
            SetCurrent(cur);
            result.to = cur;
            result.moved = true;
        }
        return result;
    }

    MoveResult KeyGraph::MoveStop(int32_t dir, bool wrap)
    {
        MoveResult result;
        if (!Rerender())
            return result;

        auto* node = CurrentNode();
        result.from = node;
        result.to = node;
        if (node == nullptr)
            return result;

        auto stops = StopOrder();
        if (stops.size() <= 1)
            return result;

        auto it = std::find(stops.begin(), stops.end(), node->stopKey);
        if (it == stops.end())
            return result;
        auto idx = static_cast<int32_t>(it - stops.begin());
        int32_t ni = idx + dir;
        const auto count = static_cast<int32_t>(stops.size());
        if (wrap)
            ni = ((ni % count) + count) % count;
        if (ni < 0 || ni >= count || ni == idx)
            return result;

        auto* dest = StopLanding(stops[static_cast<size_t>(ni)]);
        if (dest == nullptr)
            return result;

        SetCurrent(dest);
        result.to = dest;
        result.moved = true;
        return result;
    }

    MoveResult KeyGraph::MoveRegion(int32_t dir)
    {
        MoveResult result;
        if (!Rerender())
            return result;

        auto* node = CurrentNode();
        result.from = node;
        result.to = node;
        if (node == nullptr || node->regionKey.empty())
            return result;

        std::vector<std::string> regions;
        for (auto* n : _current->order)
        {
            if (n->stopKey == node->stopKey && !n->regionKey.empty()
                && std::find(regions.begin(), regions.end(), n->regionKey) == regions.end())
                regions.push_back(n->regionKey);
        }

        auto it = std::find(regions.begin(), regions.end(), node->regionKey);
        if (it == regions.end())
            return result;
        auto idx = static_cast<int32_t>(it - regions.begin());
        const int32_t ni = idx + dir;
        if (ni < 0 || ni >= static_cast<int32_t>(regions.size()))
            return result;

        for (auto* n : _current->order)
        {
            if (n->stopKey == node->stopKey && n->regionKey == regions[static_cast<size_t>(ni)])
            {
                SetCurrent(n);
                result.to = n;
                result.moved = true;
                return result;
            }
        }
        return result;
    }

    bool KeyGraph::Focus(const ControlId& id)
    {
        if (id.IsEmpty() || !Rerender())
            return false;
        auto* node = _current->NodeAt(id);
        if (node == nullptr)
            return false;
        SetCurrent(node);
        return true;
    }

    bool KeyGraph::FocusByReference(const void* reference)
    {
        if (reference == nullptr || !Rerender())
            return false;
        for (auto* n : _current->order)
        {
            if (n->id.ReferenceMatches(reference))
            {
                const bool changed = _state->curKey.IsEmpty() || !(_state->curKey == n->id);
                SetCurrent(n);
                return changed;
            }
        }
        return false;
    }

    std::vector<std::string> KeyGraph::StopOrder() const
    {
        std::vector<std::string> stops;
        for (auto* n : _current->order)
        {
            if (!n->stopKey.empty() && std::find(stops.begin(), stops.end(), n->stopKey) == stops.end())
                stops.push_back(n->stopKey);
        }
        return stops;
    }

    GraphNode* KeyGraph::StopLanding(const std::string& stopKey)
    {
        return StopLanding(*_current, *_state, stopKey);
    }

    GraphNode* KeyGraph::StopLanding(const GraphRender& render, const GraphState& state, const std::string& stopKey)
    {
        auto remembered = state.stopMemory.find(stopKey);
        if (remembered != state.stopMemory.end())
        {
            auto* node = render.NodeAt(remembered->second);
            if (node != nullptr && node->stopKey == stopKey)
                return node;
        }
        auto* selected = SelectedNodeInStop(render, stopKey);
        if (selected != nullptr)
            return selected;
        for (auto* n : render.order)
        {
            if (n->stopKey == stopKey)
                return n;
        }
        return nullptr;
    }

    GraphNode* KeyGraph::SelectedNodeInStop(const GraphRender& render, const std::string& stopKey)
    {
        for (auto* n : render.order)
        {
            if (n->stopKey != stopKey)
                continue;
            for (const auto& a : n->vtable.announcements)
            {
                if (a.kind != AnnouncementKinds::kSelected)
                    continue;
                // The probe resolves a closure over live application state from inside the
                // kernel; a throwing part reads as not-selected (keep this guard).
                std::string t;
                try
                {
                    if (a.text)
                        t = a.text();
                }
                catch (...)
                {
                }
                if (!t.empty())
                    return n;
            }
        }
        return nullptr;
    }

    // ---- tree operations ----

    bool KeyGraph::InTree(const GraphNode* node)
    {
        for (auto* n = node; n != nullptr; n = n->parent)
        {
            if (n->expandable)
                return true;
        }
        return false;
    }

    KeyGraph::TreeResult KeyGraph::TreeRight()
    {
        TreeResult result;
        if (!Rerender())
            return result;
        auto* node = CurrentNode();
        if (node == nullptr)
            return result;

        if (node->expandable && !node->expanded)
        {
            // Copy the id BEFORE the rebuild: `node` points into the current render, and the
            // Rerender below frees it (in the GC reference the old node survives; here it can't).
            const ControlId nodeId = node->id;
            SetExpanded(node, true);
            if (!Rerender())
                return result;
            auto* header = _current->NodeAt(nodeId);
            if (header == nullptr)
                return result;
            if (FirstChildOf(header) == nullptr)
            {
                // A lazy drill-in that resolved to nothing: don't leave a silent empty-expanded
                // node.
                SetExpanded(header, false);
                Rerender();
                result.kind = TreeMove::emptyGroup;
                return result;
            }
            result.kind = TreeMove::expanded;
            return result;
        }

        if (node->expandable && node->expanded)
        {
            auto* child = FirstChildOf(node);
            if (child == nullptr)
            {
                result.kind = TreeMove::leaf;
                return result;
            }
            result.move.from = node;
            SetCurrent(child);
            result.move.to = child;
            result.move.moved = true;
            result.kind = TreeMove::descended;
            return result;
        }

        result.kind = InTree(node) ? TreeMove::leaf : TreeMove::none;
        return result;
    }

    KeyGraph::TreeResult KeyGraph::TreeLeft()
    {
        TreeResult result;
        if (!Rerender())
            return result;
        auto* node = CurrentNode();
        if (node == nullptr)
            return result;

        if (node->expandable && node->expanded)
        {
            SetExpanded(node, false);
            Rerender(); // focus stays on the header by identity
            result.kind = TreeMove::collapsed;
            return result;
        }

        for (auto* p = node->parent; p != nullptr; p = p->parent)
        {
            if (!p->focusable || _current->nodes.count(p->id) == 0)
                continue;
            result.move.from = node;
            auto* target = _current->NodeAt(p->id);
            SetCurrent(target);
            result.move.to = target;
            result.move.moved = true;
            result.kind = TreeMove::ascended;
            return result;
        }

        result.kind = InTree(node) ? TreeMove::leaf : TreeMove::none;
        return result;
    }

    MoveResult KeyGraph::MoveToSiblingEdge(bool first)
    {
        MoveResult result;
        if (!Rerender())
            return result;
        auto* node = CurrentNode();
        result.from = node;
        result.to = node;
        if (node == nullptr)
            return result;

        GraphNode* target = nullptr;
        for (auto* n : _current->order)
        {
            if (n->parent != node->parent)
                continue;
            // Root-level nodes all share the null parent - without the stop filter, Home/End on
            // a top-level node would scan every stop (arrows never cross a stop; neither may this).
            if (n->stopKey != node->stopKey)
                continue;
            if (first)
            {
                target = n;
                break;
            }
            target = n; // last match wins
        }
        if (target == nullptr || target == node)
            return result;
        SetCurrent(target);
        result.to = target;
        result.moved = true;
        return result;
    }

    void KeyGraph::SetExpanded(GraphNode* group, bool expanded)
    {
        if (expanded && group->vtable.onExpand)
        {
            group->vtable.onExpand();
            return;
        }
        if (!expanded && group->vtable.onCollapse)
        {
            group->vtable.onCollapse();
            return;
        }
        if (expanded)
            _state->expanded.insert(group->id);
        else
            _state->expanded.erase(group->id);
    }

    GraphNode* KeyGraph::FirstChildOf(const GraphNode* group)
    {
        for (auto* n : _current->order)
        {
            if (n->parent == group)
                return n;
        }
        return nullptr;
    }

    // ---- behavior invokers ----

    bool KeyGraph::Activate()
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onActivate)
            return false;
        node->vtable.onActivate();
        return true;
    }

    bool KeyGraph::Secondary()
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onSecondary)
            return false;
        node->vtable.onSecondary();
        return true;
    }

    bool KeyGraph::ActivateShift()
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onActivateShift)
            return false;
        node->vtable.onActivateShift();
        return true;
    }

    bool KeyGraph::ActivateCtrl()
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onActivateCtrl)
            return false;
        node->vtable.onActivateCtrl();
        return true;
    }

    bool KeyGraph::Tooltip()
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onTooltip)
            return false;
        node->vtable.onTooltip();
        return true;
    }

    bool KeyGraph::TryAdjust(int32_t sign, bool large)
    {
        if (!Rerender())
            return false;
        auto* node = CurrentNode();
        if (node == nullptr || !node->vtable.onAdjust)
            return false;
        node->vtable.onAdjust(sign, large);
        return true;
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph
