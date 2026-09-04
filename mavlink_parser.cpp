#include "mavlink_parser.h"

std::vector<ParsedMavlinkMessage> parse_mavlink_data(
    const uint8_t* data, std::size_t length, MavlinkDirection direction) {
    std::vector<ParsedMavlinkMessage> messages;
    mavlink_message_t receive{}, message{};
    mavlink_status_t parser{}, status{};
    // udp frames must be complete within a datagram; state cannot leak between senders.
    for (std::size_t i = 0; i < length; ++i) {
        if (mavlink_frame_char_buffer(&receive, &parser, data[i], &message, &status) == MAVLINK_FRAMING_OK) {
            const auto wire_length = mavlink_msg_get_send_buffer_length(&message);
            if (wire_length <= i + 1)
                messages.push_back({message, direction,
                    std::vector<uint8_t>(data + i + 1 - wire_length, data + i + 1)});
        }
    }
    return messages;
}
