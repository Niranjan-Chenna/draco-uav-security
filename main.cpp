#include <iostream>
#include <stdexcept>
#include "gateway_options.h"

int main(int argc, char** argv) {
    try {
        GatewayOptions options;
        bool evaluation = false;
        std::string id, authority, mode = "FULL_DRACO";
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "draco --policy FILE [--results DIR] [--evaluation --principal ID --authority TIER]\n"
                    "      [--mode FULL_DRACO|ABLATION_NO_DELTA|ABLATION_NO_INTENT|ABLATION_NO_CAUSALITY|ABLATION_NO_FRESH_EVIDENCE|ABLATION_NO_CHANGE_BUDGET]\n"
                    "      [--evaluation-context DIR] [--evaluation-upload-delay-ms N]\n"
                    "      [--gcs-port N --px4-local-port N --px4-remote-port N]\n";
                return 0;
            }
            if (arg == "--evaluation") { evaluation = true; continue; }
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            std::string value = argv[++i];
            if (arg == "--policy") options.policy_path = value;
            else if (arg == "--results") options.results_directory = value;
            else if (arg == "--evaluation-context") options.evaluation_context_directory = value;
            else if (arg == "--principal") id = value;
            else if (arg == "--authority") authority = value;
            else if (arg == "--mode") mode = value;
            else if (arg == "--evaluation-upload-delay-ms") {
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("invalid evaluation transport delay");
                auto delay = std::stoul(value);
                if (delay > 5000) throw std::runtime_error("evaluation transport delay exceeds 5000 ms");
                options.evaluation_upload_delay_ms = delay;
            }
            else if (arg == "--gcs-port" || arg == "--px4-local-port" || arg == "--px4-remote-port") {
                if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("invalid port");
                auto port = std::stoul(value);
                if (port == 0 || port > 65535) throw std::runtime_error("port out of range");
                if (arg == "--gcs-port") options.gcs_port = port;
                else if (arg == "--px4-local-port") options.px4_local_port = port;
                else options.px4_remote_port = port;
            } else throw std::runtime_error("unknown option: " + arg);
        }
        if (options.policy_path.empty()) throw std::runtime_error("explicit --policy FILE is required");
        if (!evaluation && !options.evaluation_context_directory.empty())
            throw std::runtime_error("evaluation context requires --evaluation");
        if (!evaluation && options.evaluation_upload_delay_ms)
            throw std::runtime_error("evaluation transport delay requires --evaluation");
        options.principal = resolve_principal(evaluation, id, authority);
        options.mode = parse_evaluation_mode(mode, evaluation);
        return run_gateway(options);
    } catch (const std::exception& error) {
        std::cerr << "CONFIGURATION_OR_RUNTIME_FAILURE: " << error.what() << '\n';
        return 1;
    }
}
