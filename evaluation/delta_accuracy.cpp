#include "scenarios.h"
#include "structured_events.h"
#include <iostream>
#include <set>

int main(int argc, char** argv) {
    EventLog log(argc > 1 ? argv[1] : "evaluation/results/raw/delta_accuracy");
    const auto base = evaluation_mission();
    using T = MissionDeltaType;
    struct Case { std::string name; CanonicalMission proposed; std::set<T> expected; int alignment_index; };
    std::vector<Case> cases;
    auto add = [&](std::string name, CanonicalMission mission, std::set<T> expected, int index = -1) {
        for (std::size_t i = 0; i < mission.items.size(); ++i) mission.items[i].seq = i;
        cases.push_back({name, mission, expected, index});
    };
    add("no_op", base, {T::NO_OP});
    auto mission = base; auto inserted = base.items[3]; inserted.x += 200;
    mission.items.insert(mission.items.begin() + 4, inserted);
    add("insertion", mission, {T::INSERT});
    mission = base; mission.items.erase(mission.items.begin() + 3);
    add("deletion", mission, {T::DELETE});
    mission = base; mission.items[3].z += 5;
    add("altitude", mission, {T::ALTITUDE_CHANGE}, 3);
    mission = base; mission.items[3].x += 100;
    add("horizontal", mission, {T::MOVE_HORIZONTAL}, 3);
    mission = base; mission.items[3].command = MAV_CMD_DO_SET_SERVO;
    add("command", mission, {T::COMMAND_CHANGE}, 3);
    mission = base; mission.items[3].param1 = 3;
    add("parameter", mission, {T::PARAMETER_CHANGE}, 3);
    mission = base; std::swap(mission.items[3], mission.items[4]);
    add("reorder", mission, {T::REORDER});
    mission = base; mission.items.back().x += 100;
    add("destination", mission, {T::MOVE_HORIZONTAL, T::DESTINATION_CHANGE}, 9);
    mission = base; for (auto& item : mission.items) item.x += 5000;
    add("major_replacement", mission, {T::MOVE_HORIZONTAL, T::DESTINATION_CHANGE, T::MAJOR_REPLACEMENT});
    int true_positive = 0, false_positive = 0, false_negative = 0, alignment_total = 0, alignment_correct = 0;
    bool passed = true;
    for (const auto& fixture : cases) {
        const auto delta = compute_mission_delta(base, fixture.proposed);
        std::set<T> observed;
        if (delta.no_op) observed.insert(T::NO_OP);
        for (const auto& change : delta.changes) observed.insert(change.types.begin(), change.types.end());
        for (auto type : fixture.expected) {
            if (observed.count(type)) ++true_positive; else ++false_negative;
        }
        for (auto type : observed) if (!fixture.expected.count(type)) ++false_positive;
        bool aligned = false;
        if (fixture.alignment_index >= 0) {
            ++alignment_total;
            for (const auto& change : delta.changes)
                if (change.has_old_item && change.has_new_item && change.old_index == std::size_t(fixture.alignment_index) &&
                    change.new_index == std::size_t(fixture.alignment_index)) aligned = true;
            alignment_correct += aligned;
        }
        Event event;
        put(event, "event_type", "delta_labeled_case"); put(event, "measurement_scope", "LABELED_DELTA_FIXTURES");
        put(event, "case", fixture.name); put(event, "delta_correct", observed == fixture.expected);
        event["alignment_correct"] = fixture.alignment_index >= 0 ? (aligned ? "true" : "false") : "null";
        log.emit(event);
        passed = passed && observed == fixture.expected && (fixture.alignment_index < 0 || aligned);
    }
    Event summary;
    put(summary, "event_type", "delta_accuracy_summary"); put(summary, "measurement_scope", "LABELED_DELTA_FIXTURES");
    put(summary, "cases", double(cases.size())); put(summary, "true_positive_labels", double(true_positive));
    put(summary, "false_positive_labels", double(false_positive)); put(summary, "false_negative_labels", double(false_negative));
    double precision = double(true_positive) / (true_positive + false_positive);
    double recall = double(true_positive) / (true_positive + false_negative);
    put(summary, "precision", precision); put(summary, "recall", recall);
    put(summary, "f1", 2 * precision * recall / (precision + recall));
    put(summary, "alignment_checks", double(alignment_total)); put(summary, "alignment_correct", double(alignment_correct));
    log.emit(summary);
    std::cout << serialize_event(summary) << '\n';
    return passed ? 0 : 1;
}
