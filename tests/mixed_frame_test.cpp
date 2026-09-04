#include <cassert>
#include <iostream>
#include "mavlink_parser.h"

int main() {
    mavlink_message_t count{}, heartbeat{};
    mavlink_mission_count_t payload{};
    payload.count = 2;
    mavlink_msg_mission_count_encode(255, 190, &count, &payload);
    mavlink_heartbeat_t hb{};
    mavlink_msg_heartbeat_encode(255, 190, &heartbeat, &hb);
    uint8_t count_bytes[MAVLINK_MAX_PACKET_LEN], heartbeat_bytes[MAVLINK_MAX_PACKET_LEN];
    const auto c = mavlink_msg_to_send_buffer(count_bytes, &count);
    const auto h = mavlink_msg_to_send_buffer(heartbeat_bytes, &heartbeat);
    for (bool reverse : {false, true}) {
        std::vector<uint8_t> mixed;
        if (reverse) mixed.insert(mixed.end(), heartbeat_bytes, heartbeat_bytes + h);
        mixed.insert(mixed.end(), count_bytes, count_bytes + c);
        if (!reverse) mixed.insert(mixed.end(), heartbeat_bytes, heartbeat_bytes + h);
        auto frames = parse_mavlink_data(mixed.data(), mixed.size(), MavlinkDirection::GCS_TO_PX4);
        assert(frames.size() == 2);
        const auto& transparent = frames[reverse ? 0 : 1];
        assert(transparent.message.msgid == MAVLINK_MSG_ID_HEARTBEAT);
        assert(transparent.wire_bytes == std::vector<uint8_t>(heartbeat_bytes, heartbeat_bytes + h));
    }
    assert(parse_mavlink_data(count_bytes, c / 2, MavlinkDirection::GCS_TO_PX4).empty());
    assert(parse_mavlink_data(count_bytes + c / 2, c - c / 2, MavlinkDirection::GCS_TO_PX4).empty());
    assert(parse_mavlink_data(heartbeat_bytes, h, MavlinkDirection::GCS_TO_PX4).size() == 1);
    std::cout << "mixed frames preserve unrelated wire bytes and isolate incomplete datagrams\n";
}
