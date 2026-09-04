#include "mission_intent_contract.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double EARTH_RADIUS_M = 6371000.0;
constexpr double PI = 3.14159265358979323846;

double to_degrees(int32_t value) {
    return static_cast<double>(value) / 1e7;
}

double to_radians(double value) {
    return value * PI / 180.0;
}

double distance_m(
    const IntentGeoPoint& a,
    const IntentGeoPoint& b
) {
    double lat1 =
        to_radians(to_degrees(a.lat_e7));

    double lat2 =
        to_radians(to_degrees(b.lat_e7));

    double dlat =
        lat2 - lat1;

    double dlon =
        to_radians(
            to_degrees(b.lon_e7) -
            to_degrees(a.lon_e7)
        );

    double h =
        std::sin(dlat / 2.0) *
        std::sin(dlat / 2.0)
        +
        std::cos(lat1) *
        std::cos(lat2) *
        std::sin(dlon / 2.0) *
        std::sin(dlon / 2.0);

    return 2.0 * EARTH_RADIUS_M *
           std::atan2(
               std::sqrt(h),
               std::sqrt(1.0 - h)
           );
}

double point_to_segment_distance_m(
    const IntentGeoPoint& point,
    const IntentGeoPoint& start,
    const IntentGeoPoint& end
) {
    double reference_lat =
        to_radians(to_degrees(point.lat_e7));

    auto to_xy =
        [reference_lat, &point]
        (const IntentGeoPoint& p) {

        double x =
            to_radians(
                to_degrees(p.lon_e7) -
                to_degrees(point.lon_e7)
            )
            *
            std::cos(reference_lat)
            *
            EARTH_RADIUS_M;

        double y =
            to_radians(
                to_degrees(p.lat_e7) -
                to_degrees(point.lat_e7)
            )
            *
            EARTH_RADIUS_M;

        return std::pair<double, double>{x, y};
    };

    auto a = to_xy(start);
    auto b = to_xy(end);

    double dx =
        b.first - a.first;

    double dy =
        b.second - a.second;

    double length_squared =
        dx * dx + dy * dy;

    if (length_squared == 0.0) {
        return std::sqrt(
            a.first * a.first +
            a.second * a.second
        );
    }

    double t =
        -(a.first * dx + a.second * dy)
        / length_squared;

    t = std::clamp(t, 0.0, 1.0);

    double closest_x =
        a.first + t * dx;

    double closest_y =
        a.second + t * dy;

    return std::sqrt(
        closest_x * closest_x +
        closest_y * closest_y
    );
}

bool authority_present(
    MissionAuthorityTier authority,
    const std::vector<MissionAuthorityTier>& authorities
) {
    return std::find(
        authorities.begin(),
        authorities.end(),
        authority
    ) != authorities.end();
}

}

bool point_inside_region(
    const IntentGeoPoint& point,
    const IntentRegion& region
) {
    return distance_m(
        point,
        region.center
    ) <= region.radius_m;
}

bool point_inside_corridor(
    const IntentGeoPoint& point,
    const MissionCorridor& corridor
) {
    if (corridor.centerline.empty()) {
        return false;
    }

    if (corridor.centerline.size() == 1) {
        return distance_m(
            point,
            corridor.centerline.front()
        ) <= corridor.allowed_deviation_m;
    }

    for (std::size_t i = 0;
         i + 1 < corridor.centerline.size();
         ++i) {

        double distance =
            point_to_segment_distance_m(
                point,
                corridor.centerline[i],
                corridor.centerline[i + 1]
            );

        if (distance <=
            corridor.allowed_deviation_m) {

            return true;
        }
    }

    return false;
}

bool point_inside_excluded_region(
    const IntentGeoPoint& point,
    const MissionIntentContract& contract
) {
    for (const auto& region :
         contract.excluded_regions) {

        if (point_inside_region(
                point,
                region)) {

            return true;
        }
    }

    return false;
}

bool altitude_inside_envelope(
    float altitude_m,
    const AltitudeEnvelope& envelope
) {
    return
        altitude_m >= envelope.minimum_m &&
        altitude_m <= envelope.maximum_m;
}

bool command_allowed(
    uint16_t command,
    const MissionCommandPolicy& policy
) {
    if (std::find(
            policy.prohibited_commands.begin(),
            policy.prohibited_commands.end(),
            command
        ) != policy.prohibited_commands.end()) {

        return false;
    }

    if (policy.allowed_commands.empty()) {
        return true;
    }

    return std::find(
        policy.allowed_commands.begin(),
        policy.allowed_commands.end(),
        command
    ) != policy.allowed_commands.end();
}

bool emergency_command_allowed(
    uint16_t command,
    const EmergencyMissionPolicy& policy
) {
    return std::find(
        policy.allowed_commands.begin(),
        policy.allowed_commands.end(),
        command
    ) != policy.allowed_commands.end();
}

bool authority_can_change_destination(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
) {
    return authority_present(
        authority,
        policy.destination_change_authorities
    );
}

bool authority_can_use_emergency_policy(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
) {
    return authority_present(
        authority,
        policy.emergency_authorities
    );
}

bool authority_can_administer_contract(
    MissionAuthorityTier authority,
    const MissionAuthorityPolicy& policy
) {
    return authority_present(
        authority,
        policy.contract_admin_authorities
    );
}

bool contract_valid_at(
    const MissionIntentContract& contract,
    uint64_t unix_time_ms
) {
    if (!contract.has_validity_window) {
        return true;
    }

    return
        unix_time_ms >= contract.valid_from_unix_ms &&
        unix_time_ms <= contract.valid_until_unix_ms;
}

bool validate_mission_intent_contract(
    const MissionIntentContract& contract
) {
    if (contract.start_region.radius_m < 0.0) {
        return false;
    }

    if (contract.terminal_region.radius_m < 0.0) {
        return false;
    }

    if (contract.corridor.allowed_deviation_m < 0.0) {
        return false;
    }

    if (contract.altitude.minimum_m >
        contract.altitude.maximum_m) {

        return false;
    }

    for (const auto& region :
         contract.excluded_regions) {

        if (region.radius_m < 0.0) {
            return false;
        }
    }

    if (contract.has_validity_window &&
        contract.valid_from_unix_ms >
        contract.valid_until_unix_ms) {

        return false;
    }

    return true;
}