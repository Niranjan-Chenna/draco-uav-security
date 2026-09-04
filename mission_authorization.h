#pragma once

#include <cstdint>

#include "canonical_mission.h"
#include "mission_delta.h"
#include "mission_intent_contract.h"
#include "mission_revision_causality.h"
#include "mission_change_budget.h"

enum class MissionAuthorizationDecision {
    ALLOW,
    DENY,
    DEFER,
    REQUIRE_HIGHER_AUTHORITY
};

struct MissionAuthorizationResult {
    MissionAuthorizationDecision decision{
        MissionAuthorizationDecision::DEFER
    };

    const char* reason{"UNSET"};
};

MissionAuthorizationResult evaluate_mission_authorization(
    const CanonicalMission& proposed_mission,
    const MissionDelta& delta,
    const RevisionCausalityResult& causality,
    const MissionChangeBudgetResult& change_budget,
    const MissionIntentContract& contract,
    MissionAuthorityTier authority,
    bool vehicle_in_flight,
    uint64_t current_unix_ms
);

const char* mission_authorization_decision_name(
    MissionAuthorizationDecision decision
);