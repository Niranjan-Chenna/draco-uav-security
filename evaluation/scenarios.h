#pragma once
#include <string>
#include <vector>
#include "mission_pipeline.h"

struct Scenario {
    std::string id, variant, classification;
    CanonicalMission starting, proposed;
    RevisionCausalityClass expected_causality;
    MissionAuthorizationDecision expected_decision;
    std::string expected_reason;
    bool historical_parent{false}, concurrent{false}, in_flight{false};
};
CanonicalMission evaluation_mission(std::size_t size = 10);
MissionUploadTransaction mission_transaction(const CanonicalMission& mission);
std::vector<Scenario> evaluation_scenarios();
void write_mission(const CanonicalMission& mission, const std::string& path);
CanonicalMission read_mission(const std::string& path);
StateCache evaluation_evidence(bool in_flight = false);
