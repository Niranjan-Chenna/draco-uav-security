#include "canonical_mission.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace {

std::string canonical_float(float value) {

    if (std::isnan(value)) {
        return "nan";
    }

    if (std::isinf(value)) {
        return std::signbit(value)
            ? "-inf"
            : "inf";
    }

    // normalize negative zero
    if (value == 0.0f) {
        value = 0.0f;
    }

    std::ostringstream stream;

    stream.imbue(
        std::locale::classic()
    );

    stream << std::setprecision(
        std::numeric_limits<float>::max_digits10
    );

    stream << value;

    return stream.str();
}

}

CanonicalMission make_canonical_mission(
    const MissionUploadTransaction& transaction
) {
    CanonicalMission mission{};// create an empty mission

    mission.mission_type =//copies normal mission/rally/fence 
        transaction.mission_type;

    mission.items.reserve(//reserving memory
        transaction.items.size()
    );

    for (const auto& raw :// loops every item in the mission to copy
         transaction.items) {

        CanonicalMissionItem item{};// create an empty item

        item.seq = raw.seq;
        item.command = raw.command;
        item.frame = raw.frame;

        item.param1 = raw.param1;
        item.param2 = raw.param2;
        item.param3 = raw.param3;
        item.param4 = raw.param4;

        item.x = raw.x;
        item.y = raw.y;
        item.z = raw.z;

        item.current = raw.current;
        item.autocontinue =
            raw.autocontinue;

        mission.items.push_back(item);
    }

    return mission;
}

std::string serialize_canonical_mission(// for hashing mission helper
    const CanonicalMission& mission
) {
    std::ostringstream output;

    output.imbue(
        std::locale::classic()
    );

    output
        << "mission_type="
        << static_cast<int>(
            mission.mission_type
        )
        << "|count="
        << mission.items.size()
        << '\n';

    for (const auto& item :
         mission.items) {

        output
            << "seq=" << item.seq
            << "|command=" << item.command
            << "|frame="
            << static_cast<int>(item.frame)

            << "|param1="
            << canonical_float(item.param1)

            << "|param2="
            << canonical_float(item.param2)

            << "|param3="
            << canonical_float(item.param3)

            << "|param4="
            << canonical_float(item.param4)

            << "|x=" << item.x
            << "|y=" << item.y

            << "|z="
            << canonical_float(item.z)

            << "|current="
            << static_cast<int>(
                item.current
            )

            << "|autocontinue="
            << static_cast<int>(
                item.autocontinue
            )

            << '\n';
    }

    return output.str();
}