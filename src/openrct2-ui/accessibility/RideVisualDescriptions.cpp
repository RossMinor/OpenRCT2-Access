/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "RideVisualDescriptions.h"

#include <openrct2/ride/Ride.h>

namespace OpenRCT2::Ui::Accessibility
{
    // Human-authored "what does it look like" descriptions, keyed on base ride type. These lean on
    // appearance - the structure, materials, vehicles and shape you would see - rather than on what
    // riding it feels like. Unwritten or unused/placeholder ride types return an empty string and
    // are simply omitted from the read-out.
    std::string GetRideVisualDescription(ride_type_t rideType)
    {
        switch (rideType)
        {
            // --- Roller coasters ---
            case RIDE_TYPE_SPIRAL_ROLLER_COASTER:
                return "A steel coaster of smooth tubular rails in bright colours, built from gentle "
                       "hills and long sweeping spiral curves, with no inversions.";
            case RIDE_TYPE_STAND_UP_ROLLER_COASTER:
                return "A looping steel coaster whose floorless trains have tall seat-back harnesses, "
                       "so riders stand upright; the tubular track runs through loops and curves.";
            case RIDE_TYPE_SUSPENDED_SWINGING_COASTER:
                return "A steel coaster whose cars hang from a pivot beneath the track and swing out "
                       "sideways through the bends; the rails run overhead on tall A-frame supports.";
            case RIDE_TYPE_INVERTED_ROLLER_COASTER:
                return "A steel coaster with ski-lift style trains fixed below the track and legs left "
                       "dangling, looping and corkscrewing high on tall supports.";
            case RIDE_TYPE_JUNIOR_ROLLER_COASTER:
                return "A small family coaster with low, gently curving steel track close to the "
                       "ground and short, colourful trains.";
            case RIDE_TYPE_MINI_SUSPENDED_COASTER:
                return "A small suspended coaster with little cars hanging beneath the track, legs "
                       "dangling, on a compact, gently twisting layout.";
            case RIDE_TYPE_WOODEN_WILD_MOUSE:
                return "A compact coaster on a timber frame, with single boxy cars running a twisting "
                       "layout of sharp, flat hairpin turns.";
            case RIDE_TYPE_STEEPLECHASE:
                return "A coaster with slim parallel rails and small horse-shaped cars that riders "
                       "straddle, running over rolling humps like a horse race.";
            case RIDE_TYPE_BOBSLEIGH_COASTER:
                return "A coaster whose trains slide freely along a smooth open half-pipe trough that "
                       "banks through the bends like a bobsleigh run.";
            case RIDE_TYPE_LOOPING_ROLLER_COASTER:
                return "A steel coaster of tubular track in bright colours, featuring tall vertical "
                       "loops and sweeping drops.";
            case RIDE_TYPE_DINGHY_SLIDE:
                return "Round rubber dinghies that slide down a long, open, winding trough from a "
                       "raised station.";
            case RIDE_TYPE_MINE_TRAIN_COASTER:
                return "A coaster themed as a runaway mine train, its rugged ore-car trains winding on "
                       "steel track among rockwork and timber trestles.";
            case RIDE_TYPE_CORKSCREW_ROLLER_COASTER:
                return "A steel coaster of smooth tubular track with a tall lift hill, a vertical "
                       "loop, and corkscrew inversions winding through the layout.";
            case RIDE_TYPE_REVERSE_FREEFALL_COASTER:
                return "A single car launched up a tall vertical or curving steel spike, then rolling "
                       "back down the same track.";
            case RIDE_TYPE_VERTICAL_DROP_ROLLER_COASTER:
                return "A tall steel coaster built around a near-vertical, sheer first drop.";
            case RIDE_TYPE_TWISTER_ROLLER_COASTER:
                return "A large steel coaster of thick tubular track twisting through tall loops, "
                       "rolls, and overbanked turns.";
            case RIDE_TYPE_WOODEN_ROLLER_COASTER:
                return "A classic wooden coaster with a tall white timber lattice structure, its "
                       "trains running on flat-railed wooden track over steep hills.";
            case RIDE_TYPE_SIDE_FRICTION_ROLLER_COASTER:
                return "An early-style wooden coaster whose cars are held to the track by side "
                       "friction wheels, running gentle dips on a timber frame.";
            case RIDE_TYPE_STEEL_WILD_MOUSE:
                return "A compact steel coaster with single boxy cars high on a tight layout of sharp, "
                       "flat hairpin turns.";
            case RIDE_TYPE_MULTI_DIMENSION_ROLLER_COASTER:
            case RIDE_TYPE_MULTI_DIMENSION_ROLLER_COASTER_ALT:
                return "A steel coaster whose seats sit out to the sides of the track on arms that "
                       "rotate, spinning riders as the train runs.";
            case RIDE_TYPE_FLYING_ROLLER_COASTER:
            case RIDE_TYPE_FLYING_ROLLER_COASTER_ALT:
                return "A steel coaster whose trains tilt riders face-down beneath the track as if "
                       "flying, twisting through pretzel-shaped loops.";
            case RIDE_TYPE_VIRGINIA_REEL:
                return "A ride where round tubs spin freely as they slide down a switch-backed timber "
                       "track.";
            case RIDE_TYPE_LAY_DOWN_ROLLER_COASTER:
            case RIDE_TYPE_LAY_DOWN_ROLLER_COASTER_ALT:
                return "A steel coaster whose trains hold riders lying back facing the sky, on a "
                       "looping layout.";
            case RIDE_TYPE_REVERSER_ROLLER_COASTER:
                return "A wooden coaster on a timber frame whose cars are spun to face backwards by "
                       "reverser mechanisms partway round.";
            case RIDE_TYPE_HEARTLINE_TWISTER_COASTER:
                return "A steel coaster with a distinctive twin-rail track that rolls the train around "
                       "its own centre line in a smooth barrel twist.";
            case RIDE_TYPE_GIGA_COASTER:
                return "A giant steel coaster with very tall lift hills and long, steep drops on "
                       "chunky track.";
            case RIDE_TYPE_COMPACT_INVERTED_COASTER:
                return "A compact inverted coaster with trains fixed below the track, legs dangling, "
                       "looping on tall supports.";
            case RIDE_TYPE_WATER_COASTER:
                return "A coaster whose boats run on steel track but dip through water-filled flume "
                       "sections that throw up spray.";
            case RIDE_TYPE_AIR_POWERED_VERTICAL_COASTER:
                return "A coaster whose car is launched by compressed air straight up a tall vertical "
                       "tower spike.";
            case RIDE_TYPE_INVERTED_HAIRPIN_COASTER:
                return "A wild-mouse style coaster with small cars hanging below the track, taking "
                       "sharp hairpin turns.";
            case RIDE_TYPE_INVERTED_IMPULSE_COASTER:
                return "An inverted coaster launched back and forth between two tall, twisted vertical "
                       "spikes, its train hanging below the track.";
            case RIDE_TYPE_MINI_ROLLER_COASTER:
                return "A small steel coaster with compact trains on a modest layout of gentle hills "
                       "and curves.";
            case RIDE_TYPE_MINE_RIDE:
                return "A gentle mine-themed coaster with ore-car trains winding on steel track among "
                       "rocks and timber.";
            case RIDE_TYPE_LIM_LAUNCHED_ROLLER_COASTER:
                return "A steel coaster launched to speed by magnetic motors, with loops and twists on "
                       "tubular track.";
            case RIDE_TYPE_HYPERCOASTER:
                return "A very tall steel coaster built for long, steep drops and big airtime hills "
                       "rather than inversions.";
            case RIDE_TYPE_HYPER_TWISTER:
                return "A very tall steel coaster combining huge drops with twisting, overbanked "
                       "turns.";
            case RIDE_TYPE_MONSTER_TRUCKS:
                return "Cars styled as monster trucks with oversized wheels, following a winding, "
                       "road-like track.";
            case RIDE_TYPE_SPINNING_WILD_MOUSE:
                return "A wild-mouse coaster whose round cars spin freely while taking sharp hairpin "
                       "turns on a tight steel layout.";
            case RIDE_TYPE_CLASSIC_MINI_ROLLER_COASTER:
                return "A small classic steel coaster with simple trains on a gentle, compact layout.";
            case RIDE_TYPE_HYBRID_COASTER:
                return "A coaster running smooth steel track mounted on a tall wooden lattice, mixing "
                       "steep drops with inversions.";
            case RIDE_TYPE_SINGLE_RAIL_ROLLER_COASTER:
                return "A coaster whose trains straddle a single narrow rail, giving a slender, "
                       "skeletal look as it twists and dives.";
            case RIDE_TYPE_ALPINE_COASTER:
                return "Small sleds running on a single raised rail that hugs the ground, winding like "
                       "a mountain toboggan track.";
            case RIDE_TYPE_CLASSIC_WOODEN_ROLLER_COASTER:
                return "A classic wooden coaster with a tall timber lattice and trains running over "
                       "steep hills on flat wooden track.";
            case RIDE_TYPE_CLASSIC_STAND_UP_ROLLER_COASTER:
                return "A classic steel stand-up coaster whose floorless trains carry riders upright "
                       "through loops.";
            case RIDE_TYPE_LSM_LAUNCHED_ROLLER_COASTER:
                return "A steel coaster launched to high speed by magnetic motors, climbing a tall, "
                       "twisting layout.";
            case RIDE_TYPE_CLASSIC_WOODEN_TWISTER_ROLLER_COASTER:
                return "A classic wooden coaster with a tall timber structure and a tangled, twisting "
                       "layout of crossing hills.";

            // --- Gentle and transport rides ---
            case RIDE_TYPE_MINIATURE_RAILWAY:
                return "A miniature steam locomotive pulling a line of small open carriages along "
                       "narrow-gauge tracks at ground level.";
            case RIDE_TYPE_MONORAIL:
                return "A sleek, streamlined train gliding along a single raised concrete beam carried "
                       "on slim pillars.";
            case RIDE_TYPE_SUSPENDED_MONORAIL:
                return "A sleek monorail whose cars hang beneath an elevated beam carried on tall "
                       "supports.";
            case RIDE_TYPE_CAR_RIDE:
                return "Small automobile-style cars following a hidden guide rail along a winding "
                       "ground-level road, often through tunnels and scenery.";
            case RIDE_TYPE_MINI_HELICOPTERS:
                return "Small helicopter-shaped cars following a hidden guide rail along a winding "
                       "ground-level track.";
            case RIDE_TYPE_MONORAIL_CYCLES:
                return "Pedal-powered cycle cars running one behind another along an elevated rail.";
            case RIDE_TYPE_CHAIRLIFT:
                return "Open two-seat chairs hanging from a continuously moving overhead cable strung "
                       "between tall pylons.";
            case RIDE_TYPE_MAZE:
                return "A walk-through maze laid out on the ground from tall hedges, brick, or "
                       "mirrored glass walls.";
            case RIDE_TYPE_SPIRAL_SLIDE:
                return "A tall tower wrapped by a helter-skelter spiral slide; guests climb inside and "
                       "slide down the outside on mats.";
            case RIDE_TYPE_GO_KARTS:
                return "Small, low-slung go-karts driven by guests around a flat circuit marked off "
                       "with barriers.";
            case RIDE_TYPE_MINI_GOLF:
                return "A miniature golf course of small green putting holes with themed obstacles, "
                       "walked between by guests.";
            case RIDE_TYPE_OBSERVATION_TOWER:
                return "A tall, slender tower with an enclosed round cabin that climbs slowly to the "
                       "top and back down.";
            case RIDE_TYPE_LIFT:
                return "An enclosed elevator car running up and down inside a tall tower shaft.";
            case RIDE_TYPE_CROOKED_HOUSE:
                return "A deliberately tilted, crooked little house that guests walk through.";
            case RIDE_TYPE_HAUNTED_HOUSE:
                return "A spooky themed mansion that guests enter for a haunted indoor show.";
            case RIDE_TYPE_GHOST_TRAIN:
                return "Small cars on rails that travel through the dark interior of a spooky "
                       "building.";
            case RIDE_TYPE_CIRCUS:
                return "A big circus tent where guests sit and watch a live show.";
            case RIDE_TYPE_3D_CINEMA:
                return "A cinema building where guests sit and watch a short film wearing 3D glasses.";
            case RIDE_TYPE_MOTION_SIMULATOR:
                return "An enclosed cabin mounted on hydraulic legs inside a building, jolting in time "
                       "with a film.";

            // --- Thrill rides ---
            case RIDE_TYPE_LAUNCHED_FREEFALL:
                return "A tall vertical steel mast; a small open car climbs or is launched to the top "
                       "and drops back down the tower.";
            case RIDE_TYPE_ROTO_DROP:
                return "A tall lattice drop tower; a ring of seats slowly rotates as it rises to the "
                       "top, then drops.";
            case RIDE_TYPE_SWINGING_SHIP:
                return "A large open boat shaped like a galleon that swings back and forth like a "
                       "pendulum from a tall overhead frame.";
            case RIDE_TYPE_SWINGING_INVERTER_SHIP:
                return "A passenger gondola on a tall central arm that swings up and over in complete "
                       "vertical circles.";
            case RIDE_TYPE_TOP_SPIN:
                return "Two tall arms holding a wide row of seats between them, lifting and tumbling "
                       "the seats over in the air.";
            case RIDE_TYPE_SPACE_RINGS:
                return "A set of large rotating rings that turn a strapped-in rider around in every "
                       "direction.";
            case RIDE_TYPE_TWIST:
                return "Small cars on short arms that whirl around a central spinning turntable.";
            case RIDE_TYPE_MAGIC_CARPET:
                return "A large, flat carpet-shaped platform of seats on a swinging arm that rocks up "
                       "toward vertical at each end.";
            case RIDE_TYPE_ENTERPRISE:
                return "A wheel of spinning open cars that turns flat, then tilts up toward vertical "
                       "as it spins.";
            case RIDE_TYPE_MERRY_GO_ROUND:
                return "A round carousel on a raised platform under a striped conical roof, ringed "
                       "with rows of brightly painted horses on golden poles that rise and fall as it "
                       "turns.";
            case RIDE_TYPE_FERRIS_WHEEL:
                return "A tall upright wheel turning slowly in place, with open passenger gondolas "
                       "spaced evenly around its rim.";
            case RIDE_TYPE_DODGEMS:
                return "Small electric bumper cars on a flat enclosed floor beneath a low canopy.";
            case RIDE_TYPE_FLYING_SAUCERS:
                return "Saucer-shaped bumper cars that hover and drift across a large flat floor.";

            // --- Water rides ---
            case RIDE_TYPE_BOAT_HIRE:
                return "Small open powered boats that guests steer themselves around an enclosed "
                       "pond.";
            case RIDE_TYPE_LOG_FLUME:
                return "Hollow log-shaped boats on a raised wooden flume channel, with a lift hill and "
                       "a steep splashdown drop into a pool.";
            case RIDE_TYPE_RIVER_RAPIDS:
                return "Large round rubber rafts drifting down a wide, churning artificial river "
                       "channel lined with rocks.";
            case RIDE_TYPE_SPLASH_BOATS:
                return "Large open boats on a water channel that climb a lift and plunge down a big "
                       "splashdown drop.";
            case RIDE_TYPE_SUBMARINE_RIDE:
                return "Submarine-shaped cars travelling along a track set in a pool, partly submerged "
                       "in the water.";
            case RIDE_TYPE_RIVER_RAFTS:
                return "Round rafts drifting along a calm, gently winding water channel.";

            // --- Shops, stalls and facilities ---
            case RIDE_TYPE_FOOD_STALL:
                return "A small, open-fronted food stall kiosk.";
            case RIDE_TYPE_DRINK_STALL:
                return "A small, open-fronted drinks stall kiosk.";
            case RIDE_TYPE_SHOP:
                return "A small kiosk selling souvenirs or merchandise.";
            case RIDE_TYPE_INFORMATION_KIOSK:
                return "A small information kiosk offering maps and umbrellas.";
            case RIDE_TYPE_TOILETS:
                return "A small restroom building with doors on the front for guests to enter.";
            case RIDE_TYPE_CASH_MACHINE:
                return "A small cash machine kiosk for withdrawing money.";
            case RIDE_TYPE_FIRST_AID:
                return "A small first-aid room building.";

            default:
                return {};
        }
    }
} // namespace OpenRCT2::Ui::Accessibility
