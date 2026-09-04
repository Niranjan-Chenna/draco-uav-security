#pragma once
#include <string>
#include "mission_intent_contract.h"

struct PrincipalContext {
    std::string principal_id;
    MissionAuthorityTier authority{MissionAuthorityTier::NORMAL_OPERATOR};
    bool authenticated{false};
    bool evaluation_mode{false};
};

MissionAuthorityTier parse_authority(const std::string& name);
const char* authority_name(MissionAuthorityTier authority);
PrincipalContext resolve_principal(bool evaluation_mode, const std::string& id = "",
                                   const std::string& authority = "");
bool principal_may_submit(const PrincipalContext& principal);
