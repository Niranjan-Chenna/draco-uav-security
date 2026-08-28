#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <development/mavlink.h>

enum class MavlinkDirection {
    GCS_TO_PX4,
    PX4_TO_GCS
};
struct ParsedMavlinkMessage {
    mavlink_message_t message;
    MavlinkDirection direction;
};

std::vector<ParsedMavlinkMessage> parse_mavlink_data(
    const uint8_t* data,
    std::size_t length,
    MavlinkDirection direction
);