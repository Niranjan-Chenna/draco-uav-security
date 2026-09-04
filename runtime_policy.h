#pragma once
#include <string>
#include "mission_intent_contract.h"
#include "mission_change_budget.h"

struct RuntimePolicy {
    MissionIntentContract contract;
    MissionChangeBudget budget;
    std::string altitude_reference;
};

// throws on any missing, unknown, duplicate, or invalid configuration value.
RuntimePolicy load_runtime_policy(const std::string& path);
