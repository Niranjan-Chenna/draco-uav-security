#pragma once

#include <cstdint>

#include "mission_authorization.h"
#include "mission_delta.h"
#include "mission_revision_causality.h"
#include "state_cache.h"
#include "mission_change_budget.h"

struct MissionDecisionRecord {
    MissionProposalRecord proposal;
    uint64_t contract_id{0};
    uint32_t contract_version{0};

    uint64_t change_budget_policy_id{0};
    uint32_t change_budget_policy_version{0};
    MissionDelta delta;
     MissionChangeBudgetResult change_budget;
    RevisionCausalityResult causality;

    MissionAuthorityTier authority{
        MissionAuthorityTier::NORMAL_OPERATOR
    };

    EvidenceSnapshot evidence;
    bool evidence_usable{false};
    bool vehicle_in_flight{false};
    MissionAuthorizationResult authorization;
   
    uint64_t decision_time_ms{0};
    
}; 

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
);
