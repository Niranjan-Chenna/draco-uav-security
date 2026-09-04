#pragma once
#include "mission_decision_record.h"
#include "runtime_policy.h"

MissionDecisionRecord evaluate_proposal(
    const MissionUploadTransaction& upload, MissionRevisionTracker& tracker,
    const std::string& expected_parent_hash, const PrincipalContext& principal,
    const RuntimePolicy& policy, StateCache& evidence,
    EvaluationMode mode = EvaluationMode::FULL_DRACO,
    const MissionProposalRecord* active_proposal = nullptr,
    const std::string& scenario_id = "");
