#pragma once

#include <cstdint>
#include <optional>

#include "canonical_mission.h"
#include "mission_revision_tracker.h"

struct MissionAckReconciliation {
    uint8_t gcs_result{};
    const char* event_type{};
};

MissionAckReconciliation reconcile_px4_mission_ack(
    uint8_t px4_result,
    MissionRevisionTracker& revisions,
    const CanonicalMission& accepted_mission,
    std::optional<CanonicalMission>& committed_mission,
    bool& px4_state_uncertain
);
