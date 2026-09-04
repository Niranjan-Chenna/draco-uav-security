#pragma once
#include <string>
#include "principal_context.h"
#include "evaluation_mode.h"

struct GatewayOptions {
    std::string policy_path;
    std::string results_directory{"evaluation/results/raw"};
    std::string evaluation_context_directory;
    PrincipalContext principal;
    EvaluationMode mode{EvaluationMode::FULL_DRACO};
    uint16_t gcs_port{14560};
    uint16_t px4_local_port{14550};
    uint16_t px4_remote_port{18570};
    unsigned evaluation_upload_delay_ms{0};
};
int run_gateway(const GatewayOptions& options);
