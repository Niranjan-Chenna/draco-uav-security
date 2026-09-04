#include "runtime_policy.h"
#include "principal_context.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> values;
    std::string part;
    std::istringstream input(value);
    while (std::getline(input, part, delimiter)) {
        part = trim(part);
        if (part.empty()) throw std::runtime_error("empty list entry");
        values.push_back(part);
    }
    if (value.empty() || value.back() == delimiter) throw std::runtime_error("empty list entry");
    return values;
}
uint64_t integer(const std::string& value, uint64_t maximum, bool positive = false) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid unsigned integer");
    const auto result = std::stoull(value);
    if (result > maximum || (positive && result == 0)) throw std::runtime_error("integer out of range");
    return result;
}
double number(const std::string& value) {
    std::size_t used = 0;
    const double result = std::stod(value, &used);
    if (used != value.size() || !std::isfinite(result)) throw std::runtime_error("invalid finite number");
    return result;
}
bool boolean(const std::string& value) {
    if (value == "true") return true;
    if (value == "false") return false;
    throw std::runtime_error("boolean must be true or false");
}
IntentGeoPoint point(const std::vector<std::string>& v) {
    const double lat = number(v.at(0)), lon = number(v.at(1)), alt = number(v.at(2));
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180 ||
        std::abs(alt) > std::numeric_limits<float>::max())
        throw std::runtime_error("point out of range");
    return {static_cast<int32_t>(std::llround(lat * 1e7)),
            static_cast<int32_t>(std::llround(lon * 1e7)), static_cast<float>(alt)};
}
IntentRegion region(const std::string& value) {
    auto fields = split(value, ',');
    if (fields.size() != 4) throw std::runtime_error("region requires latitude,longitude,altitude,radius");
    const auto radius = number(fields[3]);
    if (radius < 0) throw std::runtime_error("negative radius");
    return {point(fields), radius};
}
std::vector<uint16_t> commands(const std::string& value, bool allow_empty) {
    if (allow_empty && value == "none") return {};
    std::vector<uint16_t> result;
    std::set<uint16_t> seen;
    for (const auto& entry : split(value, ',')) {
        auto command = static_cast<uint16_t>(integer(entry, UINT16_MAX, true));
        if (!seen.insert(command).second) throw std::runtime_error("duplicate command");
        result.push_back(command);
    }
    return result;
}
std::vector<MissionAuthorityTier> authorities(const std::string& value) {
    if (value == "none") return {};
    std::vector<MissionAuthorityTier> result;
    std::set<MissionAuthorityTier> seen;
    for (const auto& entry : split(value, ',')) {
        auto tier = parse_authority(entry);
        if (!seen.insert(tier).second) throw std::runtime_error("duplicate authority");
        result.push_back(tier);
    }
    return result;
}
}

RuntimePolicy load_runtime_policy(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open trusted policy file: " + path);
    std::map<std::string, std::string> values;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos || equals == 0 || equals == line.size() - 1)
            throw std::runtime_error("malformed policy line " + std::to_string(line_number));
        if (!values.emplace(trim(line.substr(0, equals)), trim(line.substr(equals + 1))).second)
            throw std::runtime_error("duplicate policy key at line " + std::to_string(line_number));
    }
    if (input.bad()) throw std::runtime_error("policy read failed");
    auto get = [&](const std::string& key) {
        auto found = values.find(key);
        if (found == values.end()) throw std::runtime_error("missing policy key: " + key);
        auto result = found->second;
        values.erase(found);
        return result;
    };
    RuntimePolicy policy{};
    auto& c = policy.contract;
    auto& b = policy.budget;
    c.contract_id = integer(get("contract_id"), UINT64_MAX, true);
    c.version = integer(get("contract_version"), UINT32_MAX, true);
    policy.altitude_reference = get("altitude_reference");
    if (policy.altitude_reference != "RELATIVE_HOME")
        throw std::runtime_error("only RELATIVE_HOME runtime altitude reference is supported");
    c.start_region = region(get("start_region"));
    c.terminal_region = region(get("terminal_region"));
    for (const auto& entry : split(get("corridor_points"), ';')) {
        const auto fields = split(entry, ',');
        if (fields.size() != 3) throw std::runtime_error("corridor point requires latitude,longitude,altitude");
        c.corridor.centerline.push_back(point(fields));
    }
    c.corridor.allowed_deviation_m = number(get("corridor_deviation_m"));
    const auto excluded = get("excluded_regions");
    if (excluded != "none")
        for (const auto& entry : split(excluded, ';')) c.excluded_regions.push_back(region(entry));
    const auto minimum = number(get("altitude_minimum_m"));
    const auto maximum = number(get("altitude_maximum_m"));
    if (std::abs(minimum) > std::numeric_limits<float>::max() ||
        std::abs(maximum) > std::numeric_limits<float>::max()) throw std::runtime_error("altitude overflow");
    c.altitude = {static_cast<float>(minimum), static_cast<float>(maximum)};
    c.command_policy.allowed_commands = commands(get("normal_allowed_commands"), false);
    c.emergency_policy.allowed_commands = commands(get("emergency_allowed_commands"), true);
    c.emergency_policy.allow_destination_change = boolean(get("emergency_allow_destination_change"));
    c.authority_policy.destination_change_authorities = authorities(get("destination_change_authorities"));
    c.authority_policy.emergency_authorities = authorities(get("emergency_authorities"));
    c.authority_policy.contract_admin_authorities = authorities(get("contract_admin_authorities"));
    for (auto tier : c.authority_policy.emergency_authorities)
        if (tier == MissionAuthorityTier::NORMAL_OPERATOR) throw std::runtime_error("normal operator is not emergency authority");
    for (auto tier : c.authority_policy.contract_admin_authorities)
        if (tier != MissionAuthorityTier::SECURITY_ADMIN) throw std::runtime_error("contract administration requires security admin");
    c.allow_in_flight_replanning = boolean(get("allow_in_flight_replanning"));
    c.destination_change_requires_authority = boolean(get("destination_change_requires_authority"));
    c.has_validity_window = boolean(get("has_validity_window"));
    c.valid_from_unix_ms = integer(get("valid_from_unix_ms"), UINT64_MAX);
    c.valid_until_unix_ms = integer(get("valid_until_unix_ms"), UINT64_MAX);
    if (!c.has_validity_window && (c.valid_from_unix_ms || c.valid_until_unix_ms))
        throw std::runtime_error("disabled validity window must have zero endpoints");
    b.policy_id = integer(get("budget_policy_id"), UINT64_MAX, true);
    b.version = integer(get("budget_version"), UINT32_MAX, true);
    b.maximum_horizontal_change_m = number(get("maximum_horizontal_change_m"));
    b.maximum_altitude_change_m = number(get("maximum_altitude_change_m"));
    b.maximum_insertions = integer(get("maximum_insertions"), UINT16_MAX);
    b.maximum_deletions = integer(get("maximum_deletions"), UINT16_MAX);
    b.maximum_changed_item_ratio = number(get("maximum_changed_item_ratio"));
    b.allow_destination_change = boolean(get("allow_destination_change"));
    if (b.maximum_horizontal_change_m < 0 || b.maximum_altitude_change_m < 0 ||
        b.maximum_changed_item_ratio < 0 || b.maximum_changed_item_ratio > 1)
        throw std::runtime_error("invalid change budget bounds");
    if (!validate_mission_intent_contract(c)) throw std::runtime_error("invalid mission intent geometry or validity bounds");
    if (!values.empty()) throw std::runtime_error("unknown policy key: " + values.begin()->first);
    return policy;
}
