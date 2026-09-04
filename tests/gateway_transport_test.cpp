#include <cassert>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mavlink_parser.h"

namespace {
sockaddr_in address(int port) {
    sockaddr_in result{}; result.sin_family = AF_INET;
    result.sin_addr.s_addr = htonl(INADDR_LOOPBACK); result.sin_port = htons(port);
    return result;
}
int socket_at(int port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0); assert(fd >= 0);
    auto local = address(port);
    assert(bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0);
    return fd;
}
void send(int fd, int port, const mavlink_message_t& msg) {
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN]; auto n = mavlink_msg_to_send_buffer(bytes, &msg);
    auto target = address(port);
    assert(sendto(fd, bytes, n, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == n);
}
bool receive(int fd, uint32_t id, mavlink_message_t& message, int milliseconds = 1500) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < until) {
        pollfd descriptor{fd, POLLIN, 0};
        if (poll(&descriptor, 1, 50) <= 0) continue;
        uint8_t bytes[65536]; auto n = recv(fd, bytes, sizeof(bytes), 0);
        for (const auto& frame : parse_mavlink_data(bytes, n, MavlinkDirection::PX4_TO_GCS))
            if (frame.message.msgid == id) { message = frame.message; return true; }
    }
    return false;
}
std::string contents(const std::string& path) {
    std::ifstream file(path); return std::string(std::istreambuf_iterator<char>(file), {});
}
}
int main() {
    for (bool evaluation : {false, true}) {
        const std::string output = std::string("evaluation/results/raw/transport-") + (evaluation ? "evaluation" : "normal");
        std::filesystem::create_directories(output);
        std::ofstream(output + "/events.jsonl", std::ios::trunc).close();
        int px4 = socket_at(18871), gcs = socket_at(14862);
        pid_t child = fork(); assert(child >= 0);
        if (child == 0) {
            close(px4); close(gcs);
            if (evaluation)
                execl("build/draco", "draco", "--policy", "config/sitl_policy.conf", "--results", output.c_str(),
                    "--gcs-port", "14860", "--px4-local-port", "14850", "--px4-remote-port", "18871",
                    "--evaluation", "--principal", "test", "--authority", "NORMAL_OPERATOR", nullptr);
            else
                execl("build/draco", "draco", "--policy", "config/sitl_policy.conf", "--results", output.c_str(),
                    "--gcs-port", "14860", "--px4-local-port", "14850", "--px4-remote-port", "18871", nullptr);
            _exit(127);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        mavlink_message_t count{}, heartbeat{}, message{};
        mavlink_mission_count_t payload{}; payload.count = 1; payload.target_system = 1; payload.target_component = 1;
        mavlink_msg_mission_count_encode(255, 190, &count, &payload);
        mavlink_msg_heartbeat_pack(255, 190, &heartbeat, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
        uint8_t bytes[2 * MAVLINK_MAX_PACKET_LEN];
        const auto first = mavlink_msg_to_send_buffer(bytes, &count);
        const auto second = mavlink_msg_to_send_buffer(bytes + first, &heartbeat);
        auto target = address(14860);
        assert(sendto(gcs, bytes, first + second, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == first + second);
        assert(receive(px4, MAVLINK_MSG_ID_HEARTBEAT, message));
        uint8_t actual[MAVLINK_MAX_PACKET_LEN];
        assert(mavlink_msg_to_send_buffer(actual, &message) == second);
        assert(std::equal(actual, actual + second, bytes + first));
        assert(!receive(px4, MAVLINK_MSG_ID_MISSION_COUNT, message, 100));
        assert(receive(gcs, MAVLINK_MSG_ID_MISSION_REQUEST_INT, message));
        mavlink_msg_heartbeat_pack(1, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4, 0, 0, MAV_STATE_ACTIVE);
        send(px4, 14850, message);
        mavlink_extended_sys_state_t state{}; state.landed_state = MAV_LANDED_STATE_ON_GROUND;
        mavlink_msg_extended_sys_state_encode(1, 1, &message, &state); send(px4, 14850, message);
        mavlink_global_position_int_t position{}; position.lat = 473979578; position.lon = 85470823;
        mavlink_msg_global_position_int_encode(1, 1, &message, &position); send(px4, 14850, message);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        mavlink_mission_item_int_t item{};
        item.target_system = 1; item.target_component = 1;
        item.command = MAV_CMD_NAV_WAYPOINT; item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
        item.x = position.lat; item.y = position.lon; item.z = 50; item.autocontinue = 1;
        mavlink_msg_mission_item_int_encode(255, 190, &message, &item); send(gcs, 14860, message);
        if (!evaluation) {
            assert(receive(gcs, MAVLINK_MSG_ID_MISSION_ACK, message));
            mavlink_mission_ack_t result{}; mavlink_msg_mission_ack_decode(&message, &result);
            assert(result.type == MAV_MISSION_DENIED);
            assert(!receive(px4, MAVLINK_MSG_ID_MISSION_COUNT, message, 100));
            assert(contents(output + "/events.jsonl").find("PRINCIPAL_NOT_AUTHENTICATED") != std::string::npos);
        } else {
            assert(receive(px4, MAVLINK_MSG_ID_MISSION_COUNT, message));
            assert(contents(output + "/events.jsonl").find("revision_committed") == std::string::npos);
            mavlink_msg_mission_request_int_pack(1, 1, &message, 245, MAV_COMP_ID_ONBOARD_COMPUTER, 0, MAV_MISSION_TYPE_MISSION);
            send(px4, 14850, message);
            assert(receive(px4, MAVLINK_MSG_ID_MISSION_ITEM_INT, message));
            mavlink_mission_ack_t result{};
            result.target_system = 245; result.target_component = MAV_COMP_ID_ONBOARD_COMPUTER;
            result.type = MAV_MISSION_ACCEPTED;
            mavlink_msg_mission_ack_encode(1, 1, &message, &result); send(px4, 14850, message);
            assert(receive(gcs, MAVLINK_MSG_ID_MISSION_ACK, message));
            assert(contents(output + "/events.jsonl").find("revision_committed") != std::string::npos);
        }
        kill(child, SIGTERM); int status{}; assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        close(px4); close(gcs);
    }
    std::cout << "synthetic transport peer: mixed-frame mediation, unbound rejection, and commit-after-ack passed\n";
}
