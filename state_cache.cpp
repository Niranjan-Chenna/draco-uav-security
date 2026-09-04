#include "state_cache.h"
#include "mavlink_parser.h"

void update_state_cache(
    StateCache& cache,
    const ParsedMavlinkMessage& parsed
) {
    // only px4-originated traffic can update vehicle state
    if (parsed.direction != MavlinkDirection::PX4_TO_GCS) {
        return;
    }

    if (parsed.message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {

        mavlink_heartbeat_t heartbeat{};

        mavlink_msg_heartbeat_decode(
            &parsed.message,
            &heartbeat
        );
        auto observed_now = std::chrono::steady_clock::now();
        bool is_armed =
            (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
        //armed is a value which tells if its armed or disarmed.
        cache.armed.value = is_armed;

        cache.armed.source_sysid = parsed.message.sysid;
        cache.armed.source_compid = parsed.message.compid;

        cache.armed.observed_at =
            observed_now;

        cache.armed.valid = true;
        cache.armed.freshness = EvidenceFreshness::FRESH;
        //base mode contains flags which tells armed , manual or custom mode-enabled.
        cache.base_mode.value = heartbeat.base_mode;
        cache.base_mode.source_sysid = parsed.message.sysid;
        cache.base_mode.source_compid = parsed.message.compid;
        cache.base_mode.observed_at = observed_now;
        cache.base_mode.valid = true;
        cache.base_mode.freshness = EvidenceFreshness::FRESH;
        
        //custom mode contains px4 encoded flight information like Mission etc.
        cache.custom_mode.value = heartbeat.custom_mode;
        cache.custom_mode.source_sysid = parsed.message.sysid;
        cache.custom_mode.source_compid = parsed.message.compid;
        cache.custom_mode.observed_at = observed_now;
        cache.custom_mode.valid = true;
        cache.custom_mode.freshness = EvidenceFreshness::FRESH;

        //system status tells the current system state physically.
        cache.system_status.value = heartbeat.system_status;
        cache.system_status.source_sysid = parsed.message.sysid;
        cache.system_status.source_compid = parsed.message.compid;
        cache.system_status.observed_at = observed_now;
        cache.system_status.valid = true;
        cache.system_status.freshness = EvidenceFreshness::FRESH;
        uint8_t main_mode =
        static_cast<uint8_t>((heartbeat.custom_mode >> 16) & 0xFF);

        uint8_t sub_mode =
        static_cast<uint8_t>((heartbeat.custom_mode >> 24) & 0xFF);


        cache.control_state.value.main_mode = main_mode;
        cache.control_state.value.sub_mode = sub_mode;

        // px4 main mode 6 is offboard
        cache.control_state.value.offboard_active =
            (main_mode == 6);

        cache.control_state.source_sysid =
        parsed.message.sysid;

        cache.control_state.source_compid =
        parsed.message.compid;

        cache.control_state.observed_at =
        observed_now;

        cache.control_state.valid = true;
        cache.control_state.freshness =
        EvidenceFreshness::FRESH;  

        bool failsafe_active =
        heartbeat.system_status == MAV_STATE_CRITICAL ||
        heartbeat.system_status == MAV_STATE_EMERGENCY;

        cache.failsafe_active.value =
        failsafe_active;

        cache.failsafe_active.source_sysid =
        parsed.message.sysid;

        cache.failsafe_active.source_compid =
        parsed.message.compid;

        cache.failsafe_active.observed_at =
        observed_now;

        cache.failsafe_active.valid = true;

        cache.failsafe_active.freshness =
        EvidenceFreshness::FRESH;
        
    }

    if (parsed.message.msgid == MAVLINK_MSG_ID_EXTENDED_SYS_STATE) {

    mavlink_extended_sys_state_t extended_state{};
        //landed state helps in knowing if the drone is landed or in flight.
    mavlink_msg_extended_sys_state_decode(
        &parsed.message,
        &extended_state
    );

    auto observed_now =
        std::chrono::steady_clock::now();

    cache.landed_state.value =
        extended_state.landed_state;

    cache.landed_state.source_sysid =
        parsed.message.sysid;

    cache.landed_state.source_compid =
        parsed.message.compid;

    cache.landed_state.observed_at =
        observed_now;

    cache.landed_state.valid =
        extended_state.landed_state >= MAV_LANDED_STATE_ON_GROUND &&
        extended_state.landed_state <= MAV_LANDED_STATE_LANDING;

    cache.landed_state.freshness =
        cache.landed_state.valid ? EvidenceFreshness::FRESH : EvidenceFreshness::INVALID;

}
    if (parsed.message.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT) {

    mavlink_global_position_int_t global_position{};

    mavlink_msg_global_position_int_decode(
        &parsed.message,
        &global_position
    );

    auto observed_now =
        std::chrono::steady_clock::now();

    cache.global_position.value.lat =
        global_position.lat;

    cache.global_position.value.lon =
        global_position.lon;

    cache.global_position.value.alt_mm =
        global_position.alt;

    cache.global_position.value.relative_alt_mm =
        global_position.relative_alt;

    cache.global_position.value.vx_cms =
        global_position.vx;

    cache.global_position.value.vy_cms =
        global_position.vy;

    cache.global_position.value.vz_cms =
        global_position.vz;

    cache.global_position.source_sysid =
        parsed.message.sysid;

    cache.global_position.source_compid =
        parsed.message.compid;

    cache.global_position.observed_at =
        observed_now;

    cache.global_position.valid =
        global_position.lat >= -900000000 && global_position.lat <= 900000000 &&
        global_position.lon >= -1800000000 && global_position.lon <= 1800000000;
    cache.global_position.freshness =
        cache.global_position.valid ? EvidenceFreshness::FRESH : EvidenceFreshness::INVALID;
}
if (parsed.message.msgid == MAVLINK_MSG_ID_LOCAL_POSITION_NED) {

    mavlink_local_position_ned_t local_position{};

    mavlink_msg_local_position_ned_decode(
        &parsed.message,
        &local_position
    );

    auto observed_now =
        std::chrono::steady_clock::now();

    cache.local_position.value.x =
        local_position.x;

    cache.local_position.value.y =
        local_position.y;

    cache.local_position.value.z =
        local_position.z;

    cache.local_position.value.vx =
        local_position.vx;

    cache.local_position.value.vy =
        local_position.vy;

    cache.local_position.value.vz =
        local_position.vz;

    cache.local_position.source_sysid =
        parsed.message.sysid;

    cache.local_position.source_compid =
        parsed.message.compid;

    cache.local_position.observed_at =
        observed_now;

    cache.local_position.valid = true;
    cache.local_position.freshness =
        EvidenceFreshness::FRESH;
}
if (parsed.message.msgid == MAVLINK_MSG_ID_MISSION_CURRENT) {

    mavlink_mission_current_t mission{};

    mavlink_msg_mission_current_decode(
        &parsed.message,
        &mission
    );

    auto observed_now = std::chrono::steady_clock::now();

    cache.mission_state.value.current_seq =
        mission.seq;

    cache.mission_state.value.total =
        mission.total;

    cache.mission_state.value.mission_state =
        mission.mission_state;

    cache.mission_state.source_sysid =
        parsed.message.sysid;

    cache.mission_state.source_compid =
        parsed.message.compid;

    cache.mission_state.observed_at =
        observed_now;

    cache.mission_state.valid = true;

    cache.mission_state.freshness =
        EvidenceFreshness::FRESH;
}
if (parsed.message.msgid == MAVLINK_MSG_ID_SYS_STATUS) {

    mavlink_sys_status_t status{};

    mavlink_msg_sys_status_decode(
        &parsed.message,
        &status
    );

    auto observed_now =
        std::chrono::steady_clock::now();

    cache.system_health.value.sensors_present =
        status.onboard_control_sensors_present;

    cache.system_health.value.sensors_enabled =
        status.onboard_control_sensors_enabled;

    cache.system_health.value.sensors_healthy =
        status.onboard_control_sensors_health;

    cache.system_health.value.unhealthy_enabled =
        status.onboard_control_sensors_enabled &
        ~status.onboard_control_sensors_health;

    cache.system_health.source_sysid =
        parsed.message.sysid;

    cache.system_health.source_compid =
        parsed.message.compid;

    cache.system_health.observed_at =
        observed_now;

    cache.system_health.valid = true;

    cache.system_health.freshness =
        EvidenceFreshness::FRESH;
}
if (parsed.message.msgid == MAVLINK_MSG_ID_ESTIMATOR_STATUS) {

    mavlink_estimator_status_t estimator{};

    mavlink_msg_estimator_status_decode(
        &parsed.message,
        &estimator
    );

    auto observed_now =
        std::chrono::steady_clock::now();

    cache.estimator_health.value.flags =
        estimator.flags;

    cache.estimator_health.value.velocity_ratio =
        estimator.vel_ratio;

    cache.estimator_health.value.horizontal_position_ratio =
        estimator.pos_horiz_ratio;

    cache.estimator_health.value.vertical_position_ratio =
        estimator.pos_vert_ratio;

    cache.estimator_health.value.magnetometer_ratio =
        estimator.mag_ratio;

    cache.estimator_health.value.height_above_ground_ratio =
        estimator.hagl_ratio;

    cache.estimator_health.source_sysid =
        parsed.message.sysid;

    cache.estimator_health.source_compid =
        parsed.message.compid;

    cache.estimator_health.observed_at =
        observed_now;

    cache.estimator_health.valid = true;

    cache.estimator_health.freshness =
        EvidenceFreshness::FRESH;
}
}

namespace {

constexpr auto kEvidenceFreshnessThreshold =
    std::chrono::milliseconds(3000);

template <typename T>
void refresh_evidence_field(
    EvidenceField<T>& field,
    std::chrono::steady_clock::time_point now
) {
    if (!field.valid) {
        field.age = std::chrono::milliseconds(0);

        if (field.freshness != EvidenceFreshness::INVALID) {
            field.freshness = EvidenceFreshness::UNKNOWN;
        }

        return;
    }

    field.age =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - field.observed_at
        );

    if (field.age <= kEvidenceFreshnessThreshold) {
        field.freshness = EvidenceFreshness::FRESH;
    } else {
        field.freshness = EvidenceFreshness::STALE;
    }
}

}

void refresh_state_freshness(
    StateCache& cache
) {
    auto now =
        std::chrono::steady_clock::now();

    refresh_evidence_field(cache.armed, now);
    refresh_evidence_field(cache.base_mode, now);
    refresh_evidence_field(cache.custom_mode, now);
    refresh_evidence_field(cache.system_status, now);
    refresh_evidence_field(cache.landed_state, now);
    refresh_evidence_field(cache.global_position, now);
    refresh_evidence_field(cache.local_position, now);
    refresh_evidence_field(cache.mission_state, now);
    refresh_evidence_field(cache.control_state, now);
    refresh_evidence_field(cache.failsafe_active, now);
    refresh_evidence_field(cache.system_health, now);
    refresh_evidence_field(cache.estimator_health, now);
}
EvidenceSnapshot make_evidence_snapshot(
    const StateCache& cache
) {
    EvidenceSnapshot snapshot{};

    snapshot.state = cache;

    snapshot.captured_at =
        std::chrono::steady_clock::now();

    // refresh the copied evidence at snapshot time
    refresh_state_freshness(snapshot.state);

    return snapshot;
}
