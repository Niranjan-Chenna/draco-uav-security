#pragma once

#include "mavlink_parser.h"

// Broad MAVLink family
enum class MessageFamily {
    READ_ONLY,
    COMMAND,
    PARAMETER,
    MISSION,
    POSITION_OR_SETPOINT,
    MODE_OR_CONTROL,
    ACK_OR_RESPONSE,
    STATE_EVIDENCE,
    OTHER
};

// Actual operation DRACO cares about
enum class SemanticOperation {
    READ_ONLY,

    ARM,
    DISARM,
    TAKEOFF,
    LAND,
    RTL,

    MODE_CHANGE,
    DIRECT_CONTROL,

    PARAMETER_WRITE,
    MISSION_CHANGE,
    POSITION_TARGET,

    ACK_OR_RESPONSE,
    STATE_EVIDENCE,

    UNKNOWN_WRITE,
    OTHER
};

MessageFamily classify_message_family(
    const ParsedMavlinkMessage& parsed
);

SemanticOperation classify_semantic_operation(
    const ParsedMavlinkMessage& parsed
);

const char* message_family_name(
    MessageFamily family
);

const char* semantic_operation_name(
    SemanticOperation operation
);