#pragma once
#include <cstdint>
#include <cstddef>

#include "mission_delta.h"
#include "mission_intent_contract.h"

struct MissionChangeBudget {
    uint64_t policy_id{1};
    uint32_t version{1};

    double maximum_horizontal_change_m{100.0};
    double maximum_altitude_change_m{30.0};

    std::size_t maximum_insertions{2};
    std::size_t maximum_deletions{2};

    double maximum_changed_item_ratio{0.5};

    bool allow_destination_change{false};
};
struct MissionChangeBudgetResult {
    bool within_budget{false};
    bool requires_higher_authority{false};

    const char* reason{"UNSET"};
};

MissionChangeBudgetResult evaluate_change_budget(
    const MissionDelta& delta,
    const MissionChangeBudget& budget,
    MissionAuthorityTier authority
);