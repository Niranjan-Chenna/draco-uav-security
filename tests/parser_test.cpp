#include <iostream>
#include "mavlink_parser.h"

int main() {
    uint8_t bad_data[] = {
        0xFD, 0x20, 0x00, 0x00, 0x01
    };

    auto messages = parse_mavlink_data(
        bad_data,
        sizeof(bad_data),
        MavlinkDirection::GCS_TO_PX4
    );

    std::cout << "parsed messages: "
              << messages.size()
              << std::endl;

    return 0;
}