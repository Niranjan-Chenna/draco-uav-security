#pragma once
#include <cstdint>
#include <vector>
#include <development/mavlink.h>

struct MissionUploadTransaction {
    bool active{false};
    uint8_t mission_type{0};
    uint16_t expected_count{0};

    std::vector<mavlink_mission_item_int_t> items;

    std::vector<uint8_t> received;

};

void start_mission_upload(
    MissionUploadTransaction& transactions,
    const mavlink_mission_count_t& count
);
void store_mission_item(
    MissionUploadTransaction& transaction,
    const mavlink_mission_item_int_t& item
);

bool mission_upload_complete(
    const MissionUploadTransaction& transaction
);