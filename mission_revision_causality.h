#pragma once

#include <cstdint>
#include <string>

#include "mission_delta.h"
#include "mission_revision_tracker.h"

enum class ProposalState {
    PROPOSED,
    AUTHORIZED,
    REJECTED,
    COMMITTED,
    FAILED
};

enum class RevisionCausalityClass {
    INITIAL_MISSION,
    NO_OP_REUPLOAD,
    NORMAL_CHILD,
    STALE_PARENT,
    ROLLBACK,
    CONCURRENT_CONFLICT,
    UNRELATED_REPLACEMENT
};

struct MissionProposalRecord {
    MissionRevision revision;

    // parent that was current when this proposal
    // was created
    std::string expected_parent_hash;

    // empty means identity has not yet been bound
    // to a trusted authentication mechanism
    std::string proposer_principal;

    uint64_t proposal_time_ms{0};

    ProposalState state{
        ProposalState::PROPOSED
    };
};

struct RevisionCausalityResult {
    RevisionCausalityClass classification{
        RevisionCausalityClass::NORMAL_CHILD
    };

    bool parent_matches_current{false};
    bool matches_current_content{false};
    bool matches_historical_content{false};
    bool conflicts_with_active_proposal{false};
    bool major_replacement{false};
};

MissionProposalRecord make_proposal_record(
    const MissionRevision& revision,
    const std::optional<MissionRevision>& expected_parent,
    const std::string& proposer_principal,
    uint64_t proposal_time_ms
);

RevisionCausalityResult classify_revision_causality(
    const MissionRevisionTracker& tracker,
    const MissionProposalRecord& proposal,
    const MissionDelta& delta,
    const MissionProposalRecord* active_proposal = nullptr
);

const char* revision_causality_name(
    RevisionCausalityClass classification
);