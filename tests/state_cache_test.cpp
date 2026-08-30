#include "../state_cache.h"

#include <chrono>
#include <iostream>

int main() {

    // freshness tests
    StateCache cache;

    refresh_state_freshness(cache);

    std::cout
        << "unknown freshness: "
        << static_cast<int>(
            cache.armed.freshness
        )
        << std::endl;

    cache.armed.valid = true;
    cache.armed.observed_at =
        std::chrono::steady_clock::now();

    refresh_state_freshness(cache);

    std::cout
        << "fresh freshness: "
        << static_cast<int>(
            cache.armed.freshness
        )
        << std::endl;

    cache.armed.observed_at =
        std::chrono::steady_clock::now()
        - std::chrono::seconds(4);

    refresh_state_freshness(cache);

    std::cout
        << "stale freshness: "
        << static_cast<int>(
            cache.armed.freshness
        )
        << std::endl;


    // evidence usability tests
    StateCache usability_cache;

    std::cout
        << "unknown usable: "
        << evidence_is_usable(
            usability_cache.local_position
        )
        << std::endl;

    usability_cache.local_position.valid = true;
    usability_cache.local_position.freshness =
        EvidenceFreshness::FRESH;

    std::cout
        << "fresh usable: "
        << evidence_is_usable(
            usability_cache.local_position
        )
        << std::endl;

    usability_cache.local_position.freshness =
        EvidenceFreshness::STALE;

    std::cout
        << "stale usable: "
        << evidence_is_usable(
            usability_cache.local_position
        )
        << std::endl;

    usability_cache.local_position.valid = false;
    usability_cache.local_position.freshness =
        EvidenceFreshness::INVALID;

    std::cout
        << "invalid usable: "
        << evidence_is_usable(
            usability_cache.local_position
        )
        << std::endl;
StateCache snapshot_cache;

snapshot_cache.armed.value = false;
snapshot_cache.armed.valid = true;
snapshot_cache.armed.observed_at =
    std::chrono::steady_clock::now();
snapshot_cache.armed.freshness =
    EvidenceFreshness::FRESH;

const EvidenceSnapshot snapshot =
    make_evidence_snapshot(snapshot_cache);

// change the live cache after taking the snapshot
snapshot_cache.armed.value = true;

std::cout
    << "live armed: "
    << snapshot_cache.armed.value
    << std::endl;

std::cout
    << "snapshot armed: "
    << snapshot.state.armed.value
    << std::endl;
    return 0;
}