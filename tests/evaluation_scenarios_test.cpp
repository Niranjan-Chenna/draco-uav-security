#include <cassert>
#include <iostream>
#include "evaluation/scenarios.h"

int main() {
    const auto policy = load_runtime_policy("config/sitl_policy.conf");
    const auto principal = resolve_principal(true, "sitl-test", "NORMAL_OPERATOR");
    for (const auto& scenario : evaluation_scenarios()) {
        MissionRevisionTracker tracker;
        if (scenario.historical_parent) {
            propose_mission_revision(tracker, evaluation_mission());
            assert(commit_proposed_revision(tracker));
        }
        propose_mission_revision(tracker, scenario.starting);
        assert(commit_proposed_revision(tracker));
        const auto parent = scenario.id == "ATTACK_STALE_PARENT" ? tracker.parent->hash : tracker.current->hash;
        MissionProposalRecord active;
        if (scenario.concurrent) {
            auto rival = evaluation_mission(); rival.items[3].y += 100;
            active = make_proposal_record(make_mission_revision(tracker.next_id++, rival), tracker.current, principal.principal_id, 0);
        }
        auto evidence = evaluation_evidence(scenario.in_flight);
        auto record = evaluate_proposal(mission_transaction(scenario.proposed), tracker, parent,
            principal, policy, evidence, EvaluationMode::FULL_DRACO, scenario.concurrent ? &active : nullptr);
        assert(record.authorization.decision == scenario.expected_decision);
        assert(record.authorization.reason == scenario.expected_reason);
        assert(record.causality.classification == scenario.expected_causality);
        if (scenario.variant == "insert") assert(record.delta.summary.inserted == 1);
        if (scenario.variant == "delete") assert(record.delta.summary.deleted == 1);
        for (auto mode : {EvaluationMode::NO_DELTA, EvaluationMode::NO_INTENT, EvaluationMode::NO_CAUSALITY,
                          EvaluationMode::NO_FRESH_EVIDENCE, EvaluationMode::NO_CHANGE_BUDGET}) {
            record = evaluate_proposal(mission_transaction(scenario.proposed), tracker, parent,
                principal, policy, evidence, mode, scenario.concurrent ? &active : nullptr);
            assert(record.evaluation_mode == mode);
            if (mode == EvaluationMode::NO_DELTA) assert(record.delta.changes.empty());
            if (mode == EvaluationMode::NO_INTENT && scenario.id == "ATTACK_OUTSIDE_INTENT")
                assert(record.authorization.decision == MissionAuthorizationDecision::ALLOW);
            if (mode == EvaluationMode::NO_CAUSALITY && scenario.id == "ATTACK_SEMANTIC_ROLLBACK")
                assert(record.authorization.decision == MissionAuthorizationDecision::ALLOW);
            if (mode == EvaluationMode::NO_CHANGE_BUDGET)
                assert(std::string(record.change_budget.reason) == "EVALUATION_CHANGE_BUDGET_DISABLED");
        }
    }
    auto mission = evaluation_mission();
    MissionRevisionTracker tracker;
    StateCache absent;
    auto record = evaluate_proposal(mission_transaction(mission), tracker, "", principal, policy, absent);
    assert(record.authorization.decision == MissionAuthorizationDecision::DEFER);
    record = evaluate_proposal(mission_transaction(mission), tracker, "", principal, policy, absent, EvaluationMode::NO_FRESH_EVIDENCE);
    assert(!record.evidence_usable && record.authorization.decision == MissionAuthorizationDecision::ALLOW);
    std::cout << "all frozen scenarios and explicit ablations passed\n";
}
