/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "TrackDesignDescription.h"

#include "RideVisualDescriptions.h"

#include <openrct2/drawing/Colour.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/TrackDesign.h>
#include <openrct2/ride/ted/PitchAndRoll.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/ride/ted/TrackGroup.h>
#include <string>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::TrackMetadata;
using OpenRCT2::Drawing::Colour;

namespace OpenRCT2::Ui::Accessibility
{
    // Natural-language name for a palette colour. Falls back to the engine's identifier string
    // (with underscores turned into spaces) for anything not spelled out here.
    static std::string ColourName(Colour c)
    {
        switch (c)
        {
            case Colour::black: return "black";
            case Colour::grey: return "grey";
            case Colour::white: return "white";
            case Colour::darkPurple: return "dark purple";
            case Colour::lightPurple: return "light purple";
            case Colour::brightPurple: return "bright purple";
            case Colour::darkBlue: return "dark blue";
            case Colour::lightBlue: return "light blue";
            case Colour::icyBlue: return "icy blue";
            case Colour::darkWater: return "dark teal";
            case Colour::lightWater: return "teal";
            case Colour::saturatedGreen: return "green";
            case Colour::darkGreen: return "dark green";
            case Colour::mossGreen: return "moss green";
            case Colour::brightGreen: return "bright green";
            case Colour::oliveGreen: return "olive green";
            case Colour::darkOliveGreen: return "dark olive green";
            case Colour::brightYellow: return "bright yellow";
            case Colour::yellow: return "yellow";
            case Colour::darkYellow: return "dark yellow";
            case Colour::lightOrange: return "light orange";
            case Colour::darkOrange: return "dark orange";
            case Colour::lightBrown: return "light brown";
            case Colour::saturatedBrown: return "brown";
            case Colour::darkBrown: return "dark brown";
            case Colour::salmonPink: return "salmon pink";
            case Colour::bordeauxRed: return "bordeaux red";
            case Colour::saturatedRed: return "red";
            case Colour::brightRed: return "bright red";
            case Colour::darkPink: return "dark pink";
            case Colour::brightPink: return "bright pink";
            case Colour::lightPink: return "light pink";
            case Colour::armyGreen: return "army green";
            case Colour::honeyDew: return "honeydew";
            case Colour::tan: return "tan";
            case Colour::maroon: return "maroon";
            case Colour::coralPink: return "coral pink";
            case Colour::forestGreen: return "forest green";
            case Colour::chartreuse: return "chartreuse";
            case Colour::hunterGreen: return "hunter green";
            case Colour::celadon: return "celadon";
            case Colour::limeGreen: return "lime green";
            case Colour::sepia: return "sepia";
            case Colour::peach: return "peach";
            case Colour::periwinkle: return "periwinkle";
            case Colour::viridian: return "viridian";
            case Colour::seafoamGreen: return "seafoam green";
            case Colour::violet: return "violet";
            case Colour::lavender: return "lavender";
            case Colour::pastelOrange: return "pastel orange";
            case Colour::deepWater: return "deep blue";
            case Colour::pastelPink: return "pastel pink";
            case Colour::umber: return "umber";
            case Colour::beige: return "beige";
            default:
            {
                std::string s = Drawing::colourToString(c);
                for (auto& ch : s)
                    if (ch == '_')
                        ch = ' ';
                return s;
            }
        }
    }

    // "a hill" / "a pair of hills" / "several hills" / "many hills".
    static std::string Quantify(int n, const std::string& singular, const std::string& plural)
    {
        if (n <= 1)
            return "a " + singular;
        if (n == 2)
            return "a pair of " + plural;
        if (n <= 4)
            return "several " + plural;
        return "many " + plural;
    }

    // Joins fragments as "a, b and c".
    static std::string JoinList(const std::vector<std::string>& items)
    {
        std::string result;
        for (size_t i = 0; i < items.size(); i++)
        {
            if (i == 0)
                result = items[i];
            else if (i + 1 == items.size())
                result += " and " + items[i];
            else
                result += ", " + items[i];
        }
        return result;
    }

    namespace
    {
        struct LayoutAnalysis
        {
            int verySharpTurns = 0, sharpTurns = 0, mediumTurns = 0, sweepingTurns = 0, sBends = 0;
            bool banked = false;
            bool gentleHills = false, steepHills = false, verticalDrop = false;
            int helixes = 0;
            int loops = 0, corkscrews = 0, barrelRolls = 0, halfLoops = 0, inlineTwists = 0;
            int zeroGRolls = 0, diveLoops = 0, heartlineRolls = 0, quarterLoops = 0;
            bool chainLift = false, waterSplash = false, onridePhoto = false, booster = false;
            int totalInversions() const
            {
                return loops + corkscrews + barrelRolls + halfLoops + inlineTwists + zeroGRolls + diveLoops
                    + heartlineRolls + quarterLoops;
            }
        };
    } // namespace

    static LayoutAnalysis AnalyseLayout(const TrackDesign& design)
    {
        LayoutAnalysis a;
        for (const auto& el : design.trackElements)
        {
            if (el.flags.has(TrackDesignTrackElementFlag::hasChain))
                a.chainLift = true;

            const auto& ted = GetTrackElementDescriptor(el.type);
            const auto pitchStart = TrackPitchAndRollStart(el.type).pitch;
            const auto pitchEnd = TrackPitchAndRollEnd(el.type).pitch;
            const auto roll = TrackPitchAndRollEnd(el.type).roll;
            if (roll == TrackRoll::left || roll == TrackRoll::right)
                a.banked = true;

            auto classifyPitch = [&](TrackPitch p) {
                switch (p)
                {
                    case TrackPitch::up25:
                    case TrackPitch::down25:
                        a.gentleHills = true;
                        break;
                    case TrackPitch::up60:
                    case TrackPitch::down60:
                        a.steepHills = true;
                        break;
                    case TrackPitch::up90:
                    case TrackPitch::down90:
                        a.verticalDrop = true;
                        break;
                    default:
                        break;
                }
            };
            classifyPitch(pitchStart);
            classifyPitch(pitchEnd);

            switch (ted.definition.group)
            {
                case TrackGroup::curveVerySmall:
                    a.verySharpTurns++;
                    break;
                case TrackGroup::curveSmall:
                    a.sharpTurns++;
                    break;
                case TrackGroup::curve:
                case TrackGroup::slopeCurve:
                case TrackGroup::slopeCurveSteep:
                case TrackGroup::slopeCurveBanked:
                    a.mediumTurns++;
                    break;
                case TrackGroup::curveLarge:
                case TrackGroup::slopeCurveLarge:
                case TrackGroup::slopeCurveLargeBanked:
                    a.sweepingTurns++;
                    break;
                case TrackGroup::sBend:
                    a.sBends++;
                    break;
                case TrackGroup::helixUpBankedHalf:
                case TrackGroup::helixDownBankedHalf:
                case TrackGroup::helixUpBankedQuarter:
                case TrackGroup::helixDownBankedQuarter:
                case TrackGroup::helixUpUnbankedQuarter:
                case TrackGroup::helixDownUnbankedQuarter:
                    a.helixes++;
                    break;
                case TrackGroup::verticalLoop:
                    a.loops++;
                    break;
                case TrackGroup::halfLoop:
                case TrackGroup::halfLoopLarge:
                case TrackGroup::halfLoopMedium:
                case TrackGroup::flyingHalfLoopUninvertedUp:
                case TrackGroup::flyingHalfLoopInvertedDown:
                case TrackGroup::flyingLargeHalfLoopUninvertedUp:
                case TrackGroup::flyingLargeHalfLoopInvertedDown:
                case TrackGroup::flyingLargeHalfLoopUninvertedDown:
                case TrackGroup::flyingLargeHalfLoopInvertedUp:
                case TrackGroup::flyingHalfLoopUninvertedDown:
                case TrackGroup::flyingHalfLoopInvertedUp:
                    a.halfLoops++;
                    break;
                case TrackGroup::corkscrew:
                case TrackGroup::corkscrewUninverted:
                case TrackGroup::corkscrewInverted:
                case TrackGroup::corkscrewLarge:
                    a.corkscrews++;
                    break;
                case TrackGroup::barrelRoll:
                    a.barrelRolls++;
                    break;
                case TrackGroup::heartlineRoll:
                    a.heartlineRolls++;
                    break;
                case TrackGroup::inlineTwistUninverted:
                case TrackGroup::inlineTwistInverted:
                    a.inlineTwists++;
                    break;
                case TrackGroup::zeroGRoll:
                case TrackGroup::zeroGRollLarge:
                    a.zeroGRolls++;
                    break;
                case TrackGroup::quarterLoopUninvertedUp:
                case TrackGroup::quarterLoopUninvertedDown:
                case TrackGroup::quarterLoopInvertedUp:
                case TrackGroup::quarterLoopInvertedDown:
                    a.quarterLoops++;
                    break;
                case TrackGroup::diveLoop:
                    a.diveLoops++;
                    break;
                case TrackGroup::waterSplash:
                    a.waterSplash = true;
                    break;
                case TrackGroup::onridePhoto:
                    a.onridePhoto = true;
                    break;
                case TrackGroup::booster:
                case TrackGroup::diagBooster:
                    a.booster = true;
                    break;
                default:
                    break;
            }
        }
        return a;
    }

    // Composes the turn fragments, sharpest first.
    static std::vector<std::string> TurnFragments(const LayoutAnalysis& a)
    {
        std::vector<std::string> turns;
        if (a.verySharpTurns > 0)
            turns.push_back(Quantify(a.verySharpTurns, "hairpin turn", "hairpin turns"));
        if (a.sharpTurns > 0)
            turns.push_back(Quantify(a.sharpTurns, "sharp turn", "sharp turns"));
        if (a.mediumTurns > 0)
            turns.push_back(Quantify(a.mediumTurns, "turn", "turns"));
        if (a.sweepingTurns > 0)
            turns.push_back(Quantify(a.sweepingTurns, a.banked ? "sweeping banked curve" : "sweeping curve",
                                     a.banked ? "sweeping banked curves" : "sweeping curves"));
        if (a.sBends > 0)
            turns.push_back(Quantify(a.sBends, "S-bend", "S-bends"));
        return turns;
    }

    static std::vector<std::string> InversionFragments(const LayoutAnalysis& a)
    {
        std::vector<std::string> inv;
        if (a.loops > 0)
            inv.push_back(Quantify(a.loops, "vertical loop", "vertical loops"));
        if (a.halfLoops > 0)
            inv.push_back(Quantify(a.halfLoops, "half loop", "half loops"));
        if (a.corkscrews > 0)
            inv.push_back(Quantify(a.corkscrews, "corkscrew", "corkscrews"));
        if (a.barrelRolls > 0)
            inv.push_back(Quantify(a.barrelRolls, "barrel roll", "barrel rolls"));
        if (a.zeroGRolls > 0)
            inv.push_back(Quantify(a.zeroGRolls, "zero-G roll", "zero-G rolls"));
        if (a.inlineTwists > 0)
            inv.push_back(Quantify(a.inlineTwists, "in-line twist", "in-line twists"));
        if (a.heartlineRolls > 0)
            inv.push_back(Quantify(a.heartlineRolls, "heartline roll", "heartline rolls"));
        if (a.diveLoops > 0)
            inv.push_back(Quantify(a.diveLoops, "dive loop", "dive loops"));
        if (a.quarterLoops > 0)
            inv.push_back(Quantify(a.quarterLoops, "quarter loop", "quarter loops"));
        return inv;
    }

    static std::string HillPhrase(const LayoutAnalysis& a)
    {
        if (a.verticalDrop)
            return "a near-vertical drop";
        if (a.steepHills)
            return "steep drops and climbs";
        if (a.gentleHills)
            return "gentle rises and dips";
        return "";
    }

    static std::string ColourPhrase(const TrackDesign& design)
    {
        const auto& tc = design.appearance.trackColours[0];
        const std::string main = ColourName(tc.main);
        const std::string supports = ColourName(tc.supports);

        std::string phrase = "The track is " + main;
        if (supports != main)
            phrase += " on " + supports + " supports";

        if (!design.appearance.vehicleColours.empty())
        {
            const std::string body = ColourName(design.appearance.vehicleColours[0].Body);
            if (body != main)
                phrase += ", with " + body + " cars";
        }
        phrase += ".";
        return phrase;
    }

    std::string DescribeTrackDesign(const TrackDesign& design)
    {
        const ride_type_t rideType = design.trackAndVehicle.rtdIndex;
        const auto& rtd = GetRideTypeDescriptor(rideType);

        // Mazes and mini-golf have no track layout to read; give them a fitting fixed line.
        if (rtd.specialType == RtdSpecialType::maze)
            return "A maze of winding passages and dead ends.";
        if (rtd.specialType == RtdSpecialType::miniGolf)
            return "A landscaped miniature golf course of putting greens dotted with obstacles.";

        const LayoutAnalysis a = AnalyseLayout(design);
        const std::vector<std::string> turns = TurnFragments(a);
        const std::vector<std::string> inversions = InversionFragments(a);
        const std::string hills = HillPhrase(a);

        // If there is nothing distinctive to say (e.g. a tiny powered ride), fall back to the
        // generic per-type description rather than an empty or bland line.
        if (turns.empty() && inversions.empty() && hills.empty() && a.helixes == 0)
            return GetRideVisualDescription(rideType);

        // Pick a lead that captures the overall character, so the descriptions don't all open the
        // same way.
        const int inv = a.totalInversions();
        const int sharp = a.verySharpTurns + a.sharpTurns;
        const int flowing = a.sweepingTurns + a.mediumTurns;
        std::string lead;
        if (inv >= 3)
            lead = "An intense, inversion-packed layout";
        else if (sharp > 0 && sharp >= flowing)
            lead = "A tight, twisting layout";
        else if (a.verticalDrop || a.steepHills)
            lead = "A steep, plunging layout";
        else if (flowing > 0)
            lead = "A smooth, flowing layout";
        else
            lead = "A gentle layout";

        std::string text = lead;

        // First sentence: the shape - turns and hills.
        if (!turns.empty())
        {
            text += " of " + JoinList(turns);
            if (!hills.empty())
                text += ", with " + hills;
        }
        else if (!hills.empty())
        {
            text += " with " + hills;
        }
        text += ".";

        // Helixes read naturally alongside the inversions/feature sentence.
        std::vector<std::string> features = inversions;
        if (a.helixes > 0)
            features.push_back(Quantify(a.helixes, "helix", "helices"));
        if (!features.empty())
            text += " It features " + JoinList(features) + ".";

        // Notable extras.
        std::vector<std::string> extras;
        if (a.chainLift)
            extras.push_back("a chain-lift hill");
        if (a.booster)
            extras.push_back("a launch booster");
        if (a.waterSplash)
            extras.push_back("a water splash");
        if (a.onridePhoto)
            extras.push_back("an on-ride photo section");
        if (!extras.empty())
            text += " Along the way there is " + JoinList(extras) + ".";

        // Colour scheme last.
        text += " " + ColourPhrase(design);
        return text;
    }
} // namespace OpenRCT2::Ui::Accessibility
