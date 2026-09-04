#pragma once
#include <string>

enum class EvaluationMode {
    FULL_DRACO, NO_DELTA, NO_INTENT, NO_CAUSALITY, NO_FRESH_EVIDENCE, NO_CHANGE_BUDGET
};
EvaluationMode parse_evaluation_mode(const std::string& name, bool evaluation_enabled);
const char* evaluation_mode_name(EvaluationMode mode);
