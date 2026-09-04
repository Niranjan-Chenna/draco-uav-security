#include <cassert>
#include <iostream>

#include "mission_delta.h"

CanonicalMissionItem make_waypoint(
    uint16_t seq,
    int32_t lat,
    int32_t lon,
    float alt
) {
    CanonicalMissionItem item{};

    item.seq = seq;
    item.command = MAV_CMD_NAV_WAYPOINT;
    item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    item.x = lat;
    item.y = lon;
    item.z = alt;
    item.autocontinue = 1;

    return item;
}

bool contains_type(
    const MissionDelta& delta,
    MissionDeltaType type
) {
    for (const auto& change : delta.changes) {
        for (auto change_type : change.types) {
            if (change_type == type) {
                return true;
            }
        }
    }

    return false;
}

int main() {

    CanonicalMission original{};

    original.items = {
        make_waypoint(0, 473970000, 85470000, 50),
        make_waypoint(1, 473980000, 85471000, 50),
        make_waypoint(2, 473990000, 85472000, 50)
    };


    // no-op

    MissionDelta delta =
        compute_mission_delta(
            original,
            original
        );

    assert(delta.no_op);

    std::cout << "NO_OP passed\n";


    // insert

    CanonicalMission inserted = original;

    inserted.items.insert(
        inserted.items.begin() + 1,
        make_waypoint(
            1,
            473975000,
            85470500,
            50
        )
    );

    delta =
        compute_mission_delta(
            original,
            inserted
        );

    assert(contains_type(
        delta,
        MissionDeltaType::INSERT
    ));

    assert(delta.summary.inserted == 1);

    std::cout << "INSERT passed\n";


    // delete

    CanonicalMission deleted = original;

    deleted.items.erase(
        deleted.items.begin() + 1
    );

    delta =
        compute_mission_delta(
            original,
            deleted
        );

    assert(contains_type(
        delta,
        MissionDeltaType::DELETE
    ));

    std::cout << "DELETE passed\n";


    // altitude

    CanonicalMission altitude = original;

    altitude.items[1].z = 20;

    delta =
        compute_mission_delta(
            original,
            altitude
        );

    assert(contains_type(
        delta,
        MissionDeltaType::ALTITUDE_CHANGE
    ));

    assert(
        delta.summary.maximum_altitude_change_m ==
        30.0
    );

    std::cout << "ALTITUDE_CHANGE passed\n";


    // horizontal movement

    CanonicalMission moved = original;

    moved.items[1].x += 5000;

    delta =
        compute_mission_delta(
            original,
            moved
        );

    assert(contains_type(
        delta,
        MissionDeltaType::MOVE_HORIZONTAL
    ));

    assert(
        delta.summary.maximum_horizontal_displacement_m >
        0.0
    );

    std::cout << "MOVE_HORIZONTAL passed\n";


    // parameter change

    CanonicalMission parameter = original;

    parameter.items[1].param1 = 10.0f;

    delta =
        compute_mission_delta(
            original,
            parameter
        );

    assert(contains_type(
        delta,
        MissionDeltaType::PARAMETER_CHANGE
    ));

    std::cout << "PARAMETER_CHANGE passed\n";


    // command + destination change

    CanonicalMission command = original;

    command.items.back().command =
        MAV_CMD_NAV_LAND;

    delta =
        compute_mission_delta(
            original,
            command
        );

    assert(contains_type(
        delta,
        MissionDeltaType::COMMAND_CHANGE
    ));

    assert(contains_type(
        delta,
        MissionDeltaType::DESTINATION_CHANGE
    ));

    assert(
        delta.summary.critical_command_introduced
    );

    std::cout
        << "COMMAND_CHANGE + DESTINATION_CHANGE passed\n";


    // reorder

    CanonicalMission reordered = original;

    std::swap(
        reordered.items[1],
        reordered.items[2]
    );

    delta =
        compute_mission_delta(
            original,
            reordered
        );

    assert(contains_type(
        delta,
        MissionDeltaType::REORDER
    ));

    std::cout << "REORDER passed\n";


    // repeated waypoint + insertion

    CanonicalMission duplicate{};

    auto a =
        make_waypoint(
            0,
            473970000,
            85470000,
            50
        );

    auto b =
        make_waypoint(
            1,
            473980000,
            85471000,
            50
        );

    duplicate.items = {a, b, b};

    CanonicalMission duplicate_insert =
        duplicate;

    duplicate_insert.items.insert(
        duplicate_insert.items.begin() + 2,
        make_waypoint(
            2,
            473985000,
            85471500,
            50
        )
    );

    delta =
        compute_mission_delta(
            duplicate,
            duplicate_insert
        );

    assert(delta.summary.inserted == 1);
    assert(delta.summary.deleted == 0);

    std::cout
        << "DUPLICATE WAYPOINT passed\n";


    // complete mission replacement

    CanonicalMission replacement =
        original;

    replacement.items[0].x += 100000;
    replacement.items[1].x += 100000;
    replacement.items[2].x += 100000;

    delta =
        compute_mission_delta(
            original,
            replacement
        );

    assert(
        delta.summary.changed_item_ratio >=
        1.0
    );

    assert(contains_type(
        delta,
        MissionDeltaType::MAJOR_REPLACEMENT
    ));

    std::cout
        << "MAJOR_REPLACEMENT passed\n";


    std::cout
        << "\nAll final Task 5 tests passed."
        << std::endl;

    return 0;
}