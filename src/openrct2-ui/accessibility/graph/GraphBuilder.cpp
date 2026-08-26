/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GraphBuilder.h"

#include <algorithm>
#include <stdexcept>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    static std::string AutoStopKey(int32_t index)
    {
        return "stop#" + std::to_string(index);
    }

    GraphBuilder::GraphBuilder(const std::unordered_set<ControlId>* expansion)
        : _expansion(expansion)
        , _stopKey(AutoStopKey(0))
    {
    }

    GraphNode* GraphBuilder::CurrentParent() const
    {
        return _parents.empty() ? nullptr : _parents.back().node;
    }

    bool GraphBuilder::Suppressed() const
    {
        return !_parents.empty() && _parents.back().suppressed;
    }

    // ---- stops / regions ----

    GraphBuilder& GraphBuilder::BeginStop(const std::string& key)
    {
        if (_currentRow != nullptr)
            throw std::logic_error("Cannot begin a stop inside an open row");
        _stopKey = key.empty() ? AutoStopKey(_stopAuto) : key;
        _stopAuto++;
        _regionKey.clear(); // regions are per-stop
        return *this;
    }

    GraphBuilder& GraphBuilder::SetRegion(const std::string& key)
    {
        _regionKey = key;
        return *this;
    }

    // ---- the parent stack: contexts + groups ----

    GraphBuilder& GraphBuilder::PushContext(const std::string& label, const std::string& role, bool positions)
    {
        auto* parent = CurrentParent();
        std::vector<NodeAnnouncement> anns;
        anns.push_back(NodeAnnouncement::Static(label));
        if (!role.empty())
            anns.push_back(NodeAnnouncement::Static(role));

        auto node = std::make_unique<GraphNode>();
        // Stable synthetic identity (label-pathed) so cross-render chain diffs match up.
        node->id = ControlId::Structural(
            "ctx:" + (parent != nullptr ? parent->id.StructuralKey() : std::string()) + "/" + label);
        node->vtable.announcements = std::move(anns);
        node->parent = parent;
        node->focusable = false;
        node->suppressChildPositions = !positions;

        _parents.push_back(ParentFrame{ node.get(), Suppressed() });
        _owned.push_back(std::move(node));
        return *this;
    }

    GraphBuilder& GraphBuilder::PopContext()
    {
        if (_parents.empty())
            throw std::logic_error("No context/group to pop");
        _parents.pop_back();
        return *this;
    }

    GraphBuilder& GraphBuilder::BeginGroup(
        const ControlId& id, NodeVtable vtable, std::optional<bool> expanded, bool defaultExpanded)
    {
        if (id.IsEmpty())
            throw std::logic_error("BeginGroup requires a control id");
        if (_currentRow != nullptr)
            throw std::logic_error("Cannot begin a group inside an open row");
        const bool isExpanded = expanded.has_value()
            ? *expanded
            : (_expansion != nullptr ? _expansion->count(id) != 0 : defaultExpanded);

        GraphNode* header = nullptr;
        if (!Suppressed())
        {
            header = MakeNode(id, std::move(vtable));
            header->expandable = true;
            header->expanded = isExpanded;
            auto row = std::make_unique<Row>();
            row->stopKey = _stopKey;
            row->items.push_back(header);
            _rowOf[header] = row.get();
            _rows.push_back(std::move(row));
        }
        _parents.push_back(ParentFrame{
            // Suppressed subtree: keep chaining from the outer parent so the stack stays coherent.
            header != nullptr ? header : CurrentParent(),
            Suppressed() || !isExpanded,
        });
        return *this;
    }

    GraphBuilder& GraphBuilder::EndGroup()
    {
        return PopContext();
    }

    bool GraphBuilder::IsExpanded(const ControlId& id) const
    {
        return _expansion != nullptr && !id.IsEmpty() && _expansion->count(id) != 0;
    }

    GraphBuilder& GraphBuilder::SetStart(const ControlId& id)
    {
        _start = id;
        return *this;
    }

    // ---- menu mode ----

    GraphBuilder& GraphBuilder::StartRow(const std::string& rowKey)
    {
        if (_currentRow != nullptr)
            throw std::logic_error("Cannot start a row while another is open");
        auto row = std::make_unique<Row>();
        row->key = rowKey;
        row->stopKey = _stopKey;
        _currentRow = row.get();
        _rows.push_back(std::move(row));
        return *this;
    }

    GraphBuilder& GraphBuilder::EndRow()
    {
        if (_currentRow == nullptr)
            throw std::logic_error("No row to end");
        if (_currentRow->items.empty() && !Suppressed())
            throw std::logic_error("Row cannot be empty");
        if (_currentRow->items.empty())
        {
            // Suppressed empty row: drop it (it was pre-added when opened).
            _rows.erase(
                std::remove_if(
                    _rows.begin(), _rows.end(), [this](const std::unique_ptr<Row>& r) { return r.get() == _currentRow; }),
                _rows.end());
        }
        _currentRow = nullptr;
        return *this;
    }

    GraphBuilder& GraphBuilder::AddItem(const ControlId& id, NodeVtable vtable)
    {
        if (Suppressed())
            return *this;
        auto* node = MakeNode(id, std::move(vtable));
        if (_currentRow != nullptr)
        {
            _currentRow->items.push_back(node);
            _rowOf[node] = _currentRow;
        }
        else
        {
            auto row = std::make_unique<Row>();
            row->stopKey = _stopKey;
            row->items.push_back(node);
            _rowOf[node] = row.get();
            _rows.push_back(std::move(row));
        }
        return *this;
    }

    GraphBuilder& GraphBuilder::AddLabel(const ControlId& id, std::function<std::string()> label)
    {
        NodeVtable vt;
        vt.announcements.emplace_back(std::move(label));
        return AddItem(id, std::move(vt));
    }

    // ---- raw mode ----

    GraphBuilder& GraphBuilder::AddNode(const ControlId& id, NodeVtable vtable)
    {
        if (Suppressed())
            return *this;
        _rawNodes.push_back(MakeNode(id, std::move(vtable)));
        return *this;
    }

    GraphBuilder& GraphBuilder::Connect(const ControlId& from, GraphDir dir, const ControlId& to, const std::string& label)
    {
        if (from.IsEmpty() || to.IsEmpty())
            throw std::logic_error("Connect requires both endpoints");
        _rawEdges.push_back(RawEdge{ from, dir, to, label });
        return *this;
    }

    GraphNode* GraphBuilder::MakeNode(const ControlId& id, NodeVtable vtable)
    {
        if (id.IsEmpty())
            throw std::logic_error("A control must have an id");
        if (vtable.announcements.empty())
            throw std::logic_error("A control must have at least one announcement: " + id.StructuralKey());
        if (!_ids.insert(id).second)
            throw std::logic_error("Duplicate control id: " + id.StructuralKey());

        auto node = std::make_unique<GraphNode>();
        node->id = id;
        node->vtable = std::move(vtable);
        node->parent = CurrentParent();
        node->stopKey = _stopKey;
        node->regionKey = _regionKey;

        auto* raw = node.get();
        _declared.push_back(raw);
        _owned.push_back(std::move(node));
        return raw;
    }

    // ---- build ----

    std::unique_ptr<GraphRender> GraphBuilder::Build()
    {
        if (_currentRow != nullptr)
            throw std::logic_error("Unclosed row - call EndRow()");
        if (_rawNodes.empty() && std::all_of(_rows.begin(), _rows.end(), [](const auto& r) { return r->items.empty(); }))
            return nullptr;

        auto render = std::make_unique<GraphRender>();
        for (auto* node : _declared)
        {
            render->nodes.emplace(node->id, node);
            render->order.push_back(node);
        }

        WireMenuEdges();
        for (const auto& e : _rawEdges)
        {
            if (render->nodes.count(e.from) != 0 && render->nodes.count(e.to) != 0)
                render->nodes[e.from]->transitions[e.dir] = Transition(e.to, e.label);
        }
        StitchModeBoundaries();

        render->startKey = (!_start.IsEmpty() && render->nodes.count(_start) != 0) ? _start : render->order[0]->id;
        StampPositions();

        // Transfer ownership of every node (incl. pure-structure parents) to the render.
        render->owned = std::move(_owned);
        return render;
    }

    // Where a stop mixes MENU rows with RAW content (filter controls above a grid), the two wiring
    // systems don't see each other, leaving a vertical gap arrows can't cross. Stitch it at each
    // mode boundary (declaration order, same stop). Only MISSING edges are filled - raw content's
    // own wiring is never overridden. The two directions are deliberately asymmetric (spec §4.5).
    void GraphBuilder::StitchModeBoundaries()
    {
        std::vector<std::string> stops;
        std::unordered_map<std::string, std::vector<GraphNode*>> byStop;
        for (auto* n : _declared)
        {
            auto it = byStop.find(n->stopKey);
            if (it == byStop.end())
            {
                byStop.emplace(n->stopKey, std::vector<GraphNode*>{});
                stops.push_back(n->stopKey);
            }
            byStop[n->stopKey].push_back(n);
        }

        for (const auto& stop : stops)
        {
            auto& nodes = byStop[stop];
            for (size_t i = 1; i < nodes.size(); i++)
            {
                auto* prev = nodes[i - 1];
                auto* cur = nodes[i];
                const bool prevMenu = _rowOf.count(prev) != 0;
                const bool curMenu = _rowOf.count(cur) != 0;
                if (prevMenu == curMenu)
                    continue; // same mode - its own wiring covers it

                if (prevMenu) // menu row above raw content: row cells v first raw node without an Up
                {
                    if (cur->transitions.count(GraphDir::up) != 0)
                        continue;
                    auto* row = _rowOf[prev];
                    for (auto* cell : row->items)
                    {
                        if (cell->transitions.count(GraphDir::down) == 0)
                            cell->transitions[GraphDir::down] = Transition(cur->id);
                    }
                    cur->transitions[GraphDir::up] = Transition(row->items[0]->id);
                }
                else // raw content above a menu row: last raw node without a Down wires into the row
                {
                    auto* row = _rowOf[cur];
                    // The raw side's bottom = the latest raw node (walking back) missing a Down.
                    GraphNode* bottom = nullptr;
                    for (size_t j = i; j-- > 0 && _rowOf.count(nodes[j]) == 0;)
                    {
                        if (nodes[j]->transitions.count(GraphDir::down) == 0)
                        {
                            bottom = nodes[j];
                            break;
                        }
                    }
                    if (bottom == nullptr)
                        continue;
                    bottom->transitions[GraphDir::down] = Transition(row->items[0]->id);
                    for (auto* cell : row->items)
                    {
                        if (cell->transitions.count(GraphDir::up) == 0)
                            cell->transitions[GraphDir::up] = Transition(bottom->id);
                    }
                }
            }
        }
    }

    // Auto-stamp "n of m" positions: a multi-item row's members within their ROW; single-item-row
    // nodes among the siblings sharing their (parent, stop) - the vertical list level arrows
    // actually traverse - with sibling runs SEGMENTED at raw-content breaks, mirroring
    // WireMenuEdges: "n of m" may only count nodes the arrows at that list level can reach.
    // (A multi-item row does not break a run - the vertical chain passes through it.)
    void GraphBuilder::StampPositions()
    {
        // Multi-item rows: positioned within their row.
        for (auto& row : _rows)
        {
            if (row->items.size() > 1)
                Stamp(row->items);
        }

        struct OpenRun
        {
            GraphNode* parent;
            std::string stopKey;
            std::vector<GraphNode*> nodes;
        };
        std::vector<std::vector<GraphNode*>> finished;
        std::vector<OpenRun> open;

        for (auto* node : _declared)
        {
            auto rowIt = _rowOf.find(node);
            if (rowIt == _rowOf.end())
            {
                // Raw node: close every open run in its stop.
                for (auto it = open.begin(); it != open.end();)
                {
                    if (it->stopKey == node->stopKey)
                    {
                        finished.push_back(std::move(it->nodes));
                        it = open.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                continue;
            }
            if (rowIt->second->items.size() > 1)
                continue;
            if (node->parent != nullptr && node->parent->suppressChildPositions)
                continue;

            OpenRun* run = nullptr;
            for (auto& r : open)
            {
                if (r.parent == node->parent && r.stopKey == node->stopKey)
                {
                    run = &r;
                    break;
                }
            }
            if (run == nullptr)
            {
                open.push_back(OpenRun{ node->parent, node->stopKey, {} });
                run = &open.back();
            }
            run->nodes.push_back(node);
        }
        for (auto& r : open)
            finished.push_back(std::move(r.nodes));
        for (auto& run : finished)
            Stamp(run);
    }

    void GraphBuilder::Stamp(std::vector<GraphNode*>& siblings)
    {
        if (siblings.size() < 2)
            return;
        for (size_t i = 0; i < siblings.size(); i++)
        {
            siblings[i]->positionIndex = static_cast<int32_t>(i + 1);
            siblings[i]->positionCount = static_cast<int32_t>(siblings.size());
        }
    }

    // Left/right within a row; up/down between consecutive rows OF THE SAME STOP (arrows never
    // cross a Tab-stop). Shared non-empty row keys preserve the column; otherwise vertical lands
    // on the first item. Rows are segmented in DECLARATION order: within a stop, consecutive menu
    // rows chain vertically only when no raw node was declared between them - interleaved raw
    // content BREAKS the chain (StitchModeBoundaries wires the seams; without the break, menu
    // edges would skip straight over the raw block, leaving it an unreachable island).
    void GraphBuilder::WireMenuEdges()
    {
        std::vector<std::vector<Row*>> byStop;
        std::unordered_map<std::string, size_t> openSegment; // stop -> index of its open segment

        for (auto* node : _declared)
        {
            auto rowIt = _rowOf.find(node);
            if (rowIt != _rowOf.end())
            {
                auto* row = rowIt->second;
                auto segIt = openSegment.find(node->stopKey);
                size_t segIdx;
                if (segIt == openSegment.end())
                {
                    byStop.emplace_back();
                    segIdx = byStop.size() - 1;
                    openSegment.emplace(node->stopKey, segIdx);
                }
                else
                {
                    segIdx = segIt->second;
                }
                auto& seg = byStop[segIdx];
                if (seg.empty() || seg.back() != row)
                    seg.push_back(row);
            }
            else
            {
                openSegment.erase(node->stopKey); // raw node: close this stop's segment
            }
        }

        for (auto& rows : byStop)
        {
            for (size_t r = 0; r < rows.size(); r++)
            {
                auto* row = rows[r];
                for (size_t pos = 0; pos < row->items.size(); pos++)
                {
                    auto* node = row->items[pos];
                    if (r > 0)
                        node->transitions[GraphDir::up] = Transition(VerticalTarget(*row, *rows[r - 1], pos));
                    if (r < rows.size() - 1)
                        node->transitions[GraphDir::down] = Transition(VerticalTarget(*row, *rows[r + 1], pos));
                    if (pos > 0)
                        node->transitions[GraphDir::left] = Transition(row->items[pos - 1]->id);
                    if (pos < row->items.size() - 1)
                        node->transitions[GraphDir::right] = Transition(row->items[pos + 1]->id);
                }
            }
        }
    }

    // Where vertical navigation from position `pos` lands in the adjacent row: the same position
    // when the rows share a non-empty key (column nav) and it exists there, else the first item.
    ControlId GraphBuilder::VerticalTarget(const Row& from, const Row& to, size_t pos)
    {
        if (!from.key.empty() && !to.key.empty() && from.key == to.key && pos < to.items.size())
            return to.items[pos]->id;
        return to.items[0]->id;
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph
