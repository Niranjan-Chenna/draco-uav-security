#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "canonical_mission.h"

enum class MissionDeltaType {
    NO_OP,
    INSERT,
    DELETE,
    MOVE_HORIZONTAL,
    ALTITUDE_CHANGE,
    COMMAND_CHANGE,
    PARAMETER_CHANGE,
    REORDER,
    DESTINATION_CHANGE,
    MAJOR_REPLACEMENT
};

struct MissionItemChange {
    std::vector<MissionDeltaType> types;

    bool has_old_item{false};
    bool has_new_item{false};

    CanonicalMissionItem old_item{};
    CanonicalMissionItem new_item{};

    double horizontal_displacement_m{0.0};
    double altitude_delta_m{0.0};

    std::size_t old_index{0};
    std::size_t new_index{0};
};

struct MissionDeltaSummary {
    std::size_t inserted{0};
    std::size_t deleted{0};
    std::size_t moved_horizontal{0};
    std::size_t altitude_changed{0};
    std::size_t command_changed{0};
    std::size_t parameter_changed{0};
    std::size_t reordered{0};

    double maximum_horizontal_displacement_m{0.0};
    double maximum_altitude_change_m{0.0};

    bool destination_changed{false};
    bool critical_command_introduced{false};

    double changed_item_ratio{0.0};
    bool major_replacement{false};
};

struct MissionDelta {
    std::vector<MissionItemChange> changes;
    MissionDeltaSummary summary;

    bool no_op{false};
};

MissionDelta compute_mission_delta(
    const CanonicalMission& current,
    const CanonicalMission& proposed
);

const char* mission_delta_type_name(
    MissionDeltaType type
);