#pragma once

#include <cstdint>
#include <vector>

#include <development/mavlink.h>

enum class MissionAuthorityTier {
    NORMAL_OPERATOR,
    EMERGENCY_AUTHORITY,
    SECURITY_ADMIN
};

struct IntentGeoPoint {
    int32_t lat_e7{};
    int32_t lon_e7{};
    float altitude_m{};
};

struct IntentRegion {
    IntentGeoPoint center{};
    double radius_m{0.0};
};

struct MissionCorridor {
    std::vector<IntentGeoPoint> centerline;
    double allowed_deviation_m{0.0};
};

struct AltitudeEnvelope {
    float minimum_m{};
    float maximum_m{};
};

struct MissionCommandPolicy {
    std::vector<uint16_t> allowed_commands;
    std::vector<uint16_t> prohibited_commands;
};

struct EmergencyMissionPolicy {
    std::vector<uint16_t> allowed_commands;

    bool allow_destination_change{false};
};

struct MissionAuthorityPolicy {
    std::vector<MissionAuthorityTier>
        destination_change_authorities;

    std::vector<MissionAuthorityTier>
        emergency_authorities;

    std::vector<MissionAuthorityTier>
        contract_admin_authorities;
};

struct MissionIntentContract {
    uint64_t contract_id{0};
    uint32_t version{1};

    IntentRegion start_region;
    IntentRegion terminal_region;

    MissionCorridor corridor;

    std::vector<IntentRegion> excluded_regions;

    AltitudeEnvelope altitude;

    MissionCommandPolicy command_policy;

    EmergencyMissionPolicy emergency_policy;

    MissionAuthorityPolicy authority_policy;

    bool allow_in_flight_replanning{true};

    bool destination_change_requires_authority{true};

    bool has_validity_window{false};

    uint64_t valid_from_unix_ms{0};
    uint64_t valid_until_unix_ms{0};
};

bool point_inside_region(
    const IntentGeoPoint& point,
    const IntentRegion& region
);

bool point_inside_corridor(
    const IntentGeoPoint& point,
    const MissionCorridor& corridor
);

bool point_inside_excluded_region(
    const IntentGeoPoint& point,
    const MissionIntentContract& contract
);

bool altitude_inside_envelope(
    float altitude_m,
    const AltitudeEnvelope& envelope
);

bool command_allowed(
    uint16_t command,
    const MissionCommandPolicy& policy
);

bool emergency_command_allowed(
    uint16_t command,
    const EmergencyMissionPolicy& policy
);

bool authority_can_change_destination(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
);

bool authority_can_use_emergency_policy(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
);

bool authority_can_administer_contract(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
);

bool contract_valid_at(
    const MissionIntentContract& contract,
    uint64_t unix_time_ms
);

bool validate_mission_intent_contract(
    const MissionIntentContract& contract
);