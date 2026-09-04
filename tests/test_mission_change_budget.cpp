#include <cassert>
#include <iostream>

#include "mission_change_budget.h"

int main() {

    MissionChangeBudget budget{};


    // no-op
    {
        MissionDelta delta{};
        delta.no_op = true;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(result.within_budget);
        assert(!result.requires_higher_authority);

        std::cout << "NO_OP passed" << std::endl;
    }


    // small horizontal correction
    {
        MissionDelta delta{};

        delta.summary.maximum_horizontal_displacement_m =
            50.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(result.within_budget);

        std::cout
            << "SMALL_HORIZONTAL_CHANGE passed"
            << std::endl;
    }


    // excessive horizontal correction
    {
        MissionDelta delta{};

        delta.summary.maximum_horizontal_displacement_m =
            150.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "LARGE_HORIZONTAL_CHANGE passed"
            << std::endl;
    }


    // altitude change within budget
    {
        MissionDelta delta{};

        delta.summary.maximum_altitude_change_m =
            20.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(result.within_budget);

        std::cout
            << "SMALL_ALTITUDE_CHANGE passed"
            << std::endl;
    }


    // altitude change above budget
    {
        MissionDelta delta{};

        delta.summary.maximum_altitude_change_m =
            50.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "LARGE_ALTITUDE_CHANGE passed"
            << std::endl;
    }


    // insertion within budget
    {
        MissionDelta delta{};

        delta.summary.inserted = 2;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(result.within_budget);

        std::cout
            << "INSERTION_WITHIN_BUDGET passed"
            << std::endl;
    }


    // too many insertions
    {
        MissionDelta delta{};

        delta.summary.inserted = 3;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "INSERTION_BUDGET_EXCEEDED passed"
            << std::endl;
    }


    // too many deletions
    {
        MissionDelta delta{};

        delta.summary.deleted = 3;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "DELETION_BUDGET_EXCEEDED passed"
            << std::endl;
    }


    // changed item ratio above budget
    {
        MissionDelta delta{};

        delta.summary.changed_item_ratio = 0.75;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "CHANGED_RATIO_EXCEEDED passed"
            << std::endl;
    }


    // destination change blocked for normal operator
    {
        MissionDelta delta{};

        delta.summary.destination_changed = true;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::NORMAL_OPERATOR
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "DESTINATION_REQUIRES_AUTHORITY passed"
            << std::endl;
    }


    // emergency authority must not become unlimited
    {
        MissionDelta delta{};

        delta.summary.maximum_horizontal_displacement_m =
            500.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::EMERGENCY_AUTHORITY
            );

        assert(!result.within_budget);
        assert(result.requires_higher_authority);

        std::cout
            << "EMERGENCY_NOT_UNLIMITED passed"
            << std::endl;
    }


    // security admin may override budget limits
    {
        MissionDelta delta{};

        delta.summary.maximum_horizontal_displacement_m =
            500.0;

        auto result =
            evaluate_change_budget(
                delta,
                budget,
                MissionAuthorityTier::SECURITY_ADMIN
            );

        assert(result.within_budget);
        assert(!result.requires_higher_authority);

        std::cout
            << "SECURITY_ADMIN_OVERRIDE passed"
            << std::endl;
    }


    // evaluator must not modify mission delta
    {
        MissionDelta delta{};

        delta.summary.inserted = 4;
        delta.summary.maximum_altitude_change_m = 60.0;

        const std::size_t original_inserted =
            delta.summary.inserted;

        const double original_altitude =
            delta.summary.maximum_altitude_change_m;

        evaluate_change_budget(
            delta,
            budget,
            MissionAuthorityTier::NORMAL_OPERATOR
        );

        assert(
            delta.summary.inserted ==
            original_inserted
        );

        assert(
            delta.summary.maximum_altitude_change_m ==
            original_altitude
        );

        std::cout
            << "DELTA_IMMUTABLE passed"
            << std::endl;
    }


    std::cout
        << "All Task 10 change-budget tests passed."
        << std::endl;

    return 0;
}