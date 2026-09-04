#include <cassert>
#include <iostream>
#include <string>

#include <development/mavlink.h>

#include "gateway_ack_reconciliation.h"

namespace {
CanonicalMission mission_at(int32_t latitude) {
    CanonicalMission mission{};
    mission.mission_type = MAV_MISSION_TYPE_MISSION;
    CanonicalMissionItem item{};
    item.command = MAV_CMD_NAV_WAYPOINT;
    item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    item.x = latitude;
    item.y = 85470823;
    item.z = 50.0F;
    item.autocontinue = 1;
    mission.items.push_back(item);
    return mission;
}
}

int main() {
    {
        MissionRevisionTracker revisions{};
        const auto accepted = mission_at(473979578);
        propose_mission_revision(revisions, accepted);
        std::optional<CanonicalMission> committed;
        bool uncertain = false;

        const auto outcome = reconcile_px4_mission_ack(
            MAV_MISSION_ACCEPTED, revisions, accepted, committed, uncertain);

        assert(outcome.gcs_result == MAV_MISSION_ACCEPTED);
        assert(std::string(outcome.event_type) == "revision_committed");
        assert(revisions.current.has_value());
        assert(!revisions.proposed.has_value());
        assert(committed.has_value());
        assert(compute_mission_hash(*committed) == revisions.current->hash);
        assert(!uncertain);
    }

    {
        MissionRevisionTracker revisions{};
        const auto previously_committed = mission_at(473979500);
        propose_mission_revision(revisions, previously_committed);
        assert(commit_proposed_revision(revisions));
        const auto current_hash = revisions.current->hash;
        std::optional<CanonicalMission> committed = previously_committed;
        bool uncertain = false;

        // PX4 accepted an upload, but the corresponding local proposal is
        // unexpectedly absent, forcing commit_proposed_revision() to fail.
        const auto outcome = reconcile_px4_mission_ack(
            MAV_MISSION_ACCEPTED, revisions, mission_at(473979578), committed, uncertain);

        assert(outcome.gcs_result == MAV_MISSION_ERROR);
        assert(std::string(outcome.event_type) == "LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT");
        assert(uncertain);
        assert(!revisions.proposed.has_value());
        assert(revisions.current.has_value());
        assert(revisions.current->hash == current_hash);
        assert(compute_mission_hash(*committed) == current_hash);
    }

    std::cout << "PX4 accepted ACK reconciliation passed\n";
}
