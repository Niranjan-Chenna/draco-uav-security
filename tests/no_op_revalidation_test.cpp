#include <cassert>
#include <cstring>
#include <iostream>

#include "mission_authorization.h"

int main() {
    CanonicalMission mission{};
    CanonicalMissionItem item{};
    item.command = MAV_CMD_NAV_WAYPOINT;
    item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    item.x = 473979578;
    item.y = 85470823;
    item.z = 50;
    mission.items = {item};
    MissionIntentContract contract{};
    contract.contract_id = 7;
    contract.start_region = {{item.x, item.y, item.z}, 100};
    contract.terminal_region = contract.start_region;
    contract.corridor.centerline = {contract.start_region.center};
    contract.corridor.allowed_deviation_m = 100;
    contract.altitude = {0, 120};
    contract.command_policy.allowed_commands = {MAV_CMD_NAV_WAYPOINT};
    const auto delta = compute_mission_delta(mission, mission);
    assert(delta.no_op);
    MissionRevisionTracker tracker{};
    propose_mission_revision(tracker, mission);
    assert(commit_proposed_revision(tracker));
    propose_mission_revision(tracker, mission);
    auto proposal = make_proposal_record(*tracker.proposed, tracker.current, "test", 0);
    auto causality = classify_revision_causality(tracker, proposal, delta);
    assert(causality.classification == RevisionCausalityClass::NO_OP_REUPLOAD);
    const auto budget = evaluate_change_budget(delta, {}, MissionAuthorityTier::NORMAL_OPERATOR);
    auto check = [&](const MissionIntentContract& policy, MissionAuthorizationDecision expected,
                     const char* reason, MissionAuthorityTier tier = MissionAuthorityTier::NORMAL_OPERATOR) {
        const auto result = evaluate_mission_authorization(
            mission, delta, causality, budget, policy, tier, false, 0);
        assert(result.decision == expected);
        assert(std::strcmp(result.reason, reason) == 0);
        assert(tracker.current->hash == compute_mission_hash(mission));
    };
    check(contract, MissionAuthorizationDecision::ALLOW, "NO_OP_REUPLOAD");
    auto updated = contract;
    updated.version = 2;
    updated.altitude.maximum_m = 40;
    check(updated, MissionAuthorizationDecision::DENY, "ALTITUDE_ENVELOPE_VIOLATION");
    check(updated, MissionAuthorizationDecision::DENY, "ALTITUDE_ENVELOPE_VIOLATION", MissionAuthorityTier::SECURITY_ADMIN);
    updated = contract;
    updated.version = 2;
    updated.command_policy.allowed_commands = {MAV_CMD_NAV_TAKEOFF};
    check(updated, MissionAuthorizationDecision::DENY, "COMMAND_NOT_ALLOWED");
    updated = contract;
    updated.start_region.center.lat_e7 += 100000;
    check(updated, MissionAuthorizationDecision::DENY, "START_REGION_VIOLATION");
    updated = contract;
    updated.terminal_region.center.lat_e7 += 100000;
    check(updated, MissionAuthorizationDecision::DENY, "TERMINAL_REGION_VIOLATION");
    updated = contract;
    updated.corridor.centerline[0].lat_e7 += 100000;
    check(updated, MissionAuthorizationDecision::DENY, "MISSION_CORRIDOR_VIOLATION");
    updated = contract;
    updated.excluded_regions.push_back(contract.start_region);
    check(updated, MissionAuthorizationDecision::DENY, "EXCLUDED_REGION_VIOLATION");
    updated = contract;
    updated.has_validity_window = true;
    updated.valid_from_unix_ms = 1;
    updated.valid_until_unix_ms = 2;
    check(updated, MissionAuthorizationDecision::DEFER, "INTENT_CONTRACT_NOT_CURRENTLY_VALID");
    for (const auto tier : {MissionAuthorityTier::NORMAL_OPERATOR,
                           MissionAuthorityTier::EMERGENCY_AUTHORITY,
                           MissionAuthorityTier::SECURITY_ADMIN}) {
        causality.classification = RevisionCausalityClass::ROLLBACK;
        check(contract, MissionAuthorizationDecision::DENY, "SEMANTIC_ROLLBACK_DETECTED", tier);
        causality.classification = RevisionCausalityClass::STALE_PARENT;
        check(contract, MissionAuthorizationDecision::DENY, "STALE_PARENT_REVISION", tier);
        causality.classification = RevisionCausalityClass::CONCURRENT_CONFLICT;
        check(contract, MissionAuthorizationDecision::DENY, "CONCURRENT_REVISION_CONFLICT", tier);
    }
    std::cout << "no-op current-policy revalidation and hard causality passed\n";
}
