#include "scenarios.h"
#include "structured_events.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const std::string output = argc > 1 ? argv[1] : "evaluation/results/raw/core";
        std::filesystem::create_directories(output);
        EventLog log(output);
        const auto policy = load_runtime_policy("config/sitl_policy.conf");
        const auto principal = resolve_principal(true, "sitl-normal-operator", "NORMAL_OPERATOR");
        std::ofstream manifest(std::filesystem::path(output) / "scenarios.jsonl");
        write_mission(evaluation_mission(), output + "/base.mission");
        bool passed = true;
        for (auto mode : {EvaluationMode::FULL_DRACO, EvaluationMode::NO_DELTA, EvaluationMode::NO_INTENT,
                          EvaluationMode::NO_CAUSALITY, EvaluationMode::NO_FRESH_EVIDENCE, EvaluationMode::NO_CHANGE_BUDGET}) {
            for (const auto& scenario : evaluation_scenarios()) {
                MissionRevisionTracker tracker;
                if (scenario.historical_parent) {
                    propose_mission_revision(tracker, evaluation_mission());
                    commit_proposed_revision(tracker);
                }
                propose_mission_revision(tracker, scenario.starting);
                commit_proposed_revision(tracker);
                const auto parent = scenario.id == "ATTACK_STALE_PARENT" ? tracker.parent->hash : tracker.current->hash;
                MissionProposalRecord active;
                if (scenario.concurrent) {
                    auto rival = evaluation_mission(); rival.items[3].y += 100;
                    active = make_proposal_record(make_mission_revision(tracker.next_id++, rival), tracker.current,
                        principal.principal_id, 0);
                }
                auto evidence = evaluation_evidence(scenario.in_flight);
                const auto record = evaluate_proposal(mission_transaction(scenario.proposed), tracker, parent, principal,
                    policy, evidence, mode, scenario.concurrent ? &active : nullptr, scenario.id);
                auto event = decision_event(record);
                const bool correct = record.authorization.decision == scenario.expected_decision &&
                    record.causality.classification == scenario.expected_causality && record.authorization.reason == scenario.expected_reason;
                put(event, "event_type", "core_scenario_result");
                put(event, "measurement_scope", "CORE_WITH_SYNTHETIC_EVIDENCE");
                put(event, "scenario_class", scenario.classification);
                put(event, "variant", scenario.variant);
                put(event, "expected_outcome", mission_authorization_decision_name(scenario.expected_decision));
                put(event, "expected_reason", scenario.expected_reason);
                put(event, "outcome_correct", correct);
                log.emit(event);
                if (mode == EvaluationMode::FULL_DRACO) {
                    passed = passed && correct;
                    std::cout << scenario.id << ' ' << scenario.variant << ' '
                              << mission_authorization_decision_name(record.authorization.decision) << ' '
                              << record.authorization.reason << " correct=" << correct << '\n';
                    const auto stem = scenario.id + (scenario.variant.empty() ? "" : "_" + scenario.variant);
                    write_mission(scenario.starting, output + "/" + stem + "_starting.mission");
                    write_mission(scenario.proposed, output + "/" + stem + "_proposed.mission");
                    Event entry;
                    put(entry, "scenario_id", scenario.id); put(entry, "variant", scenario.variant);
                    put(entry, "scenario_class", scenario.classification); put(entry, "seed", 0.0);
                    put(entry, "starting_file", stem + "_starting.mission");
                    put(entry, "proposed_file", stem + "_proposed.mission");
                    put(entry, "historical_parent", scenario.historical_parent); put(entry, "concurrent", scenario.concurrent);
                    put(entry, "in_flight", scenario.in_flight);
                    put(entry, "expected_causality", revision_causality_name(scenario.expected_causality));
                    put(entry, "expected_decision", mission_authorization_decision_name(scenario.expected_decision));
                    put(entry, "expected_reason", scenario.expected_reason);
                    manifest << serialize_event(entry) << '\n';
                }
            }
        }
        for (std::size_t size : {10, 50, 100, 500, 1000}) {
            const auto mission = evaluation_mission(size);
            for (int repetition = 0; repetition < 30; ++repetition) {
                MissionRevisionTracker tracker;
                propose_mission_revision(tracker, mission); commit_proposed_revision(tracker);
                auto changed = mission; changed.items[size / 2].y += 100;
                auto evidence = evaluation_evidence();
                const auto record = evaluate_proposal(mission_transaction(changed), tracker, tracker.current->hash,
                    principal, policy, evidence);
                auto event = decision_event(record);
                put(event, "event_type", "scaling_measurement");
                put(event, "measurement_scope", "CORE_WITH_SYNTHETIC_EVIDENCE");
                put(event, "repetition", double(repetition));
                bool alignment = record.delta.changes.size() == 1 && record.delta.changes[0].old_index == size / 2 &&
                    record.delta.changes[0].new_index == size / 2;
                put(event, "alignment_correct", alignment);
                put(event, "delta_correct", record.delta.summary.moved_horizontal == 1 && record.delta.summary.inserted == 0 &&
                    record.delta.summary.deleted == 0 && record.delta.summary.command_changed == 0 &&
                    record.delta.summary.altitude_changed == 0 && !record.delta.summary.destination_changed);
                log.emit(event);
                passed = passed && alignment;
            }
        }
        return passed ? 0 : 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
