#include "evaluation_mode.h"
#include <stdexcept>

const char* evaluation_mode_name(EvaluationMode mode) {
    switch (mode) {
        case EvaluationMode::FULL_DRACO: return "FULL_DRACO";
        case EvaluationMode::NO_DELTA: return "ABLATION_NO_DELTA";
        case EvaluationMode::NO_INTENT: return "ABLATION_NO_INTENT";
        case EvaluationMode::NO_CAUSALITY: return "ABLATION_NO_CAUSALITY";
        case EvaluationMode::NO_FRESH_EVIDENCE: return "ABLATION_NO_FRESH_EVIDENCE";
        case EvaluationMode::NO_CHANGE_BUDGET: return "ABLATION_NO_CHANGE_BUDGET";
    }
    return "INVALID";
}
EvaluationMode parse_evaluation_mode(const std::string& name, bool evaluation_enabled) {
    for (auto mode : {EvaluationMode::FULL_DRACO, EvaluationMode::NO_DELTA,
                     EvaluationMode::NO_INTENT, EvaluationMode::NO_CAUSALITY,
                     EvaluationMode::NO_FRESH_EVIDENCE, EvaluationMode::NO_CHANGE_BUDGET}) {
        if (name == evaluation_mode_name(mode)) {
            if (!evaluation_enabled && mode != EvaluationMode::FULL_DRACO)
                throw std::runtime_error("ablation requires explicit evaluation mode");
            return mode;
        }
    }
    throw std::runtime_error("unknown evaluation mode");
}
