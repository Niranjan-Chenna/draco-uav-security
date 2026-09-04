#include "scenarios.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>

CanonicalMission evaluation_mission(std::size_t size) {
    if (size < 3 || size > 1000) throw std::runtime_error("mission size must be 3..1000");
    CanonicalMission mission{};
    for (std::size_t i = 0; i < size; ++i) {
        CanonicalMissionItem item{};
        item.seq = i;
        item.command = i == 0 ? MAV_CMD_NAV_TAKEOFF : (i + 1 == size ? MAV_CMD_NAV_LAND : MAV_CMD_NAV_WAYPOINT);
        item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
        item.x = 473979578 + (473984130 - 473979578) * i / (size - 1);
        item.y = 85470823 + (85472362 - 85470823) * i / (size - 1);
        item.z = 50;
        item.current = i == 0 ? 1 : 0;
        item.autocontinue = 1;
        mission.items.push_back(item);
    }
    return mission;
}
MissionUploadTransaction mission_transaction(const CanonicalMission& mission) {
    MissionUploadTransaction upload{};
    mavlink_mission_count_t count{};
    count.count = mission.items.size(); count.mission_type = mission.mission_type;
    start_mission_upload(upload, count);
    for (const auto& item : mission.items) {
        mavlink_mission_item_int_t raw{};
        raw.seq = item.seq; raw.command = item.command; raw.frame = item.frame;
        raw.param1 = item.param1; raw.param2 = item.param2; raw.param3 = item.param3; raw.param4 = item.param4;
        raw.x = item.x; raw.y = item.y; raw.z = item.z;
        raw.current = item.current; raw.autocontinue = item.autocontinue;
        raw.mission_type = mission.mission_type;
        store_mission_item(upload, raw);
    }
    return upload;
}
std::vector<Scenario> evaluation_scenarios() {
    using C = RevisionCausalityClass;
    using D = MissionAuthorizationDecision;
    const auto base = evaluation_mission();
    std::vector<Scenario> cases;
    auto add = [&](const std::string& id, CanonicalMission proposal, C c, D d, const std::string& reason) -> Scenario& {
        for (std::size_t i = 0; i < proposal.items.size(); ++i) proposal.items[i].seq = i;
        cases.push_back({id, "", id.rfind("BENIGN", 0) == 0 ? "benign" : "attack", base, proposal, c, d, reason});
        return cases.back();
    };
    add("BENIGN_NO_OP", base, C::NO_OP_REUPLOAD, D::ALLOW, "NO_OP_REUPLOAD");
    auto changed = base; changed.items[3].y += 100;
    add("BENIGN_SMALL_CORRECTION", changed, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED");
    auto inserted = base;
    auto extra = base.items[4]; extra.x += 200; extra.y += 300;
    inserted.items.insert(inserted.items.begin() + 5, extra);
    add("BENIGN_INSERT_DELETE", inserted, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED").variant = "insert";
    auto deleted = base; deleted.items.erase(deleted.items.begin() + 4);
    add("BENIGN_INSERT_DELETE", deleted, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED").variant = "delete";
    changed = base; changed.items[3].z += 5;
    add("BENIGN_ALTITUDE_CORRECTION", changed, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED");
    changed = base; changed.items[4].y += 3000;
    add("BENIGN_DETOUR", changed, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED");
    changed = base; changed.items[4].y += 200;
    add("BENIGN_IN_FLIGHT_REPLAN", changed, C::NORMAL_CHILD, D::ALLOW, "AUTHORIZED").in_flight = true;
    changed = base; changed.items[3].y += 100;
    auto& rollback = add("ATTACK_SEMANTIC_ROLLBACK", base, C::ROLLBACK, D::DENY, "SEMANTIC_ROLLBACK_DETECTED");
    rollback.starting = changed; rollback.historical_parent = true;
    auto stale = base; stale.items[4].y += 200;
    auto& stale_case = add("ATTACK_STALE_PARENT", stale, C::STALE_PARENT, D::DENY, "STALE_PARENT_REVISION");
    stale_case.starting = changed; stale_case.historical_parent = true;
    add("ATTACK_CONCURRENT_CONFLICT", stale, C::CONCURRENT_CONFLICT, D::DENY, "CONCURRENT_REVISION_CONFLICT").concurrent = true;
    changed = base; changed.items.back().x += 500;
    add("ATTACK_DESTINATION_DIVERSION", changed, C::NORMAL_CHILD, D::REQUIRE_HIGHER_AUTHORITY, "DESTINATION_CHANGE_OUTSIDE_BUDGET");
    changed = base; changed.items[3].command = MAV_CMD_DO_SET_SERVO;
    add("ATTACK_COMMAND_SUBSTITUTION", changed, C::NORMAL_CHILD, D::DENY, "COMMAND_NOT_ALLOWED");
    changed = base; for (auto& item : changed.items) item.x += 5000;
    add("ATTACK_MAJOR_REPLACEMENT", changed, C::UNRELATED_REPLACEMENT, D::REQUIRE_HIGHER_AUTHORITY, "UNRELATED_REPLACEMENT_REQUIRES_ADMIN");
    changed = base; extra = base.items[4]; extra.y += 100000;
    changed.items.insert(changed.items.begin() + 5, extra);
    add("ATTACK_OUTSIDE_INTENT", changed, C::NORMAL_CHILD, D::DENY, "MISSION_CORRIDOR_VIOLATION");
    return cases;
}
void write_mission(const CanonicalMission& mission, const std::string& path) {
    std::ofstream file(path);
    file << std::setprecision(9);
    for (const auto& i : mission.items)
        file << i.seq << ' ' << i.command << ' ' << int(i.frame) << ' ' << i.param1 << ' ' << i.param2 << ' '
             << i.param3 << ' ' << i.param4 << ' ' << i.x << ' ' << i.y << ' ' << i.z << ' '
             << int(i.current) << ' ' << int(i.autocontinue) << '\n';
    if (!file) throw std::runtime_error("cannot write mission file");
}
CanonicalMission read_mission(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot read mission file");
    CanonicalMission result;
    CanonicalMissionItem item;
    unsigned frame, current, autocontinue;
    while (file >> item.seq >> item.command >> frame >> item.param1 >> item.param2 >> item.param3 >> item.param4
                >> item.x >> item.y >> item.z >> current >> autocontinue) {
        if (item.seq != result.items.size() || frame > 255 || current > 1 || autocontinue > 1 || result.items.size() >= 1000)
            throw std::runtime_error("invalid mission item");
        item.frame = frame; item.current = current; item.autocontinue = autocontinue;
        result.items.push_back(item);
    }
    if (!file.eof()) throw std::runtime_error("malformed mission file");
    return result;
}
StateCache evaluation_evidence(bool in_flight) {
    StateCache state;
    auto fresh = [](auto& field) {
        field.valid = true; field.freshness = EvidenceFreshness::FRESH;
        field.observed_at = std::chrono::steady_clock::now();
        field.source_sysid = 1; field.source_compid = 1;
    };
    fresh(state.armed); fresh(state.landed_state); fresh(state.global_position);
    state.armed.value = in_flight;
    state.landed_state.value = in_flight ? MAV_LANDED_STATE_IN_AIR : MAV_LANDED_STATE_ON_GROUND;
    return state;
}
