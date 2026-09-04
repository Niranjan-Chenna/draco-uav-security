#include "mission_reconstructor.h"

void start_mission_upload(
    MissionUploadTransaction& transaction,
    const mavlink_mission_count_t& count
) {
    transaction.active = true;

    transaction.mission_type =
        count.mission_type;

    transaction.expected_count =
        count.count;

    transaction.items.clear();
    transaction.received.clear();

    transaction.items.resize(
        count.count
    );

    transaction.received.resize(
        count.count,
        0
    );
}

void store_mission_item(
    MissionUploadTransaction& transaction,
    const mavlink_mission_item_int_t& item
) {
    if (!transaction.active) {
        return;
    }

    if (item.mission_type !=
        transaction.mission_type) {
        return;
    }

    if (item.seq >=
        transaction.expected_count) {
        return;
    }

    transaction.items[item.seq] =
        item;

    transaction.received[item.seq] =
        1;
}

bool mission_upload_complete(
    const MissionUploadTransaction& transaction
) {
    if (!transaction.active) {
        return false;
    }

    if (transaction.expected_count == 0) {
        return true;
    }

    for (uint8_t received :
         transaction.received) {

        if (received == 0) {
            return false;
        }
    }

    return true;
}
