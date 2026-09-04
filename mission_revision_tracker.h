#pragma once

#include <optional>
#include <vector>

#include "mission_revision.h"

struct MissionRevisionTracker {
    std::optional<MissionRevision> parent;
    std::optional<MissionRevision> current;
    std::optional<MissionRevision> proposed;

    // stores superseded committed revisions
    std::vector<MissionRevision> history;

    MissionRevisionId next_id{1};
};

void propose_mission_revision(
    MissionRevisionTracker& tracker,
    const CanonicalMission& mission
);

bool commit_proposed_revision(
    MissionRevisionTracker& tracker
);

void reject_proposed_revision(
    MissionRevisionTracker& tracker
);

bool historical_content_exists(
    const MissionRevisionTracker& tracker,
    const std::string& hash
);