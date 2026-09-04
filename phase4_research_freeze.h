#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

constexpr uint32_t PHASE4_RESEARCH_FREEZE_VERSION = 1;

// frozen experiment scenario
struct FrozenScenario {
    const char* id;
    const char* name;
};

// frozen baseline
struct FrozenBaseline {
    const char* id;
    const char* name;
};

// frozen ablation
struct FrozenAblation {
    const char* id;
    const char* name;
};

// frozen metric group
struct FrozenMetric {
    const char* id;
    const char* name;
};

// task 11 frozen benign population
const std::array<FrozenScenario, 6>&
frozen_benign_scenarios();

// task 11 frozen adversarial population
const std::array<FrozenScenario, 7>&
frozen_adversarial_scenarios();

// task 11 frozen baselines
const std::array<FrozenBaseline, 5>&
frozen_baselines();

// task 11 frozen ablations
const std::array<FrozenAblation, 5>&
frozen_ablations();

// task 11 frozen metric groups
const std::array<FrozenMetric, 6>&
frozen_metrics();

// frozen mission-size scale points
const std::array<std::size_t, 5>&
frozen_scale_points();

// work permitted after the research freeze
const std::array<const char*, 7>&
evaluation_allowed_handoff_work();

// semantics that evaluation work must not redefine
const std::array<const char*, 7>&
evaluation_forbidden_research_changes();
