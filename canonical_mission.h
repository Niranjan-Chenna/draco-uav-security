#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mission_reconstructor.h"

struct CanonicalMissionItem {
    uint16_t seq{};
    uint16_t command{};
    uint8_t frame{};

    float param1{};
    float param2{};
    float param3{};
    float param4{};

    int32_t x{};
    int32_t y{};
    float z{};

    uint8_t current{};
    uint8_t autocontinue{};
};

struct CanonicalMission {
    uint8_t mission_type{};
    std::vector<CanonicalMissionItem> items;
};

CanonicalMission make_canonical_mission(
    const MissionUploadTransaction& transaction
);

std::string serialize_canonical_mission(
    const CanonicalMission& mission
);