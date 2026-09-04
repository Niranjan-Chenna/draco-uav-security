#include "gateway_options.h"
#include "gateway_ack_reconciliation.h"
#include "mission_pipeline.h"
#include "structured_events.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
constexpr uint8_t DRACO_SYSID = 245;
constexpr uint8_t DRACO_COMPID = MAV_COMP_ID_ONBOARD_COMPUTER;
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
struct Socket {
    int fd{-1};
    explicit Socket(uint16_t port) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) throw std::runtime_error("udp socket creation failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close(fd);
            throw std::runtime_error("cannot bind loopback udp port " + std::to_string(port));
        }
    }
    ~Socket() { if (fd >= 0) close(fd); }
    Socket(const Socket&) = delete;
};
void send_bytes(int fd, const sockaddr_in& target, const uint8_t* data, std::size_t size) {
    if (sendto(fd, data, size, 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) !=
        static_cast<ssize_t>(size)) throw std::runtime_error("udp send failed");
}
void send_message(int fd, const sockaddr_in& target, const mavlink_message_t& message) {
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const auto size = mavlink_msg_to_send_buffer(bytes, &message);
    send_bytes(fd, target, bytes, size);
}
struct Client {
    sockaddr_in address{};
    uint8_t sysid{}, compid{};
    MissionUploadTransaction upload;
    std::string parent_hash, scenario_id;
    Clock::time_point last_seen{};
};
void ack(int fd, const Client& client, uint8_t result, uint8_t type = MAV_MISSION_TYPE_MISSION) {
    mavlink_mission_ack_t payload{};
    payload.target_system = client.sysid;
    payload.target_component = client.compid;
    payload.type = result;
    payload.mission_type = type;
    mavlink_message_t message{};
    mavlink_msg_mission_ack_encode(DRACO_SYSID, DRACO_COMPID, &message, &payload);
    send_message(fd, client.address, message);
}
void request_next(int fd, const Client& client) {
    for (uint16_t seq = 0; seq < client.upload.expected_count; ++seq) {
        if (client.upload.received[seq]) continue;
        mavlink_message_t message{};
        mavlink_msg_mission_request_int_pack(DRACO_SYSID, DRACO_COMPID, &message,
            client.sysid, client.compid, seq, MAV_MISSION_TYPE_MISSION);
        send_message(fd, client.address, message);
        break;
    }
}
bool is_hash(const std::string& value) {
    return value.empty() || (value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos);
}
void load_context(Client& client, const GatewayOptions& options) {
    if (options.evaluation_context_directory.empty()) return;
    const auto path = std::filesystem::path(options.evaluation_context_directory) /
        (std::to_string(ntohs(client.address.sin_port)) + ".conf");
    std::ifstream input(path);
    if (!input) throw std::runtime_error("missing explicit evaluation transaction context");
    std::string key, line;
    bool have_scenario = false, have_parent = false;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto split = line.find('=');
        if (split == std::string::npos) throw std::runtime_error("malformed evaluation context");
        key = line.substr(0, split);
        auto value = line.substr(split + 1);
        if (key == "scenario_id" && !have_scenario) {
            if (value.empty() || value.size() > 96 || value.find_first_not_of(
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
                throw std::runtime_error("invalid scenario id");
            client.scenario_id = value;
            have_scenario = true;
        } else if (key == "expected_parent_hash" && !have_parent && is_hash(value)) {
            client.parent_hash = value;
            have_parent = true;
        } else throw std::runtime_error("invalid or duplicate evaluation context key");
    }
    if (!have_scenario || !have_parent) throw std::runtime_error("incomplete evaluation context");
}
}

int run_gateway(const GatewayOptions& options) {
    EventLog log(options.results_directory);
    RuntimePolicy policy;
    try { policy = load_runtime_policy(options.policy_path); }
    catch (const std::exception& error) {
        Event event; put(event, "event_type", "configuration_failure");
        put(event, "policy_loaded", false); put(event, "reason", error.what());
        log.emit(event); throw;
    }
    Socket gcs(options.gcs_port), px4(options.px4_local_port);
    sockaddr_in px4_target{};
    px4_target.sin_family = AF_INET;
    px4_target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    px4_target.sin_port = htons(options.px4_remote_port);
    StateCache state_cache{};
    MissionRevisionTracker mission_revisions{};
    std::optional<CanonicalMission> committed_canonical_mission;
    bool px4_authorized_upload_active = false;
    bool px4_state_uncertain = false;
    std::optional<mavlink_message_t> pending_count;
    Clock::time_point send_count_at{};
    Client authorized_client;
    std::optional<MissionDecisionRecord> authorized_decision;
    std::vector<bool> transferred;
    std::map<uint16_t, Client> clients;
    Clock::time_point last_px4{}, upload_started{}, last_status{};
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    running = 1;
    std::cout << "DRACO policy=" << policy.contract.contract_id << '/' << policy.contract.version
              << " principal=" << options.principal.principal_id
              << " authenticated=" << options.principal.authenticated
              << " evaluation_mode=" << options.principal.evaluation_mode
              << " variant=" << evaluation_mode_name(options.mode) << std::endl;
    auto flow = [&](const char* phase, int result = -1, int gcs_result = -1) {
        if (!authorized_decision) return;
        auto e = decision_event(*authorized_decision);
        put(e, "timestamp", double(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
        put(e, "event_type", phase);
        e.erase("authorization_decision");
        e.erase("authorization_reason");
        put(e, "px4_upload_started", px4_authorized_upload_active);
        put(e, "px4_state_uncertain", px4_state_uncertain);
        if (result >= 0) put(e, "px4_ack_result", double(result));
        if (gcs_result >= 0) put(e, "gcs_ack_result", double(gcs_result));
        log.emit(e);
    };
    auto protocol_rejection = [&](const Client& client, const char* reason, uint32_t message_id) {
        MissionProposalRecord proposal{};
        proposal.proposer_principal = options.principal.principal_id;
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto record = make_mission_decision_record(proposal, {}, {}, {}, options.principal.authority,
            false, policy.contract, policy.budget, state_cache,
            {MissionAuthorizationDecision::DENY, reason}, now_ms);
        record.principal = options.principal;
        record.evaluation_mode = options.mode;
        record.scenario_id = client.scenario_id;
        auto event = decision_event(record);
        put(event, "event_type", "protocol_rejection");
        put(event, "message_id", double(message_id));
        event["revision_id"] = "null";
        event["proposal_hash"] = "null";
        event["causality_class"] = "null";
        event["decision_latency_us"] = "null";
        log.emit(event);
    };
    auto decide = [&](Client& client) {
        auto record = evaluate_proposal(client.upload, mission_revisions, client.parent_hash,
            options.principal, policy, state_cache, options.mode,
            authorized_decision ? &authorized_decision->proposal : nullptr, client.scenario_id);
        // the authorized buffer is immutable until the corresponding px4 ack or timeout.
        if (authorized_decision && record.authorization.decision == MissionAuthorizationDecision::ALLOW)
            record.authorization = {MissionAuthorizationDecision::DEFER, "PX4_UPLOAD_IN_PROGRESS"};
        if (px4_state_uncertain && record.authorization.decision == MissionAuthorizationDecision::ALLOW)
            record.authorization = {MissionAuthorizationDecision::DEFER, "PX4_COMMIT_STATE_UNCERTAIN"};
        log.emit(decision_event(record));
        std::cout << client.scenario_id << ' ' << mission_authorization_decision_name(record.authorization.decision)
                  << ' ' << record.authorization.reason << std::endl;
        if (record.authorization.decision != MissionAuthorizationDecision::ALLOW) {
            ack(gcs.fd, client, MAV_MISSION_DENIED);
            client.upload.active = false;
            return;
        }
        authorized_client = client;
        authorized_decision = record;
        mission_revisions.proposed = record.proposal.revision;
        transferred.assign(client.upload.items.size(), false);
        mavlink_mission_count_t count{};
        count.target_system = 1;
        count.target_component = MAV_COMP_ID_AUTOPILOT1;
        count.count = client.upload.expected_count;
        count.mission_type = MAV_MISSION_TYPE_MISSION;
        mavlink_message_t message{};
        mavlink_msg_mission_count_encode(DRACO_SYSID, DRACO_COMPID, &message, &count);
        pending_count = message;
        send_count_at = Clock::now() + std::chrono::milliseconds(options.evaluation_upload_delay_ms);
        client.upload.active = false;
        flow("authorized_upload_pending");
    };
    while (running) {
        pollfd fds[2]{{gcs.fd, POLLIN, 0}, {px4.fd, POLLIN, 0}};
        const int ready = poll(fds, 2, 100);
        if (ready < 0) { if (errno == EINTR) continue; throw std::runtime_error("poll failed"); }
        refresh_state_freshness(state_cache);
        for (int side = 0; side < 2; ++side) {
            if (!(fds[side].revents & POLLIN)) continue;
            uint8_t buffer[65536];
            sockaddr_in source{};
            socklen_t source_length = sizeof(source);
            auto size = recvfrom(fds[side].fd, buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr*>(&source), &source_length);
            if (size <= 0 || source.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) continue;
            if (side == 1 && source.sin_port != px4_target.sin_port) continue;
            auto frames = parse_mavlink_data(buffer, size,
                side == 0 ? MavlinkDirection::GCS_TO_PX4 : MavlinkDirection::PX4_TO_GCS);
            for (const auto& frame : frames) {
                const auto& msg = frame.message;
                bool intercepted = false;
                if (side == 0) {
                    if (!clients.count(source.sin_port) && clients.size() >= 8) continue;
                    auto& client = clients[source.sin_port];
                    client.address = source;
                    client.last_seen = Clock::now();
                    if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
                        intercepted = true;
                        client.sysid = msg.sysid; client.compid = msg.compid;
                        mavlink_mission_count_t count{};
                        mavlink_msg_mission_count_decode(&msg, &count);
                        if (count.mission_type != MAV_MISSION_TYPE_MISSION || count.count > 1000) {
                            protocol_rejection(client, "UNSUPPORTED_MISSION_TYPE_OR_CAPACITY", msg.msgid);
                            ack(gcs.fd, client, MAV_MISSION_DENIED, count.mission_type);
                            continue;
                        }
                        client.parent_hash = mission_revisions.current ? mission_revisions.current->hash : "";
                        client.scenario_id.clear();
                        try { load_context(client, options); }
                        catch (const std::exception& error) {
                            protocol_rejection(client, "INVALID_EVALUATION_CONTEXT", msg.msgid);
                            Event e; put(e, "event_type", "evaluation_context_failure"); put(e, "reason", error.what());
                            log.emit(e); ack(gcs.fd, client, MAV_MISSION_DENIED); continue;
                        }
                        start_mission_upload(client.upload, count);
                        Event e; put(e, "event_type", "buffering_gcs_mission");
                        put(e, "scenario_id", client.scenario_id); put(e, "mission_item_count", double(count.count)); log.emit(e);
                        if (count.count == 0) decide(client); else request_next(gcs.fd, client);
                    } else if (msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT) {
                        intercepted = true;
                        mavlink_mission_item_int_t item{};
                        mavlink_msg_mission_item_int_decode(&msg, &item);
                        if (!client.upload.active || msg.sysid != client.sysid || msg.compid != client.compid ||
                            item.mission_type != MAV_MISSION_TYPE_MISSION || item.seq >= client.upload.expected_count) continue;
                        store_mission_item(client.upload, item);
                        if (mission_upload_complete(client.upload)) decide(client); else request_next(gcs.fd, client);
                    } else if (msg.msgid == MAVLINK_MSG_ID_MISSION_CLEAR_ALL || msg.msgid == MAVLINK_MSG_ID_MISSION_WRITE_PARTIAL_LIST ||
                               msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM || msg.msgid == MAVLINK_MSG_ID_MISSION_SET_CURRENT) {
                        intercepted = true;
                        Client rejected = client; rejected.sysid = msg.sysid; rejected.compid = msg.compid;
                        ack(gcs.fd, rejected, MAV_MISSION_DENIED);
                        protocol_rejection(rejected, "UNSUPPORTED_MISSION_WRITE", msg.msgid);
                    }
                    if (!intercepted) send_bytes(px4.fd, px4_target, frame.wire_bytes.data(), frame.wire_bytes.size());
                } else {
                    // endpoint pinning is transport provenance, not cryptographic identity.
                    if (msg.sysid == 1 && msg.compid == MAV_COMP_ID_AUTOPILOT1) {
                        update_state_cache(state_cache, frame);
                        last_px4 = Clock::now();
                    }
                    if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT && px4_authorized_upload_active) {
                        mavlink_mission_request_int_t request{};
                        mavlink_msg_mission_request_int_decode(&msg, &request);
                        if (request.target_system == DRACO_SYSID && request.target_component == DRACO_COMPID &&
                            request.mission_type == MAV_MISSION_TYPE_MISSION && msg.sysid == 1 && msg.compid == MAV_COMP_ID_AUTOPILOT1) {
                            intercepted = true;
                            if (request.seq < authorized_client.upload.items.size()) {
                                auto item = authorized_client.upload.items[request.seq];
                                item.target_system = 1; item.target_component = MAV_COMP_ID_AUTOPILOT1;
                                mavlink_message_t response{};
                                mavlink_msg_mission_item_int_encode(DRACO_SYSID, DRACO_COMPID, &response, &item);
                                send_message(px4.fd, px4_target, response);
                                transferred[request.seq] = true;
                                Event e; put(e, "event_type", "px4_item_transfer");
                                put(e, "revision_id", double(authorized_decision->proposal.revision.id));
                                put(e, "item_sequence", double(request.seq)); log.emit(e);
                            }
                        }
                    } else if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK && px4_authorized_upload_active) {
                        mavlink_mission_ack_t result{};
                        mavlink_msg_mission_ack_decode(&msg, &result);
                        if (result.target_system == DRACO_SYSID && result.target_component == DRACO_COMPID &&
                            result.mission_type == MAV_MISSION_TYPE_MISSION && msg.sysid == 1 && msg.compid == MAV_COMP_ID_AUTOPILOT1) {
                            intercepted = true;
                            bool all_sent = true;
                            for (bool sent : transferred) all_sent = all_sent && sent;
                            if (result.type == MAV_MISSION_ACCEPTED && !all_sent) continue;
                            const auto reconciliation = reconcile_px4_mission_ack(
                                result.type,
                                mission_revisions,
                                authorized_decision->proposal.revision.mission,
                                committed_canonical_mission,
                                px4_state_uncertain);
                            flow(reconciliation.event_type, result.type, reconciliation.gcs_result);
                            ack(gcs.fd, authorized_client, reconciliation.gcs_result);
                            px4_authorized_upload_active = false;
                            authorized_decision.reset();
                        }
                    }
                    if (!intercepted) {
                        for (const auto& entry : clients)
                            send_bytes(gcs.fd, entry.second.address, frame.wire_bytes.data(), frame.wire_bytes.size());
                    }
                }
            }
        }
        const auto now = Clock::now();
        if (pending_count && now >= send_count_at) {
            send_message(px4.fd, px4_target, *pending_count);
            pending_count.reset();
            px4_authorized_upload_active = true;
            upload_started = now;
            flow("authorized_upload_started");
        }
        if (px4_authorized_upload_active && now - upload_started > std::chrono::seconds(15)) {
            flow("px4_upload_timeout");
            ack(gcs.fd, authorized_client, MAV_MISSION_ERROR);
            reject_proposed_revision(mission_revisions);
            px4_authorized_upload_active = false;
            px4_state_uncertain = true;
            authorized_decision.reset();
        }
        for (auto it = clients.begin(); it != clients.end();) {
            if (now - it->second.last_seen > std::chrono::seconds(30)) it = clients.erase(it);
            else ++it;
        }
        if (now - last_status > std::chrono::seconds(1)) {
            Event e; put(e, "event_type", "status");
            put(e, "gcs_connected", !clients.empty());
            put(e, "px4_connected", now - last_px4 < std::chrono::seconds(3));
            put(e, "draco_gcs_endpoint", "127.0.0.1:" + std::to_string(options.gcs_port));
            put(e, "draco_px4_endpoint", "127.0.0.1:" + std::to_string(options.px4_local_port));
            put(e, "px4_endpoint", "127.0.0.1:" + std::to_string(options.px4_remote_port));
            std::string endpoints;
            for (const auto& entry : clients) endpoints += "127.0.0.1:" + std::to_string(ntohs(entry.first)) + " ";
            put(e, "gcs_endpoint", endpoints);
            put(e, "armed", state_cache.armed.valid ? (state_cache.armed.value ? "armed" : "disarmed") : "unknown");
            put(e, "landed_state", state_cache.landed_state.valid ? std::to_string(state_cache.landed_state.value) : "unknown");
            put(e, "evidence_usable", evidence_is_usable(state_cache.armed) && evidence_is_usable(state_cache.landed_state) &&
                evidence_is_usable(state_cache.global_position));
            put(e, "evidence_age_ms", double(std::max({state_cache.armed.age.count(), state_cache.landed_state.age.count(),
                state_cache.global_position.age.count()})));
            if (state_cache.global_position.valid) {
                put(e, "latitude", state_cache.global_position.value.lat / 1e7);
                put(e, "longitude", state_cache.global_position.value.lon / 1e7);
                put(e, "relative_altitude_m", state_cache.global_position.value.relative_alt_mm / 1000.0);
            }
            put(e, "failsafe", state_cache.failsafe_active.valid ? (state_cache.failsafe_active.value ? "active" : "inactive") : "unknown");
            put(e, "health_unhealthy_enabled", state_cache.system_health.valid ?
                std::to_string(state_cache.system_health.value.unhealthy_enabled) : "unknown");
            put(e, "policy_loaded", true);
            put(e, "contract_id", double(policy.contract.contract_id));
            put(e, "contract_version", double(policy.contract.version));
            put(e, "change_budget_policy_id", double(policy.budget.policy_id));
            put(e, "change_budget_policy_version", double(policy.budget.version));
            put(e, "allow_in_flight_replanning", policy.contract.allow_in_flight_replanning);
            put(e, "current_revision_id", double(mission_revisions.current ? mission_revisions.current->id : 0));
            put(e, "parent_revision_id", double(mission_revisions.parent ? mission_revisions.parent->id : 0));
            put(e, "current_mission_hash", mission_revisions.current ? mission_revisions.current->hash : "");
            put(e, "mission_item_count", double(committed_canonical_mission ? committed_canonical_mission->items.size() : 0));
            put(e, "px4_state_uncertain", px4_state_uncertain);
            log.emit(e); last_status = now;
        }
    }
    return 0;
}
