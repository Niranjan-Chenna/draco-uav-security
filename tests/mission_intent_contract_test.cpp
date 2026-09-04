#include <cassert>
#include <iostream>

#include "mission_intent_contract.h"

int main() {

    MissionIntentContract contract{};

    contract.contract_id = 1;
    contract.version = 1;

    contract.start_region.center = {
        473970000,
        85470000,
        50
    };

    contract.start_region.radius_m = 100.0;

    contract.terminal_region.center = {
        473990000,
        85472000,
        50
    };

    contract.terminal_region.radius_m = 100.0;

    contract.corridor.centerline = {
        {473970000, 85470000, 50},
        {473980000, 85471000, 50},
        {473990000, 85472000, 50}
    };

    contract.corridor.allowed_deviation_m =
        150.0;

    contract.altitude.minimum_m = 30.0f;
    contract.altitude.maximum_m = 120.0f;

    contract.command_policy.allowed_commands = {
        MAV_CMD_NAV_WAYPOINT,
        MAV_CMD_NAV_TAKEOFF,
        MAV_CMD_NAV_LAND,
        MAV_CMD_NAV_RETURN_TO_LAUNCH
    };

    contract.emergency_policy.allowed_commands = {
        MAV_CMD_NAV_LAND,
        MAV_CMD_NAV_RETURN_TO_LAUNCH
    };

    contract.emergency_policy
        .allow_destination_change = true;

    contract.authority_policy
        .destination_change_authorities = {
            MissionAuthorityTier::EMERGENCY_AUTHORITY
        };

    contract.authority_policy
        .emergency_authorities = {
            MissionAuthorityTier::EMERGENCY_AUTHORITY
        };

    contract.authority_policy
        .contract_admin_authorities = {
            MissionAuthorityTier::SECURITY_ADMIN
        };


    assert(
        validate_mission_intent_contract(contract)
    );

    std::cout
        << "CONTRACT_VALIDATION passed\n";


    IntentGeoPoint terminal_point{
        473990000,
        85472000,
        50
    };

    assert(
        point_inside_region(
            terminal_point,
            contract.terminal_region
        )
    );

    std::cout
        << "TERMINAL_REGION passed\n";


    assert(
        altitude_inside_envelope(
            50.0f,
            contract.altitude
        )
    );

    assert(
        !altitude_inside_envelope(
            20.0f,
            contract.altitude
        )
    );

    std::cout
        << "ALTITUDE_ENVELOPE passed\n";


    IntentGeoPoint corridor_point{
        473980000,
        85471000,
        50
    };

    assert(
        point_inside_corridor(
            corridor_point,
            contract.corridor
        )
    );

    std::cout
        << "MISSION_CORRIDOR passed\n";


    assert(
        command_allowed(
            MAV_CMD_NAV_WAYPOINT,
            contract.command_policy
        )
    );

    std::cout
        << "COMMAND_POLICY passed\n";


    IntentRegion forbidden{};

    forbidden.center = {
        473985000,
        85471500,
        50
    };

    forbidden.radius_m = 50.0;

    contract.excluded_regions.push_back(
        forbidden
    );

    IntentGeoPoint forbidden_point{
        473985000,
        85471500,
        50
    };

    assert(
        point_inside_excluded_region(
            forbidden_point,
            contract
        )
    );

    std::cout
        << "EXCLUDED_REGION passed\n";


    assert(
        !authority_can_change_destination(
            MissionAuthorityTier::NORMAL_OPERATOR,
            contract.authority_policy
        )
    );

    assert(
        authority_can_change_destination(
            MissionAuthorityTier::EMERGENCY_AUTHORITY,
            contract.authority_policy
        )
    );

    std::cout
        << "DESTINATION_AUTHORITY passed\n";


    assert(
        !authority_can_use_emergency_policy(
            MissionAuthorityTier::NORMAL_OPERATOR,
            contract.authority_policy
        )
    );

    assert(
        authority_can_use_emergency_policy(
            MissionAuthorityTier::EMERGENCY_AUTHORITY,
            contract.authority_policy
        )
    );

    std::cout
        << "EMERGENCY_AUTHORITY passed\n";


    assert(
        !authority_can_administer_contract(
            MissionAuthorityTier::EMERGENCY_AUTHORITY,
            contract.authority_policy
        )
    );

    assert(
        authority_can_administer_contract(
            MissionAuthorityTier::SECURITY_ADMIN,
            contract.authority_policy
        )
    );

    std::cout
        << "SECURITY_ADMIN passed\n";


    contract.has_validity_window = true;
    contract.valid_from_unix_ms = 1000;
    contract.valid_until_unix_ms = 5000;

    assert(
        contract_valid_at(
            contract,
            3000
        )
    );

    assert(
        !contract_valid_at(
            contract,
            6000
        )
    );

    std::cout
        << "VALIDITY_WINDOW passed\n";


    std::cout
        << "\nAll Task 6 contract tests passed."
        << std::endl;

    return 0;
}