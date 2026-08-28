#include "semantic_classifier.h"


// broad message family

MessageFamily classify_message_family(
    const ParsedMavlinkMessage& parsed
) {
    const auto& msg = parsed.message;


    // command messages
    if (
        msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG ||
        msg.msgid == MAVLINK_MSG_ID_COMMAND_INT
    ) {
        return MessageFamily::COMMAND;
    }


    // parameter writes
    if (
        msg.msgid == MAVLINK_MSG_ID_PARAM_SET ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_EXT_SET
    ) {
        return MessageFamily::PARAMETER;
    }


    // mission operations
    if (
        msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_CLEAR_ALL ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_SET_CURRENT ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_WRITE_PARTIAL_LIST
    ) {
        return MessageFamily::MISSION;
    }


    // position and setpoint operations
    if (
        msg.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED ||
        msg.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT
    ) {
        return MessageFamily::POSITION_OR_SETPOINT;
    }


    // mode and direct control
    if (
        msg.msgid == MAVLINK_MSG_ID_SET_MODE ||
        msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL ||
        msg.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE
    ) {
        return MessageFamily::MODE_OR_CONTROL;
    }


    // acknowledgements and responses
    if (
        msg.msgid == MAVLINK_MSG_ID_COMMAND_ACK ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_ACK ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_VALUE ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_EXT_ACK
    ) {
        return MessageFamily::ACK_OR_RESPONSE;
    }


    // px4 state evidence
    if (
        parsed.direction == MavlinkDirection::PX4_TO_GCS &&
        (
            msg.msgid == MAVLINK_MSG_ID_HEARTBEAT ||
            msg.msgid == MAVLINK_MSG_ID_SYS_STATUS ||
            msg.msgid == MAVLINK_MSG_ID_ATTITUDE ||
            msg.msgid == MAVLINK_MSG_ID_ATTITUDE_QUATERNION ||
            msg.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT ||
            msg.msgid == MAVLINK_MSG_ID_LOCAL_POSITION_NED ||
            msg.msgid == MAVLINK_MSG_ID_EXTENDED_SYS_STATE
        )
    ) {
        return MessageFamily::STATE_EVIDENCE;
    }


    // read-only requests
    if (
        msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_EXT_REQUEST_READ ||
        msg.msgid == MAVLINK_MSG_ID_PARAM_EXT_REQUEST_LIST ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_LIST ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT ||
        msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_PARTIAL_LIST ||
        msg.msgid == MAVLINK_MSG_ID_REQUEST_EVENT
    ) {
        return MessageFamily::READ_ONLY;
    }


    return MessageFamily::OTHER;
}


// map mav_cmd values into draco operations

static SemanticOperation map_mav_cmd(
    uint16_t command,
    float param1
) {
    switch (command) {

        case MAV_CMD_COMPONENT_ARM_DISARM:

            if (param1 > 0.5f) {
                return SemanticOperation::ARM;
            }

            return SemanticOperation::DISARM;


        case MAV_CMD_NAV_TAKEOFF:
            return SemanticOperation::TAKEOFF;


        case MAV_CMD_NAV_LAND:
            return SemanticOperation::LAND;


        case MAV_CMD_NAV_RETURN_TO_LAUNCH:
            return SemanticOperation::RTL;


        case MAV_CMD_DO_SET_MODE:
            return SemanticOperation::MODE_CHANGE;


        case MAV_CMD_DO_REPOSITION:
            return SemanticOperation::POSITION_TARGET;


        // request_message asks px4 to send information
        case MAV_CMD_REQUEST_MESSAGE:
            return SemanticOperation::READ_ONLY;


        // unknown command inside a command container is treated conservatively
        default:
            return SemanticOperation::UNKNOWN_WRITE;
    }
}


// actual semantic operation

SemanticOperation classify_semantic_operation(
    const ParsedMavlinkMessage& parsed
) {
    const auto& msg = parsed.message;


    // decode command_long from gcs
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        msg.msgid == MAVLINK_MSG_ID_COMMAND_LONG
    ) {
        mavlink_command_long_t command{};

        mavlink_msg_command_long_decode(
            &msg,
            &command
        );

        return map_mav_cmd(
            command.command,
            command.param1
        );
    }


    // decode command_int from gcs
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        msg.msgid == MAVLINK_MSG_ID_COMMAND_INT
    ) {
        mavlink_command_int_t command{};

        mavlink_msg_command_int_decode(
            &msg,
            &command
        );

        return map_mav_cmd(
            command.command,
            command.param1
        );
    }


    // parameter write
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        (
            msg.msgid == MAVLINK_MSG_ID_PARAM_SET ||
            msg.msgid == MAVLINK_MSG_ID_PARAM_EXT_SET
        )
    ) {
        return SemanticOperation::PARAMETER_WRITE;
    }


    // mission change
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        (
            msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT ||
            msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM ||
            msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT ||
            msg.msgid == MAVLINK_MSG_ID_MISSION_CLEAR_ALL ||
            msg.msgid == MAVLINK_MSG_ID_MISSION_SET_CURRENT ||
            msg.msgid == MAVLINK_MSG_ID_MISSION_WRITE_PARTIAL_LIST
        )
    ) {
        return SemanticOperation::MISSION_CHANGE;
    }


    // position target
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        (
            msg.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_LOCAL_NED ||
            msg.msgid == MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT
        )
    ) {
        return SemanticOperation::POSITION_TARGET;
    }


    // mode change
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        msg.msgid == MAVLINK_MSG_ID_SET_MODE
    ) {
        return SemanticOperation::MODE_CHANGE;
    }


    // direct control
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        (
            msg.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL ||
            msg.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE
        )
    ) {
        return SemanticOperation::DIRECT_CONTROL;
    }


    MessageFamily family =
        classify_message_family(parsed);


    // read-only traffic
    if (family == MessageFamily::READ_ONLY) {
        return SemanticOperation::READ_ONLY;
    }


    // acknowledgements and responses
    if (family == MessageFamily::ACK_OR_RESPONSE) {
        return SemanticOperation::ACK_OR_RESPONSE;
    }


    // px4 state evidence
    if (family == MessageFamily::STATE_EVIDENCE) {
        return SemanticOperation::STATE_EVIDENCE;
    }


    // harmless gcs protocol traffic
    if (
        parsed.direction == MavlinkDirection::GCS_TO_PX4 &&
        (
            msg.msgid == MAVLINK_MSG_ID_HEARTBEAT ||
            msg.msgid == MAVLINK_MSG_ID_PING ||
            msg.msgid == MAVLINK_MSG_ID_SYSTEM_TIME ||
            msg.msgid == MAVLINK_MSG_ID_TIMESYNC
        )
    ) {
        return SemanticOperation::OTHER;
    }


    // unknown traffic arriving from the gcs side is treated conservatively
    if (parsed.direction == MavlinkDirection::GCS_TO_PX4) {
        return SemanticOperation::UNKNOWN_WRITE;
    }


    // unknown px4 telemetry remains other
    return SemanticOperation::OTHER;
}


// printable family name

const char* message_family_name(
    MessageFamily family
) {
    switch (family) {

        case MessageFamily::READ_ONLY:
            return "READ_ONLY";

        case MessageFamily::COMMAND:
            return "COMMAND";

        case MessageFamily::PARAMETER:
            return "PARAMETER";

        case MessageFamily::MISSION:
            return "MISSION";

        case MessageFamily::POSITION_OR_SETPOINT:
            return "POSITION_OR_SETPOINT";

        case MessageFamily::MODE_OR_CONTROL:
            return "MODE_OR_CONTROL";

        case MessageFamily::ACK_OR_RESPONSE:
            return "ACK_OR_RESPONSE";

        case MessageFamily::STATE_EVIDENCE:
            return "STATE_EVIDENCE";

        case MessageFamily::OTHER:
            return "OTHER";
    }

    return "OTHER";
}


// printable semantic operation name

const char* semantic_operation_name(
    SemanticOperation operation
) {
    switch (operation) {

        case SemanticOperation::READ_ONLY:
            return "READ_ONLY";

        case SemanticOperation::ARM:
            return "ARM";

        case SemanticOperation::DISARM:
            return "DISARM";

        case SemanticOperation::TAKEOFF:
            return "TAKEOFF";

        case SemanticOperation::LAND:
            return "LAND";

        case SemanticOperation::RTL:
            return "RTL";

        case SemanticOperation::MODE_CHANGE:
            return "MODE_CHANGE";

        case SemanticOperation::DIRECT_CONTROL:
            return "DIRECT_CONTROL";

        case SemanticOperation::PARAMETER_WRITE:
            return "PARAMETER_WRITE";

        case SemanticOperation::MISSION_CHANGE:
            return "MISSION_CHANGE";

        case SemanticOperation::POSITION_TARGET:
            return "POSITION_TARGET";

        case SemanticOperation::ACK_OR_RESPONSE:
            return "ACK_OR_RESPONSE";

        case SemanticOperation::STATE_EVIDENCE:
            return "STATE_EVIDENCE";

        case SemanticOperation::UNKNOWN_WRITE:
            return "UNKNOWN_WRITE";

        case SemanticOperation::OTHER:
            return "OTHER";
    }

    return "OTHER";
}