#include "mission_revision_causality.h"

MissionProposalRecord make_proposal_record(
    const MissionRevision& revision,
    const std::optional<MissionRevision>& expected_parent,
    const std::string& proposer_principal,
    uint64_t proposal_time_ms
) {
    MissionProposalRecord record{};

    record.revision =
        revision;

    if (expected_parent.has_value()) {
        record.expected_parent_hash =
            expected_parent->hash;
    }

    record.proposer_principal =
        proposer_principal;

    record.proposal_time_ms =
        proposal_time_ms;

    record.state =
        ProposalState::PROPOSED;

    return record;
}

RevisionCausalityResult classify_revision_causality(
    const MissionRevisionTracker& tracker,
    const MissionProposalRecord& proposal,
    const MissionDelta& delta,
    const MissionProposalRecord* active_proposal
) {
    RevisionCausalityResult result{};

    result.major_replacement =
        delta.summary.major_replacement;

    // there is no committed mission yet
    if (!tracker.current.has_value()) {
        result.classification =
            RevisionCausalityClass::INITIAL_MISSION;

        return result;
    }

    const MissionRevision& current =
        tracker.current.value();

    result.parent_matches_current =
        proposal.expected_parent_hash ==
        current.hash;

    result.matches_current_content =
        proposal.revision.hash ==
        current.hash;

    result.matches_historical_content =
        historical_content_exists(
            tracker,
            proposal.revision.hash
        );

    // same content as the currently committed mission
    if (result.matches_current_content) {

        result.classification =
            RevisionCausalityClass::NO_OP_REUPLOAD;

        return result;
    }

    // old superseded mission submitted again
    // using a fresh transaction
    if (result.matches_historical_content) {

        result.classification =
            RevisionCausalityClass::ROLLBACK;

        return result;
    }

    // another different proposal is already pending
    // against the same committed parent
    if (active_proposal != nullptr &&
        active_proposal->state ==
            ProposalState::PROPOSED &&
        active_proposal->revision.id !=
            proposal.revision.id &&
        active_proposal->revision.hash !=
            proposal.revision.hash &&
        active_proposal->expected_parent_hash ==
            current.hash &&
        proposal.expected_parent_hash ==
            current.hash) {

        result.conflicts_with_active_proposal =
            true;

        result.classification =
            RevisionCausalityClass::CONCURRENT_CONFLICT;

        return result;
    }

    // proposal was created against an older parent
    if (!result.parent_matches_current) {

        result.classification =
            RevisionCausalityClass::STALE_PARENT;

        return result;
    }

    // parent is correct but almost the entire mission
    // has been replaced
    if (result.major_replacement) {

        result.classification =
            RevisionCausalityClass::UNRELATED_REPLACEMENT;

        return result;
    }

    result.classification =
        RevisionCausalityClass::NORMAL_CHILD;

    return result;
}

const char* revision_causality_name(
    RevisionCausalityClass classification
) {
    switch (classification) {

        case RevisionCausalityClass::INITIAL_MISSION:
            return "INITIAL_MISSION";

        case RevisionCausalityClass::NO_OP_REUPLOAD:
            return "NO_OP_REUPLOAD";

        case RevisionCausalityClass::NORMAL_CHILD:
            return "NORMAL_CHILD";

        case RevisionCausalityClass::STALE_PARENT:
            return "STALE_PARENT";

        case RevisionCausalityClass::ROLLBACK:
            return "ROLLBACK";

        case RevisionCausalityClass::CONCURRENT_CONFLICT:
            return "CONCURRENT_CONFLICT";

        case RevisionCausalityClass::UNRELATED_REPLACEMENT:
            return "UNRELATED_REPLACEMENT";
    }

    return "UNKNOWN";
}