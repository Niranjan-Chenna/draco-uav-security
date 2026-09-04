#include "mission_pipeline.h"
#include <chrono>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}
}

MissionDecisionRecord evaluate_proposal(
    const MissionUploadTransaction& upload, MissionRevisionTracker& tracker,
    const std::string& expected_parent_hash, const PrincipalContext& principal,
    const RuntimePolicy& policy, StateCache& evidence, EvaluationMode mode,
    const MissionProposalRecord* active_proposal, const std::string& scenario_id) {
    if (mode != EvaluationMode::FULL_DRACO && !principal.evaluation_mode)
        throw std::runtime_error("ablation forbidden outside explicit evaluation mode");
    if (!mission_upload_complete(upload)) throw std::runtime_error("incomplete proposal");
    const auto total_start = Clock::now();
    auto start = Clock::now();
    auto canonical = make_canonical_mission(upload);
    const auto canonical_us = elapsed(start);
    start = Clock::now();
    auto revision = make_mission_revision(tracker.next_id++, canonical);
    const auto hash_us = elapsed(start);
    start = Clock::now();
    MissionDelta delta{};
    if (tracker.current && mode != EvaluationMode::NO_DELTA)
        delta = compute_mission_delta(tracker.current->mission, canonical);
    const auto delta_us = elapsed(start);
    const auto unix_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    auto proposal = make_proposal_record(revision, tracker.current, principal.principal_id, unix_ms);
    proposal.expected_parent_hash = expected_parent_hash;
    const auto causality = classify_revision_causality(tracker, proposal, delta, active_proposal);
    auto effective_causality = causality;
    if (mode == EvaluationMode::NO_CAUSALITY)
        effective_causality.classification = tracker.current ? RevisionCausalityClass::NORMAL_CHILD :
            RevisionCausalityClass::INITIAL_MISSION;
    refresh_state_freshness(evidence);
    const bool in_flight = evidence_is_usable(evidence.armed) && evidence_is_usable(evidence.landed_state) &&
        evidence.armed.value && evidence.landed_state.value == MAV_LANDED_STATE_IN_AIR;
    start = Clock::now();
    auto budget = evaluate_change_budget(delta, policy.budget, principal.authority);
    if (mode == EvaluationMode::NO_CHANGE_BUDGET)
        budget = {true, false, "EVALUATION_CHANGE_BUDGET_DISABLED"};
    auto authorization = evaluate_mission_authorization(canonical, delta, effective_causality, budget,
        policy.contract, principal.authority, in_flight, unix_ms, mode);
    auto record = make_mission_decision_record(proposal, delta, budget, causality, principal.authority,
        in_flight, policy.contract, policy.budget, evidence, authorization, unix_ms);
    // principal resolution cannot convert any negative security result to allow.
    if (!principal_may_submit(principal) && authorization.decision == MissionAuthorizationDecision::ALLOW)
        authorization = {MissionAuthorizationDecision::DEFER, "PRINCIPAL_NOT_AUTHENTICATED"};
    if (!record.evidence_usable && mode != EvaluationMode::NO_FRESH_EVIDENCE &&
        authorization.decision == MissionAuthorizationDecision::ALLOW)
        authorization = {MissionAuthorizationDecision::DEFER, "PX4_EVIDENCE_NOT_USABLE"};
    // the runtime adapter supports one explicit altitude reference; it performs no implicit conversion.
    if (authorization.decision == MissionAuthorizationDecision::ALLOW) {
        for (const auto& item : canonical.items) {
            if (item.frame != MAV_FRAME_MISSION && item.frame != MAV_FRAME_GLOBAL_RELATIVE_ALT &&
                item.frame != MAV_FRAME_GLOBAL_RELATIVE_ALT_INT) {
                authorization = {MissionAuthorizationDecision::DEFER, "UNSUPPORTED_ALTITUDE_REFERENCE"};
                break;
            }
        }
    }
    record.authorization = authorization;
    record.principal = principal;
    record.evaluation_mode = mode;
    record.scenario_id = scenario_id;
    record.current_revision_hash = tracker.current ? tracker.current->hash : "";
    record.canonicalization_latency_us = canonical_us;
    record.mission_hash_latency_us = hash_us;
    record.semantic_delta_latency_us = delta_us;
    record.policy_latency_us = elapsed(start);
    record.decision_latency_us = elapsed(total_start);
    return record;
}
