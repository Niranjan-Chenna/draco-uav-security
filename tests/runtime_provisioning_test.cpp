#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "mission_pipeline.h"
#include "structured_events.h"

int main() {
    const auto policy = load_runtime_policy("config/sitl_policy.conf");
    assert(policy.contract.contract_id == 1001 && policy.contract.version == 1);
    assert(policy.budget.policy_id == 2001 && policy.budget.maximum_insertions == 2);
    const auto normal = resolve_principal(false);
    assert(!normal.authenticated && !normal.evaluation_mode && !principal_may_submit(normal));
    for (auto name : {"NORMAL_OPERATOR", "EMERGENCY_AUTHORITY", "SECURITY_ADMIN"}) {
        auto principal = resolve_principal(true, "sitl-test", name);
        assert(principal.authority == parse_authority(name) && !principal.authenticated);
        assert(principal.evaluation_mode && principal_may_submit(principal));
    }
    auto throws = [](auto operation) {
        bool failed = false;
        try { operation(); } catch (const std::exception&) { failed = true; }
        assert(failed);
    };
    throws([] { resolve_principal(false, "simulated", "SECURITY_ADMIN"); });
    throws([] { resolve_principal(true, "simulated", "ROOT"); });
    throws([] { load_runtime_policy("missing-policy.conf"); });
    std::ifstream input("config/sitl_policy.conf");
    std::ostringstream text; text << input.rdbuf();
    const auto original = text.str();
    const auto path = std::filesystem::path("evaluation/results/raw/policy-test.conf");
    std::filesystem::create_directories(path.parent_path());
    auto invalid = [&](const std::string& old, const std::string& replacement) {
        auto content = original;
        auto offset = content.find(old);
        assert(offset != std::string::npos);
        content.replace(offset, old.size(), replacement);
        { std::ofstream output(path); output << content; }
        throws([&] { load_runtime_policy(path.string()); });
    };
    invalid("contract_id=1001", "contract_id=0");
    invalid("contract_version=1", "contract_version=0");
    invalid("contract_version=1", "contract_version=4294967296");
    invalid("contract_id=1001", "contract_id=1001\ncontract_id=1001");
    invalid("contract_id=1001", "contract_id");
    invalid("contract_id=1001", "unknown=1001");
    invalid("47.3979578,8.5470823,50,150", "91,8.5470823,50,150");
    invalid("corridor_deviation_m=200", "corridor_deviation_m=nan");
    invalid("corridor_deviation_m=200", "corridor_deviation_m=-1");
    invalid("corridor_points=47", "corridor_points=bad;47");
    invalid("altitude_maximum_m=120", "altitude_maximum_m=-1");
    invalid("altitude_maximum_m=120", "altitude_maximum_m=inf");
    invalid("normal_allowed_commands=22,16,20,21", "normal_allowed_commands=none");
    invalid("emergency_authorities=EMERGENCY_AUTHORITY,SECURITY_ADMIN", "emergency_authorities=NORMAL_OPERATOR");
    invalid("contract_admin_authorities=SECURITY_ADMIN", "contract_admin_authorities=EMERGENCY_AUTHORITY");
    invalid("maximum_changed_item_ratio=0.5", "maximum_changed_item_ratio=2");
    invalid("maximum_insertions=2", "maximum_insertions=-1");
    invalid("maximum_horizontal_change_m=100", "maximum_horizontal_change_m=100x");
    invalid("budget_version=1", "budget_version=0");
    // every provisioned key is mandatory; deleting any one must fail closed.
    std::istringstream lines(original);
    std::string line;
    while (std::getline(lines, line))
        if (!line.empty() && line[0] != '#' && line.find('=') != std::string::npos) invalid(line, "");
    assert(parse_evaluation_mode("FULL_DRACO", false) == EvaluationMode::FULL_DRACO);
    for (auto name : {"ABLATION_NO_DELTA", "ABLATION_NO_INTENT", "ABLATION_NO_CAUSALITY",
                      "ABLATION_NO_FRESH_EVIDENCE", "ABLATION_NO_CHANGE_BUDGET"}) {
        throws([&] { parse_evaluation_mode(name, false); });
        assert(parse_evaluation_mode(name, true) != EvaluationMode::FULL_DRACO);
    }
    MissionUploadTransaction upload{};
    mavlink_mission_count_t count{}; count.count = 1;
    start_mission_upload(upload, count);
    mavlink_mission_item_int_t item{};
    item.command = MAV_CMD_NAV_WAYPOINT; item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    item.x = policy.contract.start_region.center.lat_e7;
    item.y = policy.contract.start_region.center.lon_e7; item.z = 50;
    store_mission_item(upload, item);
    StateCache state;
    auto fresh = [](auto& field) {
        field.valid = true; field.freshness = EvidenceFreshness::FRESH;
        field.observed_at = std::chrono::steady_clock::now();
    };
    fresh(state.armed); fresh(state.landed_state); fresh(state.global_position);
    state.landed_state.value = MAV_LANDED_STATE_ON_GROUND;
    MissionRevisionTracker tracker;
    auto record = evaluate_proposal(upload, tracker, "", normal, policy, state);
    assert(record.authorization.decision == MissionAuthorizationDecision::DEFER);
    assert(std::string(record.authorization.reason) == "PRINCIPAL_NOT_AUTHENTICATED");
    auto principal = resolve_principal(true, "sitl-test", "NORMAL_OPERATOR");
    record = evaluate_proposal(upload, tracker, "", principal, policy, state);
    assert(record.authorization.decision == MissionAuthorizationDecision::ALLOW);
    assert(record.contract_id == 1001 && record.change_budget_policy_id == 2001);
    assert(record.contract_version == 1 && record.change_budget_policy_version == 1);
    assert(record.principal.principal_id == "sitl-test" && !record.principal.authenticated);
    tracker.proposed = record.proposal.revision;
    assert(commit_proposed_revision(tracker));
    state.armed.observed_at -= std::chrono::hours(1);
    record = evaluate_proposal(upload, tracker, tracker.current->hash, principal, policy, state);
    assert(record.causality.classification == RevisionCausalityClass::NO_OP_REUPLOAD);
    assert(record.authorization.decision == MissionAuthorizationDecision::DEFER);
    assert(std::string(record.authorization.reason) == "PX4_EVIDENCE_NOT_USABLE");
    fresh(state.armed);
    mavlink_message_t invalid_state{};
    mavlink_extended_sys_state_t unknown_landed{};
    mavlink_msg_extended_sys_state_encode(1, 1, &invalid_state, &unknown_landed);
    update_state_cache(state, {invalid_state, MavlinkDirection::PX4_TO_GCS, {}});
    assert(!evidence_is_usable(state.landed_state));
    record = evaluate_proposal(upload, tracker, tracker.current->hash, principal, policy, state);
    assert(record.authorization.decision == MissionAuthorizationDecision::DEFER);
    fresh(state.landed_state);
    mavlink_global_position_int_t invalid_position{};
    invalid_position.lat = 910000000;
    mavlink_msg_global_position_int_encode(1, 1, &invalid_state, &invalid_position);
    update_state_cache(state, {invalid_state, MavlinkDirection::PX4_TO_GCS, {}});
    assert(!evidence_is_usable(state.global_position));
    record = evaluate_proposal(upload, tracker, tracker.current->hash, principal, policy, state);
    assert(record.authorization.decision == MissionAuthorizationDecision::DEFER);
    auto admin = resolve_principal(true, "sitl-admin", "SECURITY_ADMIN");
    assert(evaluate_proposal(upload, tracker, tracker.current->hash, admin, policy, state).authorization.decision ==
           MissionAuthorizationDecision::DEFER);
    throws([&] { evaluate_proposal(upload, tracker, tracker.current->hash, normal, policy, state, EvaluationMode::NO_INTENT); });
    const auto event = decision_event(record);
    assert(event.at("principal_authenticated") == "false");
    assert(event.at("contract_id") == "1001");
    assert(json_quote("a\n\"\\") == "\"a\\u000a\\\"\\\\\"");
    EventLog log("evaluation/results/raw/serialization-test");
    log.emit(event);
    std::filesystem::remove(path);
    std::cout << "strict policy, principal, no-op freshness, mode, provenance, and serialization tests passed\n";
}
