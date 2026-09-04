#include "mission_decision_record.h"

MissionDecisionRecord make_mission_decision_record(
    const MissionProposalRecord& proposal,
    const MissionDelta& delta,
    const MissionChangeBudgetResult& change_budget,
    const RevisionCausalityResult& causality,
    MissionAuthorityTier authority,
    bool vehicle_in_flight,
    const MissionIntentContract& contract,
    const MissionChangeBudget& budget_policy,
    const StateCache& state_cache,
    const MissionAuthorizationResult& authorization,
    uint64_t decision_time_ms
) {

    MissionDecisionRecord record{};

    record.proposal = proposal;

    record.contract_id =
        contract.contract_id;

    record.contract_version =
        contract.version;

    record.change_budget_policy_id =
        budget_policy.policy_id;

    record.change_budget_policy_version =
        budget_policy.version;

    record.delta = delta;
    record.change_budget = change_budget;
    record.causality = causality;
    record.authority = authority;

    record.evidence =
        make_evidence_snapshot(state_cache);

    record.evidence_usable =
        evidence_is_usable(
            record.evidence.state.armed
        ) &&
        evidence_is_usable(
            record.evidence.state.landed_state
        ) &&
        evidence_is_usable(
            record.evidence.state.global_position
        );

    record.vehicle_in_flight =
        vehicle_in_flight;

    record.authorization =
        authorization;

    record.decision_time_ms =
        decision_time_ms;

    return record;
}