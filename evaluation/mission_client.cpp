#include "scenarios.h"
#include "structured_events.h"
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <deque>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
constexpr uint8_t SYSID = 255, COMPID = 190;
uint16_t port(const char* value) {
    const std::string text = value;
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid local port");
    const auto n = std::stoul(text);
    if (!n || n > 65535) throw std::runtime_error("port out of range");
    return n;
}
struct Connection {
    int fd;
    sockaddr_in target{};
    std::deque<mavlink_message_t> inbox;
    Connection(uint16_t remote, uint16_t local) {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) throw std::runtime_error("client socket failed");
        sockaddr_in address{};
        address.sin_family = AF_INET; address.sin_port = htons(local);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
            close(fd); throw std::runtime_error("client bind failed");
        }
        target = address; target.sin_port = htons(remote);
        mavlink_message_t heartbeat{};
        mavlink_msg_heartbeat_pack(SYSID, COMPID, &heartbeat, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
        send(heartbeat);
    }
    ~Connection() { close(fd); }
    void send(const mavlink_message_t& message) {
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const auto length = mavlink_msg_to_send_buffer(buffer, &message);
        if (sendto(fd, buffer, length, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target)) != length)
            throw std::runtime_error("client send failed");
    }
    bool receive(mavlink_message_t& message, int timeout_ms = 1000) {
        const auto until = Clock::now() + std::chrono::milliseconds(timeout_ms);
        while (inbox.empty() && Clock::now() < until) {
            pollfd descriptor{fd, POLLIN, 0};
            if (poll(&descriptor, 1, 100) <= 0) continue;
            uint8_t buffer[65536]; sockaddr_in source{}; socklen_t length = sizeof(source);
            const auto count = recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&source), &length);
            if (count <= 0 || source.sin_addr.s_addr != htonl(INADDR_LOOPBACK) || source.sin_port != target.sin_port) continue;
            for (const auto& frame : parse_mavlink_data(buffer, count, MavlinkDirection::PX4_TO_GCS)) inbox.push_back(frame.message);
        }
        if (inbox.empty()) return false;
        message = inbox.front(); inbox.pop_front(); return true;
    }
};
int upload(Connection& connection, const CanonicalMission& mission) {
    const auto transaction = mission_transaction(mission);
    mavlink_mission_count_t count{};
    count.target_system = 1; count.target_component = 1; count.count = mission.items.size();
    mavlink_message_t first{};
    mavlink_msg_mission_count_encode(SYSID, COMPID, &first, &count);
    connection.send(first);
    const auto deadline = Clock::now() + std::chrono::seconds(25);
    bool requested = false;
    while (Clock::now() < deadline) {
        mavlink_message_t message{};
        if (!connection.receive(message)) { if (!requested) connection.send(first); continue; }
        if (message.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
            mavlink_mission_request_int_t request{};
            mavlink_msg_mission_request_int_decode(&message, &request);
            if (request.target_system != SYSID || request.target_component != COMPID || request.mission_type != MAV_MISSION_TYPE_MISSION) continue;
            if (request.seq >= transaction.items.size()) throw std::runtime_error("invalid requested item");
            auto item = transaction.items[request.seq];
            item.target_system = message.sysid; item.target_component = message.compid;
            mavlink_message_t response{};
            mavlink_msg_mission_item_int_encode(SYSID, COMPID, &response, &item);
            connection.send(response); requested = true;
        } else if (message.msgid == MAVLINK_MSG_ID_MISSION_ACK) {
            mavlink_mission_ack_t result{};
            mavlink_msg_mission_ack_decode(&message, &result);
            if (result.target_system == SYSID && result.target_component == COMPID && result.mission_type == MAV_MISSION_TYPE_MISSION)
                return result.type;
        }
    }
    throw std::runtime_error("mission upload timeout");
}
CanonicalMission download(Connection& connection) {
    mavlink_message_t request_list{};
    mavlink_msg_mission_request_list_pack(SYSID, COMPID, &request_list, 1, 1, MAV_MISSION_TYPE_MISSION);
    connection.send(request_list);
    MissionUploadTransaction transaction;
    const auto deadline = Clock::now() + std::chrono::seconds(25);
    auto request_item = [&] {
        for (uint16_t seq = 0; seq < transaction.expected_count; ++seq) {
            if (transaction.received[seq]) continue;
            mavlink_message_t request{};
            mavlink_msg_mission_request_int_pack(SYSID, COMPID, &request, 1, 1, seq, MAV_MISSION_TYPE_MISSION);
            connection.send(request); return;
        }
    };
    while (Clock::now() < deadline) {
        mavlink_message_t message{};
        if (!connection.receive(message)) {
            if (!transaction.active) connection.send(request_list); else request_item();
            continue;
        }
        if (message.sysid != 1 || message.compid != 1) continue;
        if (message.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
            mavlink_mission_count_t count{};
            mavlink_msg_mission_count_decode(&message, &count);
            if (count.target_system != SYSID || count.target_component != COMPID || count.mission_type != MAV_MISSION_TYPE_MISSION) continue;
            if (count.count > 1000) throw std::runtime_error("readback mission exceeds evaluation capacity");
            start_mission_upload(transaction, count);
            request_item();
        } else if (message.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT && transaction.active) {
            mavlink_mission_item_int_t item{};
            mavlink_msg_mission_item_int_decode(&message, &item);
            if (item.target_system != SYSID || item.target_component != COMPID || item.mission_type != MAV_MISSION_TYPE_MISSION) continue;
            store_mission_item(transaction, item); request_item();
        }
        if (mission_upload_complete(transaction)) {
            mavlink_mission_ack_t ack{};
            ack.target_system = 1; ack.target_component = 1; ack.type = MAV_MISSION_ACCEPTED;
            mavlink_message_t response{};
            mavlink_msg_mission_ack_encode(SYSID, COMPID, &response, &ack);
            connection.send(response);
            return make_canonical_mission(transaction);
        }
    }
    throw std::runtime_error("mission download timeout");
}
void flight_command(Connection& connection, uint16_t command, float first) {
    mavlink_command_long_t payload{};
    payload.target_system = 1; payload.target_component = 1;
    payload.command = command; payload.param1 = first;
    payload.param4 = payload.param5 = payload.param6 = payload.param7 = std::numeric_limits<float>::quiet_NaN();
    mavlink_message_t message{};
    mavlink_msg_command_long_encode(SYSID, COMPID, &message, &payload);
    connection.send(message);
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (Clock::now() < deadline) {
        mavlink_message_t response{};
        if (!connection.receive(response, 500)) continue;
        if (response.msgid != MAVLINK_MSG_ID_COMMAND_ACK || response.sysid != 1) continue;
        mavlink_command_ack_t ack{};
        mavlink_msg_command_ack_decode(&response, &ack);
        if (ack.command != command) continue;
        if (ack.result != MAV_RESULT_ACCEPTED) throw std::runtime_error("PX4 flight command rejected: " + std::to_string(ack.result));
        return;
    }
    throw std::runtime_error("PX4 flight command acknowledgement timeout");
}
void flight(Connection& connection, const std::string& action) {
    if (action == "takeoff") {
        flight_command(connection, MAV_CMD_NAV_TAKEOFF, 0);
        flight_command(connection, MAV_CMD_COMPONENT_ARM_DISARM, 1);
    } else if (action == "land") flight_command(connection, MAV_CMD_NAV_LAND, 0);
    else throw std::runtime_error("flight action must be takeoff or land");
    const auto deadline = Clock::now() + std::chrono::seconds(20);
    auto heartbeat_at = Clock::now();
    bool armed = true;
    while (Clock::now() < deadline) {
        if (Clock::now() >= heartbeat_at) {
            mavlink_message_t heartbeat{};
            mavlink_msg_heartbeat_pack(SYSID, COMPID, &heartbeat, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
            connection.send(heartbeat);
            heartbeat_at = Clock::now() + std::chrono::milliseconds(500);
        }
        mavlink_message_t message{};
        if (!connection.receive(message, 100)) continue;
        if (message.sysid != 1 || message.compid != 1) continue;
        if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
            mavlink_heartbeat_t payload{}; mavlink_msg_heartbeat_decode(&message, &payload);
            armed = payload.base_mode & MAV_MODE_FLAG_SAFETY_ARMED;
            if (action == "land" && !armed) return;
        }
        if (message.msgid == MAVLINK_MSG_ID_EXTENDED_SYS_STATE && action == "takeoff") {
            mavlink_extended_sys_state_t payload{}; mavlink_msg_extended_sys_state_decode(&message, &payload);
            if (armed && payload.landed_state == MAV_LANDED_STATE_IN_AIR) return;
        }
    }
    throw std::runtime_error("PX4 did not reach requested flight state");
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "hash") {
            Event result; put(result, "hash", compute_mission_hash(read_mission(argv[2])));
            std::cout << serialize_event(result) << '\n'; return 0;
        }
        if (argc != 5) throw std::runtime_error("usage: mission_client upload|download FILE TARGET_PORT LOCAL_PORT; flight takeoff|land TARGET_PORT LOCAL_PORT; or hash FILE");
        const std::string action = argv[1];
        Connection connection(port(argv[3]), port(argv[4]));
        Event result;
        if (action == "flight") {
            flight(connection, argv[2]);
            put(result, "flight_action", argv[2]); put(result, "completed", true);
            std::cout << serialize_event(result) << '\n'; return 0;
        }
        if (action == "upload") {
            const auto mission = read_mission(argv[2]);
            const int outcome = upload(connection, mission);
            put(result, "ack_result", double(outcome)); put(result, "proposal_hash", compute_mission_hash(mission));
            std::cout << serialize_event(result) << '\n';
            return outcome == MAV_MISSION_ACCEPTED ? 0 : 3;
        }
        if (action != "download") throw std::runtime_error("unknown client action");
        const auto mission = download(connection);
        write_mission(mission, argv[2]);
        put(result, "hash", compute_mission_hash(mission)); put(result, "item_count", double(mission.items.size()));
        put(result, "source", "PX4_MISSION_ITEM_INT_READBACK");
        std::cout << serialize_event(result) << '\n';
        return 0;
    } catch (const std::exception& error) {
        Event result; put(result, "error", error.what());
        std::cout << serialize_event(result) << '\n'; return 2;
    }
}
