#include "structured_events.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

std::string json_quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c);
        else out << c;
    }
    out << '"';
    return out.str();
}
void put(Event& e, const std::string& k, const std::string& v) { e[k] = json_quote(v); }
void put(Event& e, const std::string& k, const char* v) { put(e, k, std::string(v)); }
void put(Event& e, const std::string& k, double v) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << v;
    e[k] = std::isfinite(v) ? out.str() : "null";
}
void put(Event& e, const std::string& k, bool v) { e[k] = v ? "true" : "false"; }
std::string serialize_event(const Event& e) {
    std::string out = "{";
    for (const auto& pair : e) {
        if (out.size() > 1) out += ',';
        out += json_quote(pair.first) + ':' + pair.second;
    }
    return out + '}';
}
Event decision_event(const MissionDecisionRecord& r) {
    Event e;
    put(e, "timestamp", static_cast<double>(r.decision_time_ms));
    put(e, "event_type", "decision");
    put(e, "direction", "GCS_TO_PX4");
    put(e, "scenario_id", r.scenario_id);
    e["revision_id"] = std::to_string(r.proposal.revision.id);
    put(e, "proposal_hash", r.proposal.revision.hash);
    put(e, "parent_hash", r.proposal.expected_parent_hash);
    put(e, "current_revision_hash", r.current_revision_hash);
    put(e, "causality_class", revision_causality_name(r.causality.classification));
    e["contract_id"] = std::to_string(r.contract_id);
    e["contract_version"] = std::to_string(r.contract_version);
    e["change_budget_policy_id"] = std::to_string(r.change_budget_policy_id);
    e["change_budget_policy_version"] = std::to_string(r.change_budget_policy_version);
    put(e, "principal_id", r.principal.principal_id);
    put(e, "principal_authenticated", r.principal.authenticated);
    put(e, "authority", authority_name(r.authority));
    put(e, "evaluation_mode", r.principal.evaluation_mode);
    put(e, "evaluation_variant", evaluation_mode_name(r.evaluation_mode));
    put(e, "evidence_usable", r.evidence_usable);
    put(e, "vehicle_in_flight", r.vehicle_in_flight);
    put(e, "authorization_decision", mission_authorization_decision_name(r.authorization.decision));
    put(e, "authorization_reason", r.authorization.reason);
    const auto& d = r.delta.summary;
    for (const auto& p : std::vector<std::pair<std::string, double>>{
        {"insertions", double(d.inserted)}, {"deletions", double(d.deleted)},
        {"horizontal_moves", double(d.moved_horizontal)}, {"altitude_changes", double(d.altitude_changed)},
        {"command_changes", double(d.command_changed)}, {"parameter_changes", double(d.parameter_changed)},
        {"reorder", double(d.reordered)}, {"changed_item_ratio", d.changed_item_ratio},
        {"maximum_horizontal_change_m", d.maximum_horizontal_displacement_m},
        {"maximum_altitude_change_m", d.maximum_altitude_change_m},
        {"canonicalization_latency_us", r.canonicalization_latency_us},
        {"mission_hash_latency_us", r.mission_hash_latency_us},
        {"semantic_delta_latency_us", r.semantic_delta_latency_us},
        {"authorization_latency_us", r.policy_latency_us}, {"decision_latency_us", r.decision_latency_us},
        {"mission_item_count", double(r.proposal.revision.mission.items.size())}}) put(e, p.first, p.second);
    put(e, "destination_changed", d.destination_changed);
    put(e, "major_replacement", d.major_replacement);
    put(e, "px4_upload_started", false);
    for (const auto* key : {"px4_ack_result", "px4_mission_hash_before", "px4_mission_hash_after",
                            "expected_outcome", "outcome_correct"}) e[key] = "null";
    return e;
}

namespace {
const std::vector<std::string> columns = {
    "timestamp", "scenario_id", "direction", "event_type", "revision_id", "proposal_hash", "parent_hash",
    "current_revision_hash", "causality_class", "insertions", "deletions", "horizontal_moves", "altitude_changes",
    "command_changes", "parameter_changes", "reorder", "destination_changed", "major_replacement",
    "changed_item_ratio", "maximum_horizontal_change_m", "maximum_altitude_change_m", "contract_id", "contract_version",
    "change_budget_policy_id", "change_budget_policy_version", "principal_id", "principal_authenticated", "authority",
    "evaluation_mode", "evaluation_variant", "evidence_usable", "vehicle_in_flight", "authorization_decision",
    "authorization_reason", "decision_latency_us", "px4_upload_started", "px4_ack_result", "px4_mission_hash_before",
    "px4_mission_hash_after", "expected_outcome", "outcome_correct"};
std::string csv_cell(const std::string& value) {
    std::string decoded;
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        for (std::size_t i = 1; i + 1 < value.size(); ++i) {
            if (value[i] == '\\' && i + 2 < value.size()) {
                ++i;
                if (value[i] == 'u' && i + 4 < value.size()) {
                    decoded += static_cast<char>(std::stoi(value.substr(i + 1, 4), nullptr, 16));
                    i += 4;
                } else decoded += value[i];
            } else decoded += value[i];
        }
    } else decoded = value;
    std::string result = "\"";
    for (char c : decoded) result += c == '"' ? "\"\"" : std::string(1, c);
    return result + '"';
}
}
EventLog::EventLog(const std::string& directory) {
    std::filesystem::create_directories(directory);
    const auto root = std::filesystem::path(directory);
    const bool header = !std::filesystem::exists(root / "decisions.csv") ||
        std::filesystem::file_size(root / "decisions.csv") == 0;
    jsonl_.open(root / "events.jsonl", std::ios::app);
    csv_.open(root / "decisions.csv", std::ios::app);
    if (!jsonl_ || !csv_) throw std::runtime_error("cannot open structured event output");
    if (header) {
        for (std::size_t i = 0; i < columns.size(); ++i) csv_ << (i ? "," : "") << columns[i];
        csv_ << '\n';
    }
}
void EventLog::emit(Event event) {
    if (!event.count("timestamp")) put(event, "timestamp", static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    jsonl_ << serialize_event(event) << '\n';
    jsonl_.flush();
    if (event.count("authorization_decision")) {
        for (std::size_t i = 0; i < columns.size(); ++i) {
            const auto it = event.find(columns[i]);
            csv_ << (i ? "," : "") << csv_cell(it == event.end() ? "null" : it->second);
        }
        csv_ << '\n';
        csv_.flush();
    }
    if (!jsonl_ || !csv_) throw std::runtime_error("structured event write failed");
}
