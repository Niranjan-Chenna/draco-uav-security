#include "principal_context.h"
#include <stdexcept>
#include <cctype>

MissionAuthorityTier parse_authority(const std::string& name) {
    if (name == "NORMAL_OPERATOR") return MissionAuthorityTier::NORMAL_OPERATOR;
    if (name == "EMERGENCY_AUTHORITY") return MissionAuthorityTier::EMERGENCY_AUTHORITY;
    if (name == "SECURITY_ADMIN") return MissionAuthorityTier::SECURITY_ADMIN;
    throw std::runtime_error("invalid authority");
}

const char* authority_name(MissionAuthorityTier authority) {
    switch (authority) {
        case MissionAuthorityTier::NORMAL_OPERATOR: return "NORMAL_OPERATOR";
        case MissionAuthorityTier::EMERGENCY_AUTHORITY: return "EMERGENCY_AUTHORITY";
        case MissionAuthorityTier::SECURITY_ADMIN: return "SECURITY_ADMIN";
    }
    return "INVALID";
}

PrincipalContext resolve_principal(bool evaluation_mode, const std::string& id,
                                   const std::string& authority) {
    PrincipalContext result{};
    if (!evaluation_mode) {
        if (!id.empty() || !authority.empty())
            throw std::runtime_error("evaluation principal requires explicit evaluation mode");
        // no authenticated binding provider is installed; an empty id is intentional.
        return result;
    }
    if (id.empty() || id.size() > 96)
        throw std::runtime_error("evaluation principal id is required (1..96 characters)");
    for (unsigned char c : id)
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.')
            throw std::runtime_error("invalid evaluation principal id");
    result.principal_id = id;
    result.authority = parse_authority(authority);
    result.evaluation_mode = true;
    return result;
}

bool principal_may_submit(const PrincipalContext& principal) {
    return !principal.principal_id.empty() &&
        (principal.authenticated || principal.evaluation_mode);
}
