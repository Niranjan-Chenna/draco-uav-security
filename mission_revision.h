#pragma once
#include <cstdint>
#include <string>
#include "canonical_mission.h"

using MissionRevisionId= uint64_t; 

struct MissionRevision {
    MissionRevisionId id;
    std::string hash;
    CanonicalMission mission;
};

std::string compute_mission_hash(const CanonicalMission& mission);
MissionRevision make_mission_revision(MissionRevisionId id, const CanonicalMission& mission);
