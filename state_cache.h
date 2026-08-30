#pragma once 

#include <chrono> //TIME RELATED
#include <cstdint> //for fixed size integers
#include "mavlink_parser.h" //for ParsedMavlinkMessage
enum class EvidenceFreshness {
    FRESH,
    STALE,
    INVALID,
    UNKNOWN

};

template <typename T>
struct EvidenceField {
    T value{};

    uint8_t source_sysid{0};//source system id
    uint8_t source_compid{0};//source component id
    std::chrono::milliseconds age{0};
    std::chrono::steady_clock::time_point observed_at{}; //time when the evidence was observed
    bool valid{false};
    EvidenceFreshness freshness{EvidenceFreshness::UNKNOWN}; //freshness of the evidence

};
template <typename T>
bool evidence_is_usable(
    const EvidenceField<T>& field
) {
    return field.valid &&
           field.freshness == EvidenceFreshness::FRESH;
}
struct GlobalPositionEvidence {// global position based on earth coordinates
    int32_t lat{};
    int32_t lon{};
    int32_t alt_mm{};
    int32_t relative_alt_mm{};

    int16_t vx_cms{};
    int16_t vy_cms{};
    int16_t vz_cms{};
};


struct LocalPositionEvidence {// locat position based on origin coordinates
    float x{};
    float y{};
    float z{};

    float vx{};
    float vy{};
    float vz{};
};

struct MissionStateEvidence {// mission state evidence based on mission progress
    uint16_t current_seq{};// mission items px4 targets
    uint16_t total{};// no of mission items
    uint8_t mission_state{};// state of the machine like unknown and active etc.
};
struct ControlStateEvidence {// control state evidence based on control inputs
    uint8_t main_mode{};
    uint8_t sub_mode{};
    bool offboard_active{false};

};
struct SystemHealthEvidence {
    uint32_t sensors_present{};
    uint32_t sensors_enabled{};
    uint32_t sensors_healthy{};
    uint32_t unhealthy_enabled{};

};

struct EstimatorHealthEvidence {
    uint16_t flags{};

    float velocity_ratio{};
    float horizontal_position_ratio{};
    float vertical_position_ratio{};
    float magnetometer_ratio{};
    float height_above_ground_ratio{};
};

struct StateCache {
    EvidenceField<bool> armed;
    EvidenceField<uint8_t> base_mode;
    EvidenceField<uint32_t> custom_mode;
    EvidenceField<uint8_t> system_status;
    EvidenceField<uint8_t> landed_state;
    EvidenceField<GlobalPositionEvidence> global_position;
    EvidenceField<LocalPositionEvidence> local_position;
    EvidenceField<MissionStateEvidence> mission_state;
    EvidenceField<ControlStateEvidence> control_state;
    EvidenceField<bool> failsafe_active;
    EvidenceField<SystemHealthEvidence> system_health;
    EvidenceField<EstimatorHealthEvidence> estimator_health;
};

struct EvidenceSnapshot {
    StateCache state;

    std::chrono::steady_clock::time_point captured_at{};
};

EvidenceSnapshot make_evidence_snapshot(
    const StateCache& cache
);
void update_state_cache(
    StateCache& cache,
    const ParsedMavlinkMessage& parsed
);


void refresh_state_freshness(StateCache& cache);