#include <cassert>
#include <cstring>
#include <iostream>

#include "phase4_research_freeze.h"


bool contains_text(
    const char* text,
    const char* expected
) {
    return std::strstr(
        text,
        expected
    ) != nullptr;
}


int main() {

    assert(
        PHASE4_RESEARCH_FREEZE_VERSION == 1
    );


    // benign population is frozen
    {
        const auto& benign =
            frozen_benign_scenarios();

        assert(benign.size() == 6);

        assert(
            contains_text(
                benign[0].name,
                "no-op"
            )
        );

        assert(
            contains_text(
                benign[5].name,
                "in-flight"
            )
        );

        std::cout
            << "BENIGN_POPULATION_FROZEN passed"
            << std::endl;
    }


    // adversarial population is frozen
    {
        const auto& attacks =
            frozen_adversarial_scenarios();

        assert(attacks.size() == 7);

        assert(
            contains_text(
                attacks[0].name,
                "rollback"
            )
        );

        assert(
            contains_text(
                attacks[1].name,
                "stale-parent"
            )
        );

        assert(
            contains_text(
                attacks[2].name,
                "conflicting"
            )
        );

        std::cout
            << "ADVERSARIAL_POPULATION_FROZEN passed"
            << std::endl;
    }


    // baselines are frozen
    {
        const auto& baselines =
            frozen_baselines();

        assert(baselines.size() == 5);

        assert(
            contains_text(
                baselines[0].name,
                "plain mavlink"
            )
        );

        assert(
            contains_text(
                baselines[4].name,
                "draco"
            )
        );

        std::cout
            << "BASELINES_FROZEN passed"
            << std::endl;
    }


    // ablations are frozen
    {
        const auto& ablations =
            frozen_ablations();

        assert(ablations.size() == 5);

        assert(
            contains_text(
                ablations[0].name,
                "semantic delta"
            )
        );

        assert(
            contains_text(
                ablations[2].name,
                "causality"
            )
        );

        assert(
            contains_text(
                ablations[3].name,
                "fresh evidence"
            )
        );

        std::cout
            << "ABLATIONS_FROZEN passed"
            << std::endl;
    }


    // metrics are frozen
    {
        const auto& metrics =
            frozen_metrics();

        assert(metrics.size() == 6);

        bool safety_metric_found = false;

        for (const auto& metric : metrics) {

            if (contains_text(
                    metric.name,
                    "committed px4 mission unchanged"
                )) {

                safety_metric_found = true;
            }
        }

        assert(safety_metric_found);

        std::cout
            << "METRICS_FROZEN passed"
            << std::endl;
    }


    // scale points are frozen
    {
        const auto& scale =
            frozen_scale_points();

        assert(scale.size() == 5);

        assert(scale[0] == 10);
        assert(scale[1] == 50);
        assert(scale[2] == 100);
        assert(scale[3] == 500);
        assert(scale[4] == 1000);

        std::cout
            << "SCALE_POINTS_FROZEN passed"
            << std::endl;
    }


    // codex boundary is frozen
    {
        const auto& allowed =
            codex_allowed_handoff_work();

        const auto& forbidden =
            codex_forbidden_research_changes();

        assert(allowed.size() == 7);
        assert(forbidden.size() == 7);

        bool benchmark_allowed = false;
        bool semantics_forbidden = false;

        for (const auto* item : allowed) {

            if (contains_text(
                    item,
                    "benchmark runners"
                )) {

                benchmark_allowed = true;
            }
        }

        for (const auto* item : forbidden) {

            if (contains_text(
                    item,
                    "semantic delta"
                )) {

                semantics_forbidden = true;
            }
        }

        assert(benchmark_allowed);
        assert(semantics_forbidden);

        std::cout
            << "CODEX_HANDOFF_BOUNDARY_FROZEN passed"
            << std::endl;
    }


    std::cout
        << "All Task 11 research-freeze tests passed."
        << std::endl;

    return 0;
}