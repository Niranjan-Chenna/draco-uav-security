#include "gateway_ack_reconciliation.h"

#include <development/mavlink.h>

MissionAckReconciliation reconcile_px4_mission_ack(
    uint8_t px4_result,
    MissionRevisionTracker& revisions,
    const CanonicalMission& accepted_mission,
    std::optional<CanonicalMission>& committed_mission,
    bool& px4_state_uncertain
) {
    if (px4_result == MAV_MISSION_ACCEPTED) {
        if (commit_proposed_revision(revisions)) {
            committed_mission = accepted_mission;
            return {MAV_MISSION_ACCEPTED, "revision_committed"};
        }

        reject_proposed_revision(revisions);
        px4_state_uncertain = true;
        return {MAV_MISSION_ERROR, "LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT"};
    }

    reject_proposed_revision(revisions);
    return {px4_result, "revision_rejected"};
}
