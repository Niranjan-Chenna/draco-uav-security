#include <cassert>
#include <iostream>

#include "mission_revision_causality.h"

CanonicalMissionItem make_waypoint(
    uint16_t seq,
    int32_t lat,
    int32_t lon,
    float altitude
) {
    CanonicalMissionItem item{};

    item.seq = seq;
    item.command =
        MAV_CMD_NAV_WAYPOINT;

    item.frame =
        MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;

    item.x = lat;
    item.y = lon;
    item.z = altitude;

    item.autocontinue = 1;

    return item;
}

CanonicalMission make_mission_a() {
    CanonicalMission mission{};

    mission.items = {
        make_waypoint(
            0,
            473970000,
            85470000,
            50
        ),
        make_waypoint(
            1,
            473980000,
            85471000,
            50
        ),
        make_waypoint(
            2,
            473990000,
            85472000,
            50
        )
    };

    return mission;
}

int main() {

    MissionRevisionTracker tracker{};

    CanonicalMission mission_a =
        make_mission_a();


    // initial mission

    MissionRevision initial_revision =
        make_mission_revision(
            tracker.next_id++,
            mission_a
        );

    MissionDelta empty_delta{};

    MissionProposalRecord initial =
        make_proposal_record(
            initial_revision,
            tracker.current,
            "gcs-normal",
            1000
        );

    auto result =
        classify_revision_causality(
            tracker,
            initial,
            empty_delta
        );

    assert(
        result.classification ==
        RevisionCausalityClass::INITIAL_MISSION
    );

    std::cout
        << "INITIAL_MISSION passed\n";


    // commit mission A

    tracker.proposed =
        initial_revision;

    assert(
        commit_proposed_revision(tracker)
    );


    // exact re-upload

    MissionRevision same_revision =
        make_mission_revision(
            tracker.next_id++,
            mission_a
        );

    MissionProposalRecord same =
        make_proposal_record(
            same_revision,
            tracker.current,
            "gcs-normal",
            2000
        );

    MissionDelta same_delta =
        compute_mission_delta(
            tracker.current->mission,
            mission_a
        );

    result =
        classify_revision_causality(
            tracker,
            same,
            same_delta
        );

    assert(
        result.classification ==
        RevisionCausalityClass::NO_OP_REUPLOAD
    );

    std::cout
        << "NO_OP_REUPLOAD passed\n";


    // normal child revision

    CanonicalMission mission_b =
        mission_a;

    mission_b.items[1].x += 5000;

    MissionRevision revision_b =
        make_mission_revision(
            tracker.next_id++,
            mission_b
        );

    MissionProposalRecord proposal_b =
        make_proposal_record(
            revision_b,
            tracker.current,
            "gcs-normal",
            3000
        );

    MissionDelta delta_b =
        compute_mission_delta(
            tracker.current->mission,
            mission_b
        );

    result =
        classify_revision_causality(
            tracker,
            proposal_b,
            delta_b
        );

    assert(
        result.classification ==
        RevisionCausalityClass::NORMAL_CHILD
    );

    std::cout
        << "NORMAL_CHILD passed\n";


    // commit B so A becomes history

    tracker.proposed =
        revision_b;

    assert(
        commit_proposed_revision(tracker)
    );


    // fresh transaction containing old mission A

    MissionRevision rollback_revision =
        make_mission_revision(
            tracker.next_id++,
            mission_a
        );

    MissionProposalRecord rollback =
        make_proposal_record(
            rollback_revision,
            tracker.current,
            "gcs-normal",
            4000
        );

    MissionDelta rollback_delta =
        compute_mission_delta(
            tracker.current->mission,
            mission_a
        );

    result =
        classify_revision_causality(
            tracker,
            rollback,
            rollback_delta
        );

    assert(
        result.classification ==
        RevisionCausalityClass::ROLLBACK
    );

    std::cout
        << "ROLLBACK passed\n";


    // stale parent proposal

    CanonicalMission mission_c =
        mission_b;

    mission_c.items[2].z = 70;

    MissionRevision revision_c =
        make_mission_revision(
            tracker.next_id++,
            mission_c
        );

    MissionProposalRecord stale =
        make_proposal_record(
            revision_c,
            tracker.parent,
            "gcs-normal",
            5000
        );

    MissionDelta delta_c =
        compute_mission_delta(
            tracker.current->mission,
            mission_c
        );

    result =
        classify_revision_causality(
            tracker,
            stale,
            delta_c
        );

    assert(
        result.classification ==
        RevisionCausalityClass::STALE_PARENT
    );

    std::cout
        << "STALE_PARENT passed\n";


    // concurrent conflicting proposals

    MissionProposalRecord active =
        make_proposal_record(
            revision_c,
            tracker.current,
            "gcs-one",
            6000
        );

    CanonicalMission mission_d =
        mission_b;

    mission_d.items[1].z = 90;

    MissionRevision revision_d =
        make_mission_revision(
            tracker.next_id++,
            mission_d
        );

    MissionProposalRecord concurrent =
        make_proposal_record(
            revision_d,
            tracker.current,
            "gcs-two",
            6001
        );

    MissionDelta delta_d =
        compute_mission_delta(
            tracker.current->mission,
            mission_d
        );

    result =
        classify_revision_causality(
            tracker,
            concurrent,
            delta_d,
            &active
        );

    assert(
        result.classification ==
        RevisionCausalityClass::CONCURRENT_CONFLICT
    );

    std::cout
        << "CONCURRENT_CONFLICT passed\n";


    // complete unrelated replacement

    CanonicalMission replacement =
        tracker.current->mission;

    replacement.items[0].x += 100000;
    replacement.items[1].x += 100000;
    replacement.items[2].x += 100000;

    MissionRevision replacement_revision =
        make_mission_revision(
            tracker.next_id++,
            replacement
        );

    MissionProposalRecord replacement_proposal =
        make_proposal_record(
            replacement_revision,
            tracker.current,
            "gcs-normal",
            7000
        );

    MissionDelta replacement_delta =
        compute_mission_delta(
            tracker.current->mission,
            replacement
        );

    result =
        classify_revision_causality(
            tracker,
            replacement_proposal,
            replacement_delta
        );

    assert(
        result.classification ==
        RevisionCausalityClass::UNRELATED_REPLACEMENT
    );

    std::cout
        << "UNRELATED_REPLACEMENT passed\n";


    std::cout
        << "\nAll Task 7 causality tests passed."
        << std::endl;

    return 0;
}