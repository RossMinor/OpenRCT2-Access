/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <functional>
#include <string>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    // The identity of a control (graph node) - a two-tier identity so focus can be followed across
    // rebuilds even when the world shifts under us. Transcribed from the Graph A11y Kernel reference
    // implementation (RTAccess), lineage Factorio Access -> Tanglebeep -> WrathAccess -> RTAccess.
    //
    // `reference` (optional) is the domain object a node was derived from (a ride, a save slot, a
    // config field), compared by POINTER IDENTITY ONLY. In this non-GC port it is an opaque
    // comparison token - NEVER a resource, NEVER dereferenced: the id stored in the cursor outlives
    // the render, and may be compared against a freed-and-reallocated address. That rare false
    // tier-1 hit is acceptable because the structural key stays authoritative and reconciliation's
    // tie-break prefers structural agreement.
    //
    // `structuralKey` (always present) is a value-equatable key. Two controls are "the same" when
    // their references are identical (tier 1 - a perfect match that follows an object that MOVED,
    // its structural key changing) OR their structural keys are equal (tier 2 - follows a logical
    // control whose backing object was rebuilt: new instance, same identity).
    //
    // Equality/hashing is defined on `structuralKey` ALONE, so a ControlId is a stable map key.
    // The reference tier is metadata, applied explicitly during focus reconciliation.
    //
    // Structural keys must be stable across rebuilds for as long as the control logically exists.
    // Index-based keys are a last resort: under reordering, tier-2 recovery silently teleports
    // focus and the frame differ re-announces a control the user never left. Prefer persistent
    // content ids ("ride:" + id).
    class ControlId
    {
    public:
        ControlId() = default;

        // A control identified only by a structural key (no backing object).
        static ControlId Structural(std::string structuralKey)
        {
            ControlId id;
            id._structuralKey = std::move(structuralKey);
            return id;
        }

        // A control with both tiers: a backing object (opaque token) and a structural key.
        static ControlId Referenced(const void* reference, std::string structuralKey)
        {
            ControlId id;
            id._reference = reference;
            id._structuralKey = std::move(structuralKey);
            return id;
        }

        bool IsEmpty() const
        {
            return _structuralKey.empty();
        }

        const void* Reference() const
        {
            return _reference;
        }

        const std::string& StructuralKey() const
        {
            return _structuralKey;
        }

        // Tier-1 test: is `obj` this control's backing object?
        bool ReferenceMatches(const void* obj) const
        {
            return _reference != nullptr && _reference == obj;
        }

        // Equality on the structural key alone (the reference tier is reconciliation metadata).
        bool operator==(const ControlId& other) const
        {
            return _structuralKey == other._structuralKey;
        }
        bool operator!=(const ControlId& other) const
        {
            return !(*this == other);
        }

    private:
        const void* _reference = nullptr; // opaque comparison token - never dereferenced
        std::string _structuralKey;       // empty = "no id" (an unset/default ControlId)
    };
} // namespace OpenRCT2::Ui::Accessibility::Graph

template<>
struct std::hash<OpenRCT2::Ui::Accessibility::Graph::ControlId>
{
    size_t operator()(const OpenRCT2::Ui::Accessibility::Graph::ControlId& id) const noexcept
    {
        return std::hash<std::string>{}(id.StructuralKey());
    }
};
