#include <cassert>
#include <iostream>

#include "mission_authorization.h"


CanonicalMission make_test_mission() {

    CanonicalMission mission{};
    mission.mission_type = MAV_MISSION_TYPE_MISSION;

    CanonicalMissionItem takeoff{};
    takeoff.seq = 0;
    takeoff.command = MAV_CMD_NAV_TAKEOFF;
    takeoff.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    takeoff.x = 473979578;
    takeoff.y = 85470823;
    takeoff.z = 50.0f;
    takeoff.autocontinue = 1;

    CanonicalMissionItem waypoint1{};
    waypoint1.seq = 1;
    waypoint1.command = MAV_CMD_NAV_WAYPOINT;
    waypoint1.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    waypoint1.x = 473980788;
    waypoint1.y = 85465924;
    waypoint1.z = 50.0f;
    waypoint1.autocontinue = 1;

    CanonicalMissionItem waypoint2{};
    waypoint2.seq = 2;
    waypoint2.command = MAV_CMD_NAV_WAYPOINT;
    waypoint2.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    waypoint2.x = 473985559;
    waypoint2.y = 85462147;
    waypoint2.z = 50.0f;
    waypoint2.autocontinue = 1;

    CanonicalMissionItem waypoint3{};
    waypoint3.seq = 3;
    waypoint3.command = MAV_CMD_NAV_WAYPOINT;
    waypoint3.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    waypoint3.x = 473984130;
    waypoint3.y = 85472362;
    waypoint3.z = 50.0f;
    waypoint3.autocontinue = 1;

    CanonicalMissionItem rtl{};
    rtl.seq = 4;
    rtl.command = MAV_CMD_NAV_RETURN_TO_LAUNCH;
    rtl.frame = MAV_FRAME_MISSION;
    rtl.x = 0;
    rtl.y = 0;
    rtl.z = 0.0f;
    rtl.autocontinue = 1;

    mission.items = {
        takeoff,
        waypoint1,
        waypoint2,
        waypoint3,
        rtl
    };

    return mission;
}


MissionIntentContract make_test_contract() {

    MissionIntentContract contract{};

    contract.contract_id = 1;
    contract.version = 1;


    // trusted start area
    contract.start_region.center.lat_e7 = 473979578;
    contract.start_region.center.lon_e7 = 85470823;
    contract.start_region.center.altitude_m = 50.0f;
    contract.start_region.radius_m = 150.0;


    // trusted terminal area
    contract.terminal_region.center.lat_e7 = 473984130;
    contract.terminal_region.center.lon_e7 = 85472362;
    contract.terminal_region.center.altitude_m = 50.0f;
    contract.terminal_region.radius_m = 200.0;


    // authorized corridor
    contract.corridor.centerline = {
        {473979578, 85470823, 50.0f},
        {473980788, 85465924, 50.0f},
        {473985559, 85462147, 50.0f},
        {473984130, 85472362, 50.0f}
    };

    contract.corridor.allowed_deviation_m = 200.0;


    // altitude policy
    contract.altitude.minimum_m = 0.0f;
    contract.altitude.maximum_m = 120.0f;


    // normal command policy
    contract.command_policy.allowed_commands = {
        MAV_CMD_NAV_TAKEOFF,
        MAV_CMD_NAV_WAYPOINT,
        MAV_CMD_NAV_RETURN_TO_LAUNCH
    };


    // emergency command policy
    contract.emergency_policy.allowed_commands = {
        MAV_CMD_NAV_RETURN_TO_LAUNCH,
        MAV_CMD_NAV_LAND
    };

    contract.emergency_policy.allow_destination_change = true;


    // destination changes require admin
    contract.authority_policy.destination_change_authorities = {
        MissionAuthorityTier::SECURITY_ADMIN
    };


    // emergency authority policy
    contract.authority_policy.emergency_authorities = {
        MissionAuthorityTier::EMERGENCY_AUTHORITY,
        MissionAuthorityTier::SECURITY_ADMIN
    };


    // contract administration authority
    contract.authority_policy.contract_admin_authorities = {
        MissionAuthorityTier::SECURITY_ADMIN
    };


    contract.allow_in_flight_replanning = true;
    contract.destination_change_requires_authority = true;

    contract.has_validity_window = false;

    return contract;
}


int main() {

    const CanonicalMission mission =
        make_test_mission();

    const MissionIntentContract contract =
        make_test_contract();


    // normal task 10 budget result
    MissionChangeBudgetResult within_budget{
        true,
        false,
        "WITHIN_CHANGE_BUDGET"
    };


    // test 1: normal child inside policy and budget
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::ALLOW
        );

        std::cout
            << "NORMAL_CHILD_ALLOW passed"
            << std::endl;
    }


    // test 2: semantic rollback
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::ROLLBACK;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::DENY
        );

        std::cout
            << "ROLLBACK_DENY passed"
            << std::endl;
    }


    // test 3: stale parent
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::STALE_PARENT;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::DENY
        );

        std::cout
            << "STALE_PARENT_DENY passed"
            << std::endl;
    }


    // test 4: concurrent revision conflict
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::CONCURRENT_CONFLICT;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::DENY
        );

        std::cout
            << "CONCURRENT_CONFLICT_DENY passed"
            << std::endl;
    }


    // test 5: destination change by normal operator
    {
        MissionDelta delta{};
        delta.summary.destination_changed = true;

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::
                REQUIRE_HIGHER_AUTHORITY
        );

        std::cout
            << "DESTINATION_HIGHER_AUTHORITY passed"
            << std::endl;
    }


    // test 6: destination change by security admin
    {
        MissionDelta delta{};
        delta.summary.destination_changed = true;

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::SECURITY_ADMIN,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::ALLOW
        );

        std::cout
            << "DESTINATION_ADMIN_ALLOW passed"
            << std::endl;
    }


    // test 7: no provisioned intent contract
    {
        MissionIntentContract invalid_contract{};

        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                invalid_contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::DEFER
        );

        std::cout
            << "INVALID_CONTRACT_DEFER passed"
            << std::endl;
    }


    // test 8: task 10 budget exceeded
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionChangeBudgetResult exceeded_budget{
            false,
            true,
            "HORIZONTAL_CHANGE_BUDGET_EXCEEDED"
        };

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                exceeded_budget,
                contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::
                REQUIRE_HIGHER_AUTHORITY
        );

        std::cout
            << "CHANGE_BUDGET_HIGHER_AUTHORITY passed"
            << std::endl;
    }


    // test 9: in-flight replanning disabled
    {
        MissionIntentContract no_replan_contract =
            contract;

        no_replan_contract.allow_in_flight_replanning =
            false;

        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                no_replan_contract,
                MissionAuthorityTier::NORMAL_OPERATOR,
                true,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::
                REQUIRE_HIGHER_AUTHORITY
        );

        std::cout
            << "IN_FLIGHT_REPLAN_HIGHER_AUTHORITY passed"
            << std::endl;
    }


    // test 10: security admin may perform in-flight replan
    {
        MissionIntentContract no_replan_contract =
            contract;

        no_replan_contract.allow_in_flight_replanning =
            false;

        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::NORMAL_CHILD;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                no_replan_contract,
                MissionAuthorityTier::SECURITY_ADMIN,
                true,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::ALLOW
        );

        std::cout
            << "IN_FLIGHT_ADMIN_ALLOW passed"
            << std::endl;
    }


    // test 11: admin cannot override rollback
    {
        MissionDelta delta{};

        RevisionCausalityResult causality{};
        causality.classification =
            RevisionCausalityClass::ROLLBACK;

        MissionAuthorizationResult result =
            evaluate_mission_authorization(
                mission,
                delta,
                causality,
                within_budget,
                contract,
                MissionAuthorityTier::SECURITY_ADMIN,
                false,
                0
            );

        assert(
            result.decision ==
            MissionAuthorizationDecision::DENY
        );

        std::cout
            << "ADMIN_CANNOT_OVERRIDE_ROLLBACK passed"
            << std::endl;
    }


    std::cout
        << "All Task 8 and Task 10 authorization tests passed."
        << std::endl;

    return 0;
}