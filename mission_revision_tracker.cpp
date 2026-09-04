#include "mission_revision_tracker.h"

void propose_mission_revision(
    MissionRevisionTracker& tracker,
    const CanonicalMission& mission
) {
    tracker.proposed =
        make_mission_revision(
            tracker.next_id,
            mission
        );

    ++tracker.next_id;
}

bool commit_proposed_revision(
    MissionRevisionTracker& tracker
) {
    if (!tracker.proposed.has_value()) {
        return false;
    }

    if (tracker.current.has_value()) {

        // preserve the old committed revision
        // before replacing it
        tracker.history.push_back(
            tracker.current.value()
        );

        tracker.parent =
            tracker.current;
    }


    tracker.current =
        tracker.proposed;

    tracker.proposed.reset();

    return true;
}

void reject_proposed_revision(
    MissionRevisionTracker& tracker
) {
    tracker.proposed.reset();
}

bool historical_content_exists(
    const MissionRevisionTracker& tracker,
    const std::string& hash
) {
    for (const auto& revision :
         tracker.history) {

        if (revision.hash == hash) {
            return true;
        }
    }

    return false;
}