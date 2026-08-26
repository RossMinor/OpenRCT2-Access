/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GraphAnnouncer.h"

#include <algorithm>

namespace OpenRCT2::Ui::Accessibility::Graph::GraphAnnouncer
{
    std::function<bool(const ControlType*, const NodeAnnouncement&)> PartFilter;
    std::function<std::string(int32_t, int32_t)> PositionText;
    std::function<std::string(bool)> ExpandedStateText;

    // The node's path: ancestors outermost-first, then the node itself.
    static std::vector<const GraphNode*> PathOf(const GraphNode* node)
    {
        std::vector<const GraphNode*> path;
        for (auto* n = node; n != nullptr; n = n->parent)
            path.push_back(n);
        std::reverse(path.begin(), path.end());
        return path;
    }

    static bool HasKind(const std::vector<NodeAnnouncement>& anns, const std::string& kind)
    {
        if (kind.empty())
            return false;
        for (const auto& a : anns)
        {
            if (a.kind == kind)
                return true;
        }
        return false;
    }

    // Sort key: declared kinds by their order index; everything else after (one shared bucket,
    // with a declaration-index tie-break keeping their relative order).
    static size_t OrderIndex(const std::vector<std::string>& order, const std::string& kind)
    {
        if (!kind.empty())
        {
            for (size_t i = 0; i < order.size(); i++)
            {
                if (order[i] == kind)
                    return i;
            }
        }
        return order.size();
    }

    // Resolve a part's text, guarded: a throwing closure reads as silent (fault isolation - the
    // leaf-compose path runs on every focus change).
    static std::string ResolveText(const NodeAnnouncement& a)
    {
        try
        {
            return a.text ? a.text() : std::string();
        }
        catch (...)
        {
            return {};
        }
    }

    std::vector<NodeAnnouncement> EffectiveAnnouncements(const GraphNode* node)
    {
        std::vector<NodeAnnouncement> result;
        if (node == nullptr)
            return result;
        const auto& vt = node->vtable;
        const auto* type = vt.controlType;

        if (type != nullptr && type->common)
        {
            // Common parts append BEFORE the node's own; a node part of kind K drops every common
            // part of kind K; a common part with an empty kind can never be overridden.
            for (auto& c : type->common())
            {
                if (c.kind.empty() || !HasKind(vt.announcements, c.kind))
                    result.push_back(std::move(c));
            }
        }
        for (const auto& a : vt.announcements)
            result.push_back(a);

        // The kind sort runs only if the type declares an order - stable via (order index,
        // declaration index) composite keys.
        if (type != nullptr && !type->order.empty() && result.size() > 1)
        {
            std::vector<std::pair<uint64_t, NodeAnnouncement>> keyed;
            keyed.reserve(result.size());
            for (size_t i = 0; i < result.size(); i++)
            {
                keyed.emplace_back(
                    (static_cast<uint64_t>(OrderIndex(type->order, result[i].kind)) << 32) | static_cast<uint32_t>(i),
                    std::move(result[i]));
            }
            std::sort(keyed.begin(), keyed.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
            result.clear();
            for (auto& kv : keyed)
                result.push_back(std::move(kv.second));
        }

        if (PartFilter)
        {
            result.erase(
                std::remove_if(
                    result.begin(), result.end(), [type](const NodeAnnouncement& a) { return !PartFilter(type, a); }),
                result.end());
        }
        return result;
    }

    std::string LeafText(const GraphNode* node)
    {
        auto anns = EffectiveAnnouncements(node);
        std::string out;
        for (const auto& a : anns)
        {
            auto t = ResolveText(a);
            if (t.empty())
                continue;
            if (!out.empty())
                out += ", ";
            out += t;
        }

        if (node != nullptr && node->expandable && !node->vtable.speaksOwnExpansion && ExpandedStateText)
        {
            auto state = ExpandedStateText(node->expanded);
            if (!state.empty())
            {
                if (!out.empty())
                    out += ", ";
                out += state;
            }
        }

        // The auto-stamped sibling position, unless the node carries its own. Routed through the
        // part filter as a synthetic probe part of kind `position` so a per-kind toggle governs it.
        if (node != nullptr && node->positionCount > 1 && PositionText && !node->vtable.speaksOwnPosition
            && !HasKind(node->vtable.announcements, AnnouncementKinds::kPosition))
        {
            static const NodeAnnouncement kAutoPositionProbe(
                []() { return std::string(); }, false, AnnouncementKinds::kPosition);
            if (!PartFilter || PartFilter(node->vtable.controlType, kAutoPositionProbe))
            {
                auto pos = PositionText(node->positionIndex, node->positionCount);
                if (!pos.empty())
                {
                    if (!out.empty())
                        out += ", ";
                    out += pos;
                }
            }
        }
        return out;
    }

    std::string FirstPartText(const GraphNode* node)
    {
        if (node == nullptr || node->vtable.announcements.empty())
            return {};
        return ResolveText(node->vtable.announcements.front());
    }

    // The next part "starts as" this label: equal, or its first comma-separated segment is the
    // label (a control's readout leads with its label: "Game difficulty, menu button").
    static bool DuplicatesNext(const std::string& label, const std::string& next)
    {
        if (next.compare(0, label.size(), label) != 0)
            return false;
        return next.size() == label.size() || next[label.size()] == ',';
    }

    std::string Compose(const GraphNode* from, const GraphNode* to, const std::string& transitionLabel)
    {
        if (to == nullptr)
            return {};

        auto toPath = PathOf(to);
        auto fromPath = from != nullptr ? PathOf(from) : std::vector<const GraphNode*>{};

        // Common prefix by identity - levels we were already inside (or ON: descending from a
        // group onto its child keeps the group in the prefix) stay silent.
        size_t i = 0;
        while (i < fromPath.size() && i < toPath.size() && fromPath[i]->id == toPath[i]->id)
            i++;

        std::vector<std::string> parts;
        if (!transitionLabel.empty())
            parts.push_back(transitionLabel);

        if (i >= toPath.size())
        {
            // Ascended (or same node): announce just the now-innermost focus.
            auto text = LeafText(to);
            if (!text.empty())
                parts.push_back(std::move(text));
        }
        else
        {
            for (size_t j = i; j < toPath.size(); j++)
            {
                auto text = LeafText(toPath[j]);
                if (text.empty())
                    continue;
                // Dedupe: a level whose label just duplicates the next level down ("a 'Game
                // difficulty' section wrapping the 'Game difficulty' control"). The comparand is
                // the LABEL - each level's first declared part - on both sides.
                if (j + 1 < toPath.size())
                {
                    auto label = FirstPartText(toPath[j]);
                    auto next = FirstPartText(toPath[j + 1]);
                    if (!label.empty() && !next.empty() && DuplicatesNext(label, next))
                        continue;
                }
                parts.push_back(std::move(text));
            }
        }

        std::string out;
        for (const auto& p : parts)
        {
            if (!out.empty())
                out += ", ";
            out += p;
        }
        return out;
    }

    std::string ComposeFull(const GraphNode* to)
    {
        return Compose(nullptr, to);
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph::GraphAnnouncer
