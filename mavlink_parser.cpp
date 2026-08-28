#include "mavlink_parser.h"

std::vector<ParsedMavlinkMessage> parse_mavlink_data(
    const uint8_t* data,
    std::size_t length,
    MavlinkDirection direction
) {
    std::vector<ParsedMavlinkMessage> messages;
    uint8_t channel =(direction == MavlinkDirection::GCS_TO_PX4) ? 0 : 1; // Assuming channel 0 for GCS_TO_PX4 and channel 1 for PX4_TO_GCS
    mavlink_message_t message{};
    mavlink_status_t status{};

    for (std::size_t i = 0; i < length; ++i) {

        uint8_t result = mavlink_parse_char(
            channel,// channel is the communication channel
            data[i],//  data[i] is the byte of data being parsed
            &message,//  message is the output parameter where the parsed message will be stored
            &status// status is the output parameter where the parsing status will be stored
        );

        if (result == MAVLINK_FRAMING_OK) {
            messages.push_back({message, direction});
        }
    }

    return messages;
}