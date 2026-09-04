#include "mission_delta.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr double EARTH_RADIUS_M = 6371000.0;
constexpr double PI = 3.14159265358979323846;

using MatchPair =
    std::pair<std::size_t, std::size_t>;

double degrees_from_mavlink(int32_t value) {
    return static_cast<double>(value) / 1e7;
}

double horizontal_distance_m(
    const CanonicalMissionItem& a,
    const CanonicalMissionItem& b
) {
    double lat1 =
        degrees_from_mavlink(a.x) * PI / 180.0;

    double lat2 =
        degrees_from_mavlink(b.x) * PI / 180.0;

    double dlat =
        (degrees_from_mavlink(b.x) -
         degrees_from_mavlink(a.x)) * PI / 180.0;

    double dlon =
        (degrees_from_mavlink(b.y) -
         degrees_from_mavlink(a.y)) * PI / 180.0;

    double h =
        std::sin(dlat / 2.0) * std::sin(dlat / 2.0)
        +
        std::cos(lat1) * std::cos(lat2) *
        std::sin(dlon / 2.0) * std::sin(dlon / 2.0);

    return 2.0 * EARTH_RADIUS_M *
           std::atan2(
               std::sqrt(h),
               std::sqrt(1.0 - h)
           );
}

bool parameters_differ(
    const CanonicalMissionItem& a,
    const CanonicalMissionItem& b
) {
    return
        a.param1 != b.param1 ||
        a.param2 != b.param2 ||
        a.param3 != b.param3 ||
        a.param4 != b.param4 ||
        a.frame != b.frame ||
        a.current != b.current ||
        a.autocontinue != b.autocontinue;
}

bool same_semantic_item(
    const CanonicalMissionItem& a,
    const CanonicalMissionItem& b
) {
    return
        a.command == b.command &&
        a.frame == b.frame &&
        a.param1 == b.param1 &&
        a.param2 == b.param2 &&
        a.param3 == b.param3 &&
        a.param4 == b.param4 &&
        a.x == b.x &&
        a.y == b.y &&
        a.z == b.z &&
        a.current == b.current &&
        a.autocontinue == b.autocontinue;
}

bool likely_same_logical_item(
    const CanonicalMissionItem& a,
    const CanonicalMissionItem& b
) {
    bool same_position =
        a.x == b.x &&
        a.y == b.y;

    bool same_command =
        a.command == b.command &&
        a.frame == b.frame;

    return same_position || same_command;
}

bool is_critical_command(uint16_t command) {
    return
        command == MAV_CMD_NAV_LAND ||
        command == MAV_CMD_NAV_RETURN_TO_LAUNCH;
}

bool has_type(
    const MissionItemChange& change,
    MissionDeltaType type
) {
    return std::find(
        change.types.begin(),
        change.types.end(),
        type
    ) != change.types.end();
}

std::vector<MatchPair> align_exact_items(
    const CanonicalMission& current,
    const CanonicalMission& proposed
) {
    std::size_t n = current.items.size();
    std::size_t m = proposed.items.size();

    std::vector<std::vector<std::size_t>> dp(
        n + 1,
        std::vector<std::size_t>(m + 1, 0)
    );

    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {

            if (same_semantic_item(
                    current.items[i - 1],
                    proposed.items[j - 1])) {

                dp[i][j] =
                    dp[i - 1][j - 1] + 1;

            } else {

                dp[i][j] =
                    std::max(
                        dp[i - 1][j],
                        dp[i][j - 1]
                    );
            }
        }
    }

    std::vector<MatchPair> matches;

    std::size_t i = n;
    std::size_t j = m;

    while (i > 0 && j > 0) {

        if (same_semantic_item(
                current.items[i - 1],
                proposed.items[j - 1])) {

            matches.push_back({i - 1, j - 1});
            --i;
            --j;

        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            --i;
        } else {
            --j;
        }
    }

    std::reverse(matches.begin(), matches.end());

    return matches;
}

MissionItemChange classify_change(
    const CanonicalMissionItem& old_item,
    const CanonicalMissionItem& new_item,
    std::size_t old_index,
    std::size_t new_index
) {
    MissionItemChange change{};

    change.has_old_item = true;
    change.has_new_item = true;

    change.old_item = old_item;
    change.new_item = new_item;

    change.old_index = old_index;
    change.new_index = new_index;

    change.horizontal_displacement_m =
        horizontal_distance_m(old_item, new_item);

    change.altitude_delta_m =
        static_cast<double>(
            new_item.z - old_item.z
        );

    if (change.horizontal_displacement_m > 0.0) {
        change.types.push_back(
            MissionDeltaType::MOVE_HORIZONTAL
        );
    }

    if (change.altitude_delta_m != 0.0) {
        change.types.push_back(
            MissionDeltaType::ALTITUDE_CHANGE
        );
    }

    if (old_item.command != new_item.command) {
        change.types.push_back(
            MissionDeltaType::COMMAND_CHANGE
        );
    }

    if (parameters_differ(old_item, new_item)) {
        change.types.push_back(
            MissionDeltaType::PARAMETER_CHANGE
        );
    }

    return change;
}

void add_insert(
    MissionDelta& delta,
    const CanonicalMissionItem& item,
    std::size_t index
) {
    MissionItemChange change{};

    change.types.push_back(
        MissionDeltaType::INSERT
    );

    change.has_new_item = true;
    change.new_item = item;
    change.new_index = index;

    delta.changes.push_back(change);
}

void add_delete(
    MissionDelta& delta,
    const CanonicalMissionItem& item,
    std::size_t index
) {
    MissionItemChange change{};

    change.types.push_back(
        MissionDeltaType::DELETE
    );

    change.has_old_item = true;
    change.old_item = item;
    change.old_index = index;

    delta.changes.push_back(change);
}

void add_reorder(
    MissionDelta& delta,
    const CanonicalMissionItem& item,
    std::size_t old_index,
    std::size_t new_index
) {
    MissionItemChange change{};

    change.types.push_back(
        MissionDeltaType::REORDER
    );

    change.has_old_item = true;
    change.has_new_item = true;

    change.old_item = item;
    change.new_item = item;

    change.old_index = old_index;
    change.new_index = new_index;

    delta.changes.push_back(change);
}

bool destination_changed(
    const CanonicalMission& current,
    const CanonicalMission& proposed
) {
    if (current.items.empty() &&
        proposed.items.empty()) {
        return false;
    }

    if (current.items.empty() ||
        proposed.items.empty()) {
        return true;
    }

    const auto& old_terminal =
        current.items.back();

    const auto& new_terminal =
        proposed.items.back();

    return
        old_terminal.command != new_terminal.command ||
        old_terminal.x != new_terminal.x ||
        old_terminal.y != new_terminal.y ||
        old_terminal.z != new_terminal.z;
}

void calculate_summary(
    MissionDelta& delta,
    const CanonicalMission& current,
    const CanonicalMission& proposed
) {
    std::size_t item_changes =
        delta.changes.size();

    for (const auto& change : delta.changes) {

        if (has_type(change, MissionDeltaType::INSERT)) {
            ++delta.summary.inserted;
        }

        if (has_type(change, MissionDeltaType::DELETE)) {
            ++delta.summary.deleted;
        }

        if (has_type(
                change,
                MissionDeltaType::MOVE_HORIZONTAL)) {

            ++delta.summary.moved_horizontal;

            delta.summary.maximum_horizontal_displacement_m =
                std::max(
                    delta.summary.maximum_horizontal_displacement_m,
                    change.horizontal_displacement_m
                );
        }

        if (has_type(
                change,
                MissionDeltaType::ALTITUDE_CHANGE)) {

            ++delta.summary.altitude_changed;

            delta.summary.maximum_altitude_change_m =
                std::max(
                    delta.summary.maximum_altitude_change_m,
                    std::abs(change.altitude_delta_m)
                );
        }

        if (has_type(
                change,
                MissionDeltaType::COMMAND_CHANGE)) {

            ++delta.summary.command_changed;
        }

        if (has_type(
                change,
                MissionDeltaType::PARAMETER_CHANGE)) {

            ++delta.summary.parameter_changed;
        }

        if (has_type(
                change,
                MissionDeltaType::REORDER)) {

            ++delta.summary.reordered;
        }

        if (change.has_new_item &&
            is_critical_command(
                change.new_item.command) &&
            (!change.has_old_item ||
             change.old_item.command !=
             change.new_item.command)) {

            delta.summary.critical_command_introduced =
                true;
        }
    }

    std::size_t mission_size =
        std::max(
            current.items.size(),
            proposed.items.size()
        );

    if (mission_size > 0) {
        delta.summary.changed_item_ratio =
            static_cast<double>(item_changes) /
            static_cast<double>(mission_size);
    }

    delta.summary.destination_changed =
        destination_changed(
            current,
            proposed
        );

    if (delta.summary.destination_changed) {

        MissionItemChange change{};

        change.types.push_back(
            MissionDeltaType::DESTINATION_CHANGE
        );

        delta.changes.push_back(change);
    }

    // task 5 baseline:
    // complete semantic replacement is major.
    // task 10 later controls partial-change budgets.
    if (mission_size > 0 &&
        delta.summary.changed_item_ratio >= 1.0) {

        delta.summary.major_replacement = true;

        MissionItemChange change{};

        change.types.push_back(
            MissionDeltaType::MAJOR_REPLACEMENT
        );

        delta.changes.push_back(change);
    }
}

}

MissionDelta compute_mission_delta(
    const CanonicalMission& current,
    const CanonicalMission& proposed
) {
    MissionDelta delta{};

    auto matches =
        align_exact_items(
            current,
            proposed
        );

    std::vector<bool> old_used(
        current.items.size(),
        false
    );

    std::vector<bool> new_used(
        proposed.items.size(),
        false
    );

    for (const auto& match : matches) {
        old_used[match.first] = true;
        new_used[match.second] = true;
    }

    // exact items outside the aligned sequence are reorder candidates
    for (std::size_t i = 0;
         i < current.items.size();
         ++i) {

        if (old_used[i]) continue;

        for (std::size_t j = 0;
             j < proposed.items.size();
             ++j) {

            if (new_used[j]) continue;

            if (same_semantic_item(
                    current.items[i],
                    proposed.items[j])) {

                add_reorder(
                    delta,
                    current.items[i],
                    i,
                    j
                );

                old_used[i] = true;
                new_used[j] = true;

                break;
            }
        }
    }

    // pair the best remaining logical items
    while (true) {

        double best_score =
            std::numeric_limits<double>::infinity();

        std::size_t best_old = 0;
        std::size_t best_new = 0;
        bool found = false;

        for (std::size_t i = 0;
             i < current.items.size();
             ++i) {

            if (old_used[i]) continue;

            for (std::size_t j = 0;
                 j < proposed.items.size();
                 ++j) {

                if (new_used[j]) continue;

                if (!likely_same_logical_item(
                        current.items[i],
                        proposed.items[j])) {
                    continue;
                }

                bool same_position =
                    current.items[i].x ==
                    proposed.items[j].x &&
                    current.items[i].y ==
                    proposed.items[j].y;

                double score =
                    same_position
                    ? 0.0
                    : 100.0 +
                      horizontal_distance_m(
                          current.items[i],
                          proposed.items[j]
                      );

                score +=
                    std::abs(
                        static_cast<double>(
                            proposed.items[j].z -
                            current.items[i].z
                        )
                    ) * 0.01;

                score +=
                    static_cast<double>(
                        (i > j) ? i - j : j - i
                    ) * 0.001;

                if (score < best_score) {
                    best_score = score;
                    best_old = i;
                    best_new = j;
                    found = true;
                }
            }
        }

        if (!found) {
            break;
        }

        MissionItemChange change =
            classify_change(
                current.items[best_old],
                proposed.items[best_new],
                best_old,
                best_new
            );

        if (!change.types.empty()) {
            delta.changes.push_back(change);
        }

        old_used[best_old] = true;
        new_used[best_new] = true;
    }

    for (std::size_t i = 0;
         i < current.items.size();
         ++i) {

        if (!old_used[i]) {
            add_delete(
                delta,
                current.items[i],
                i
            );
        }
    }

    for (std::size_t i = 0;
         i < proposed.items.size();
         ++i) {

        if (!new_used[i]) {
            add_insert(
                delta,
                proposed.items[i],
                i
            );
        }
    }

    delta.no_op =
        delta.changes.empty();

    calculate_summary(
        delta,
        current,
        proposed
    );

    return delta;
}

const char* mission_delta_type_name(
    MissionDeltaType type
) {
    switch (type) {

        case MissionDeltaType::NO_OP:
            return "NO_OP";

        case MissionDeltaType::INSERT:
            return "INSERT";

        case MissionDeltaType::DELETE:
            return "DELETE";

        case MissionDeltaType::MOVE_HORIZONTAL:
            return "MOVE_HORIZONTAL";

        case MissionDeltaType::ALTITUDE_CHANGE:
            return "ALTITUDE_CHANGE";

        case MissionDeltaType::COMMAND_CHANGE:
            return "COMMAND_CHANGE";

        case MissionDeltaType::PARAMETER_CHANGE:
            return "PARAMETER_CHANGE";

        case MissionDeltaType::REORDER:
            return "REORDER";

        case MissionDeltaType::DESTINATION_CHANGE:
            return "DESTINATION_CHANGE";

        case MissionDeltaType::MAJOR_REPLACEMENT:
            return "MAJOR_REPLACEMENT";
    }

    return "UNKNOWN";
}