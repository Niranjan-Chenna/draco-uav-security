#include "mission_authorization.h"

namespace {

bool uses_global_position(
    const CanonicalMissionItem& item
) {

    switch (item.frame) {

        case MAV_FRAME_GLOBAL:
        case MAV_FRAME_GLOBAL_RELATIVE_ALT:
        case MAV_FRAME_GLOBAL_INT:
        case MAV_FRAME_GLOBAL_RELATIVE_ALT_INT:
        case MAV_FRAME_GLOBAL_TERRAIN_ALT:
        case MAV_FRAME_GLOBAL_TERRAIN_ALT_INT:
            return true;

        default:
            return false;
    }
}


IntentGeoPoint make_intent_point(
    const CanonicalMissionItem& item
) {

    IntentGeoPoint point{};

    point.lat_e7 = item.x;
    point.lon_e7 = item.y;
    point.altitude_m = item.z;

    return point;
}


const CanonicalMissionItem* first_position_item(
    const CanonicalMission& mission
) {

    for (const auto& item : mission.items) {

        if (uses_global_position(item)) {
            return &item;
        }
    }

    return nullptr;
}


const CanonicalMissionItem* last_position_item(
    const CanonicalMission& mission
) {

    for (
        auto it = mission.items.rbegin();
        it != mission.items.rend();
        ++it
    ) {

        if (uses_global_position(*it)) {
            return &(*it);
        }
    }

    return nullptr;
}

} // namespace


MissionAuthorizationResult evaluate_mission_authorization(
    const CanonicalMission& proposed_mission,
    const MissionDelta& delta,
    const RevisionCausalityResult& causality,
    const MissionChangeBudgetResult& change_budget,
    const MissionIntentContract& contract,
    MissionAuthorityTier authority,
    bool vehicle_in_flight,
    uint64_t current_unix_ms,
    EvaluationMode mode
) {

    // no trusted contract has been provisioned
    if (mode != EvaluationMode::NO_INTENT && contract.contract_id == 0) {

        return {
            MissionAuthorizationDecision::DEFER,
            "INTENT_CONTRACT_NOT_PROVISIONED"
        };
    }


    // invalid policy must fail closed
    if (mode != EvaluationMode::NO_INTENT && !validate_mission_intent_contract(contract)) {

        return {
            MissionAuthorizationDecision::DEFER,
            "INVALID_INTENT_CONTRACT"
        };
    }


    if (mode != EvaluationMode::NO_INTENT && !contract_valid_at(
            contract,
            current_unix_ms
        )) {

        return {
            MissionAuthorizationDecision::DEFER,
            "INTENT_CONTRACT_NOT_CURRENTLY_VALID"
        };
    }


    // hard causality violations cannot be overridden
    if (
        causality.classification ==
        RevisionCausalityClass::ROLLBACK
    ) {

        return {
            MissionAuthorizationDecision::DENY,
            "SEMANTIC_ROLLBACK_DETECTED"
        };
    }


    if (
        causality.classification ==
        RevisionCausalityClass::STALE_PARENT
    ) {

        return {
            MissionAuthorizationDecision::DENY,
            "STALE_PARENT_REVISION"
        };
    }


    if (
        causality.classification ==
        RevisionCausalityClass::CONCURRENT_CONFLICT
    ) {

        return {
            MissionAuthorizationDecision::DENY,
            "CONCURRENT_REVISION_CONFLICT"
        };
    }


    // unrelated replacement needs administrative authority
    if (
        causality.classification ==
        RevisionCausalityClass::UNRELATED_REPLACEMENT
    ) {

        if (!authority_can_administer_contract(
                authority,
                contract.authority_policy
            )) {

            return {
                MissionAuthorizationDecision::
                    REQUIRE_HIGHER_AUTHORITY,
                "UNRELATED_REPLACEMENT_REQUIRES_ADMIN"
            };
        }
    }


    // enforce task 10 change budget
    if (!change_budget.within_budget) {

        if (change_budget.requires_higher_authority) {

            return {
                MissionAuthorizationDecision::
                    REQUIRE_HIGHER_AUTHORITY,
                change_budget.reason
            };
        }

        return {
            MissionAuthorizationDecision::DEFER,
            "INVALID_CHANGE_BUDGET_RESULT"
        };
    }


    // this branch is selected only by the explicit evaluation orchestration layer.
    if (mode == EvaluationMode::NO_INTENT) {
        return {MissionAuthorizationDecision::ALLOW, "EVALUATION_INTENT_DISABLED"};
    }

    // normal mission replanning may be disabled while airborne
    if (
        vehicle_in_flight &&
        !contract.allow_in_flight_replanning &&
        causality.classification !=
            RevisionCausalityClass::NO_OP_REUPLOAD
    ) {

        if (
            authority !=
            MissionAuthorityTier::SECURITY_ADMIN
        ) {

            return {
                MissionAuthorizationDecision::
                    REQUIRE_HIGHER_AUTHORITY,
                "IN_FLIGHT_REPLANNING_REQUIRES_AUTHORITY"
            };
        }
    }


    // destination changes are independently protected by the intent contract
    if (
        delta.summary.destination_changed &&
        contract.destination_change_requires_authority
    ) {

        const bool normal_authority_allowed =
            authority_can_change_destination(
                authority,
                contract.authority_policy
            );

        const bool emergency_override_allowed =
            contract.emergency_policy.allow_destination_change &&
            authority_can_use_emergency_policy(
                authority,
                contract.authority_policy
            );

        if (
            !normal_authority_allowed &&
            !emergency_override_allowed
        ) {

            return {
                MissionAuthorizationDecision::
                    REQUIRE_HIGHER_AUTHORITY,
                "DESTINATION_CHANGE_REQUIRES_AUTHORITY"
            };
        }
    }


    // commands must satisfy normal or narrowly-scoped emergency policy
    for (const auto& item : proposed_mission.items) {

        if (!command_allowed(
                item.command,
                contract.command_policy
            )) {

            const bool permitted_as_emergency =
                emergency_command_allowed(
                    item.command,
                    contract.emergency_policy
                );

            if (permitted_as_emergency) {

                if (!authority_can_use_emergency_policy(
                        authority,
                        contract.authority_policy
                    )) {

                    return {
                        MissionAuthorizationDecision::
                            REQUIRE_HIGHER_AUTHORITY,
                        "EMERGENCY_COMMAND_REQUIRES_AUTHORITY"
                    };
                }

            } else {

                return {
                    MissionAuthorizationDecision::DENY,
                    "COMMAND_NOT_ALLOWED"
                };
            }
        }
    }


    const CanonicalMissionItem* first_item =
        first_position_item(proposed_mission);

    const CanonicalMissionItem* last_item =
        last_position_item(proposed_mission);

    if (
        first_item == nullptr ||
        last_item == nullptr
    ) {

        return {
            MissionAuthorizationDecision::DENY,
            "MISSION_HAS_NO_POSITIONAL_ITEMS"
        };
    }


    const IntentGeoPoint start_point =
        make_intent_point(*first_item);

    if (!point_inside_region(
            start_point,
            contract.start_region
        )) {

        return {
            MissionAuthorizationDecision::DENY,
            "START_REGION_VIOLATION"
        };
    }


    const IntentGeoPoint terminal_point =
        make_intent_point(*last_item);

    if (!point_inside_region(
            terminal_point,
            contract.terminal_region
        )) {

        return {
            MissionAuthorizationDecision::DENY,
            "TERMINAL_REGION_VIOLATION"
        };
    }


    for (const auto& item : proposed_mission.items) {

        if (!uses_global_position(item)) {
            continue;
        }

        const IntentGeoPoint point =
            make_intent_point(item);


        if (!altitude_inside_envelope(
                point.altitude_m,
                contract.altitude
            )) {

            return {
                MissionAuthorizationDecision::DENY,
                "ALTITUDE_ENVELOPE_VIOLATION"
            };
        }


        if (point_inside_excluded_region(
                point,
                contract
            )) {

            return {
                MissionAuthorizationDecision::DENY,
                "EXCLUDED_REGION_VIOLATION"
            };
        }


        if (!point_inside_corridor(
                point,
                contract.corridor
            )) {

            return {
                MissionAuthorizationDecision::DENY,
                "MISSION_CORRIDOR_VIOLATION"
            };
        }
    }


    // exact current mission re-upload
    if (
        causality.classification ==
        RevisionCausalityClass::NO_OP_REUPLOAD
    ) {

        return {
            MissionAuthorizationDecision::ALLOW,
            "NO_OP_REUPLOAD"
        };
    }


    return {
        MissionAuthorizationDecision::ALLOW,
        "AUTHORIZED"
    };
}


const char* mission_authorization_decision_name(
    MissionAuthorizationDecision decision
) {

    switch (decision) {

        case MissionAuthorizationDecision::ALLOW:
            return "ALLOW";

        case MissionAuthorizationDecision::DENY:
            return "DENY";

        case MissionAuthorizationDecision::DEFER:
            return "DEFER";

        case MissionAuthorizationDecision::
            REQUIRE_HIGHER_AUTHORITY:
            return "REQUIRE_HIGHER_AUTHORITY";
    }

    return "UNKNOWN";
}
