#include "phase4_research_freeze.h"

const std::array<FrozenScenario, 6>&
frozen_benign_scenarios() {
    static const std::array<FrozenScenario, 6> scenarios{{
        {"BENIGN_NO_OP", "exact re-upload / no-op"},
        {"BENIGN_SMALL_CORRECTION", "small operator correction"},
        {"BENIGN_INSERT_DELETE", "safe waypoint insertion/deletion"},
        {"BENIGN_ALTITUDE_CORRECTION", "altitude correction within intent"},
        {"BENIGN_DETOUR", "legitimate obstacle/weather detour inside contract"},
        {"BENIGN_IN_FLIGHT_REPLAN", "legitimate in-flight replan permitted by policy"}
    }};
    return scenarios;
}

const std::array<FrozenScenario, 7>&
frozen_adversarial_scenarios() {
    static const std::array<FrozenScenario, 7> scenarios{{
        {"ATTACK_SEMANTIC_ROLLBACK", "freshly signed old-mission rollback"},
        {"ATTACK_STALE_PARENT", "stale-parent proposal"},
        {"ATTACK_CONCURRENT_CONFLICT", "concurrent conflicting gcs proposals"},
        {"ATTACK_DESTINATION_DIVERSION", "destination diversion inside geofence"},
        {"ATTACK_COMMAND_SUBSTITUTION", "land / command substitution"},
        {"ATTACK_MAJOR_REPLACEMENT", "major route replacement"},
        {"ATTACK_OUTSIDE_INTENT", "outside-intent edit"}
    }};
    return scenarios;
}

const std::array<FrozenBaseline, 5>&
frozen_baselines() {
    static const std::array<FrozenBaseline, 5> baselines{{
        {"BASELINE_A", "plain mavlink"},
        {"BASELINE_B", "mavlink signing"},
        {"BASELINE_C", "signing plus native px4 feasibility/geofence"},
        {"BASELINE_D", "closest stateful-proxy concept from related work"},
        {"BASELINE_E", "full draco"}
    }};
    return baselines;
}

const std::array<FrozenAblation, 5>&
frozen_ablations() {
    static const std::array<FrozenAblation, 5> ablations{{
        {"ABLATION_NO_DELTA", "draco without semantic delta"},
        {"ABLATION_NO_INTENT", "draco without intent contract"},
        {"ABLATION_NO_CAUSALITY", "draco without causality/history"},
        {"ABLATION_NO_FRESH_EVIDENCE", "draco without fresh evidence binding"},
        {"ABLATION_NO_CHANGE_BUDGET", "draco without change budgets / terminal-objective policy"}
    }};
    return ablations;
}

const std::array<FrozenMetric, 6>&
frozen_metrics() {
    static const std::array<FrozenMetric, 6> metrics{{
        {"METRIC_DELTA", "delta precision recall f1 and alignment accuracy"},
        {"METRIC_AUTHORIZATION", "malicious blocked false negatives legitimate allowed false positives"},
        {"METRIC_PERFORMANCE", "canonicalization hash delta policy and end-to-end latency p50 p95 p99"},
        {"METRIC_SCALE", "mission-size scaling"},
        {"METRIC_CONSISTENCY", "stale-parent rollback conflict and restart/reconciliation success"},
        {"METRIC_PX4_UNCHANGED", "every deny or defer leaves previously committed px4 mission unchanged"}
    }};
    return metrics;
}

const std::array<std::size_t, 5>&
frozen_scale_points() {
    static const std::array<std::size_t, 5> points{{10, 50, 100, 500, 1000}};
    return points;
}

const std::array<const char*, 7>&
evaluation_allowed_handoff_work() {
    static const std::array<const char*, 7> work{{
        "attack generators",
        "benign mission generators",
        "benchmark runners",
        "csv/json logging",
        "graph and table scripts",
        "repeatable experiment harnesses",
        "gui packaging and documentation"
    }};
    return work;
}

const std::array<const char*, 7>&
evaluation_forbidden_research_changes() {
    static const std::array<const char*, 7> forbidden{{
        "semantic delta meaning",
        "mission intent contract meaning",
        "rollback and conflict rules",
        "authorization decision semantics",
        "change budget semantics",
        "threat model",
        "novelty claims"
    }};
    return forbidden;
}
