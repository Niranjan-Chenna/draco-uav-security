#include "mission_change_budget.h"

namespace {

MissionChangeBudgetResult budget_violation(
    MissionAuthorityTier authority,
    const char* reason
) {

    // security admin may override change-budget limits,
    // but hard intent and causality checks are enforced elsewhere
    if (authority ==
        MissionAuthorityTier::SECURITY_ADMIN) {

        return {
            true,
            false,
            "SECURITY_ADMIN_BUDGET_OVERRIDE"
        };
    }

    return {
        false,
        true,
        reason
    };
}

} // namespace


MissionChangeBudgetResult evaluate_change_budget(
    const MissionDelta& delta,
    const MissionChangeBudget& budget,
    MissionAuthorityTier authority
) {

    // an exact semantic no-op consumes no change budget
    if (delta.no_op) {

        return {
            true,
            false,
            "NO_OP"
        };
    }


    if (
        delta.summary.maximum_horizontal_displacement_m >
        budget.maximum_horizontal_change_m
    ) {

        return budget_violation(
            authority,
            "HORIZONTAL_CHANGE_BUDGET_EXCEEDED"
        );
    }


    if (
        delta.summary.maximum_altitude_change_m >
        budget.maximum_altitude_change_m
    ) {

        return budget_violation(
            authority,
            "ALTITUDE_CHANGE_BUDGET_EXCEEDED"
        );
    }


    if (
        delta.summary.inserted >
        budget.maximum_insertions
    ) {

        return budget_violation(
            authority,
            "INSERTION_BUDGET_EXCEEDED"
        );
    }


    if (
        delta.summary.deleted >
        budget.maximum_deletions
    ) {

        return budget_violation(
            authority,
            "DELETION_BUDGET_EXCEEDED"
        );
    }


    if (
        delta.summary.changed_item_ratio >
        budget.maximum_changed_item_ratio
    ) {

        return budget_violation(
            authority,
            "CHANGED_ITEM_RATIO_EXCEEDED"
        );
    }


    if (
        delta.summary.destination_changed &&
        !budget.allow_destination_change
    ) {

        return budget_violation(
            authority,
            "DESTINATION_CHANGE_OUTSIDE_BUDGET"
        );
    }


    return {
        true,
        false,
        "WITHIN_CHANGE_BUDGET"
    };
}