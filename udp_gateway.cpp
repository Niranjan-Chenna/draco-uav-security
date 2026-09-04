#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <cstdint>
#include <poll.h>
#include "mavlink_parser.h"
#include "semantic_classifier.h"
#include "state_cache.h"
#include "mission_reconstructor.h"
#include "mission_revision.h"
#include "canonical_mission.h"
#include "mission_revision_tracker.h"
#include "mission_delta.h"
#include "mission_intent_contract.h"
#include "mission_revision_causality.h"
#include <optional>
#include <chrono>
#include "mission_authorization.h"
#include "mission_decision_record.h"
#include "mission_change_budget.h"
void run_gateway () {
    using namespace std;
    int sockfd = socket(AF_INET,SOCK_DGRAM,0); // create ipv4 udp socket
    if (sockfd==-1) {
        cout <<"socket creation failed"<<endl;

    }
    else {
        cout<<"socket created successfully"<<endl;
    }
    sockaddr_in server_addr{};  //creating socket address 
    server_addr.sin_family = AF_INET; //telling its ipv4
    server_addr.sin_port=htons(14560); // converting portnumber to network byte order
    server_addr.sin_addr.s_addr=INADDR_ANY; // telling it to accept connections from any ip address
    int bind_result=bind( // bind is a function that binds socket to the address and port number
        sockfd,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)
    );
    if (bind_result==-1) {
        cout<<"bind failed"<<endl;
    }
    else {
        cout<<"bind successful"<<endl;
    }
    char buffer[10240]; // to store the data from client
    sockaddr_in client_addr{}; // creating socket address for client
    socklen_t client_addr_len=sizeof(client_addr);
    int px4_sockfd=socket(AF_INET,SOCK_DGRAM,0);//sockfd for px4
    if (px4_sockfd==-1) {
    cout <<"socket creation failed"<<endl;
}
    else {
    cout<<"socket created successfully"<<endl;}
    sockaddr_in px4_local_addr{};
    px4_local_addr.sin_family = AF_INET;
    px4_local_addr.sin_port = htons(14550);
    px4_local_addr.sin_addr.s_addr = INADDR_ANY;
    int px4_bind_result=bind(
        px4_sockfd,
        reinterpret_cast<sockaddr*>(&px4_local_addr),
        sizeof(px4_local_addr)
    );
    if (px4_bind_result==-1) {
    cout<<"PX4 bind failed"<<endl;  }
    else {
        cout<<"PX4 bind successful"<<endl;
    }

    sockaddr_in px4_target_addr{};// creating socket address for px4 target 
    px4_target_addr.sin_family = AF_INET;
    px4_target_addr.sin_port = htons(18570);//htons for port for px4
    inet_pton(
        AF_INET,
        "127.0.0.1",
        &px4_target_addr.sin_addr
    );

    StateCache state_cache{};// creating an instance of StateCache to store the state of the vehicle
    MissionUploadTransaction mission_upload{}; //to store mission upload
    MissionRevisionTracker mission_revisions{};
    MissionChangeBudget mission_change_budget{};
    std::optional<CanonicalMission> committed_canonical_mission;
    bool px4_authorized_upload_active = false;

    constexpr uint8_t DRACO_SYSID = 245;
    constexpr uint8_t DRACO_COMPID = MAV_COMP_ID_ONBOARD_COMPUTER;
    uint8_t active_gcs_sysid = 0;
    uint8_t active_gcs_compid = 0;
    MissionIntentContract mission_contract{};
    mission_contract.contract_id = 1001;
mission_contract.version = 1;

mission_contract.start_region.center = {
    473979000,
    85470000,
    50.0f
};
mission_contract.start_region.radius_m = 500.0;

mission_contract.terminal_region.center = {
    473984000,
    85470000,
    50.0f
};
mission_contract.terminal_region.radius_m = 500.0;

mission_contract.corridor.centerline = {
    {473970000, 85450000, 50.0f},
    {474000000, 85500000, 50.0f}
};
mission_contract.corridor.allowed_deviation_m = 500.0;

mission_contract.altitude.minimum_m = 0.0f;
mission_contract.altitude.maximum_m = 120.0f;

mission_contract.command_policy.allowed_commands = {
    MAV_CMD_NAV_TAKEOFF,
    MAV_CMD_NAV_WAYPOINT,
    MAV_CMD_NAV_RETURN_TO_LAUNCH,
    MAV_CMD_NAV_LAND
};

mission_contract.emergency_policy.allowed_commands = {
    MAV_CMD_NAV_RETURN_TO_LAUNCH,
    MAV_CMD_NAV_LAND
};

mission_contract.emergency_policy.allow_destination_change = true;

mission_contract.authority_policy.destination_change_authorities = {
    MissionAuthorityTier::SECURITY_ADMIN
};

mission_contract.authority_policy.emergency_authorities = {
    MissionAuthorityTier::EMERGENCY_AUTHORITY,
    MissionAuthorityTier::SECURITY_ADMIN
};

mission_contract.authority_policy.contract_admin_authorities = {
    MissionAuthorityTier::SECURITY_ADMIN
};

mission_contract.allow_in_flight_replanning = true;
mission_contract.destination_change_requires_authority = true;
mission_contract.has_validity_window = false;
    bool proposal_created_for_upload{false};
    pollfd fds[2]{};// creating an array of pollfd structures to monitor multiple file descriptors for events
    //GCS facing socket 
    fds[0].fd = sockfd; 
    fds[0].events = POLLIN; 
    //px4 facing socket
    fds[1].fd = px4_sockfd; 
    fds[1].events = POLLIN; 

    bool have_gcs=false;

    sockaddr_in px4_sender_addr{};
    socklen_t px4_sender_len=sizeof(px4_sender_addr);

   while (true) {
    int ready=poll(fds,2,100);

    if (ready == -1) {
    cout << "poll failed: " << strerror(errno) << endl;
    break;}

    if (fds[0].revents & POLLIN) {//revents is what actually happend
    ssize_t bytes_received = recvfrom( //recvfrom is a function that receives data
    sockfd,
    buffer,
    sizeof(buffer),
    0,
    reinterpret_cast<sockaddr*>(&client_addr),
    &client_addr_len
    );
    bool gcs_mission_upload_packet=false;
    if (bytes_received == -1) {
    cout << "recvfrom failed: "
              << strerror(errno)
              << endl;
            continue;
    } else {have_gcs = true;
    cout << "Received "
              << bytes_received
              << " bytes"
              << endl;
    }
    if (bytes_received > 0) {
    auto parsed_message = parse_mavlink_data(
        reinterpret_cast<const uint8_t*>(buffer),
        static_cast<size_t>(bytes_received),
        MavlinkDirection::GCS_TO_PX4

    );
    
    for (const auto& parsed : parsed_message) {
        const auto& msg = parsed.message;
       
    
        if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {// counting the number of mission items in the mission upload

        mavlink_mission_count_t count{};

         mavlink_msg_mission_count_decode(
        &msg,
        &count
            );

        if (count.mission_type == MAV_MISSION_TYPE_MISSION) {
            active_gcs_sysid = msg.sysid;
            active_gcs_compid = msg.compid;
            gcs_mission_upload_packet = true;
        start_mission_upload(
            mission_upload,
            count
            
        );
        proposal_created_for_upload=false;
        if (count.mission_type == MAV_MISSION_TYPE_MISSION &&
        count.count > 0) {

        mavlink_message_t request_message{};

         mavlink_msg_mission_request_int_pack(
        count.target_system,
        MAV_COMP_ID_AUTOPILOT1,
        &request_message,
        msg.sysid,
        msg.compid,
        0,
        count.mission_type
    );

    uint8_t request_buffer[MAVLINK_MAX_PACKET_LEN];

    uint16_t request_length =
        mavlink_msg_to_send_buffer(
            request_buffer,
            &request_message
        );

    sendto(
        sockfd,
        request_buffer,
        request_length,
        0,
        reinterpret_cast<sockaddr*>(&client_addr),
        sizeof(client_addr)
    );

    std::cout
        << "DRACO_REQUESTED_MISSION_ITEM seq=0"
        << std::endl;
}
    }



    std::cout
        << "MISSION_COUNT"
        << " count=" << count.count
        << " type="
        << static_cast<int>(count.mission_type)
        << std::endl;
}

if (msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT) {// displaying mission item details when received from GCS

    mavlink_mission_item_int_t item{};
    
    
    mavlink_msg_mission_item_int_decode(
        &msg,
        &item
    );
    if (item.mission_type == MAV_MISSION_TYPE_MISSION) {
    gcs_mission_upload_packet = true;
}
    store_mission_item(
    mission_upload,
    item);


    if (!mission_upload_complete(mission_upload)) {

    for (uint16_t next_seq = 0;
         next_seq < mission_upload.expected_count;
         ++next_seq) {

        if (mission_upload.received[next_seq] == 0) {

            mavlink_message_t request_message{};

            mavlink_msg_mission_request_int_pack(
                item.target_system,
                MAV_COMP_ID_AUTOPILOT1,
                &request_message,
                msg.sysid,
                msg.compid,
                next_seq,
                item.mission_type
            );

            uint8_t request_buffer[MAVLINK_MAX_PACKET_LEN];

            uint16_t request_length =
                mavlink_msg_to_send_buffer(
                    request_buffer,
                    &request_message
                );

            sendto(
                sockfd,
                request_buffer,
                request_length,
                0,
                reinterpret_cast<sockaddr*>(&client_addr),
                sizeof(client_addr)
            );

            std::cout
                << "DRACO_REQUESTED_MISSION_ITEM seq="
                << next_seq
                << std::endl;

            break;
        }
    }
}    

     if (!proposal_created_for_upload &&
    mission_upload_complete(mission_upload)) {

    CanonicalMission canonical =
        make_canonical_mission(
            mission_upload
        );

    propose_mission_revision(
        mission_revisions,
        canonical
    );

    proposal_created_for_upload = true;

    std::cout
        << "PROPOSED_REVISION"
        << " id="
        << mission_revisions.proposed->id
        << " hash="
        << mission_revisions.proposed->hash
        << std::endl;
    MissionDelta mission_delta{};

if (committed_canonical_mission.has_value()) {
    mission_delta = compute_mission_delta(
        committed_canonical_mission.value(),
        canonical
    );
}

uint64_t proposal_time_ms =
    static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

MissionProposalRecord proposal_record =
    make_proposal_record(
        mission_revisions.proposed.value(),
        mission_revisions.current,
        "unbound-gcs",
        proposal_time_ms
    );

RevisionCausalityResult causality =
    classify_revision_causality(
        mission_revisions,
        proposal_record,
        mission_delta
    );
MissionAuthorityTier proposal_authority =
    MissionAuthorityTier::NORMAL_OPERATOR;

bool vehicle_in_flight =
    evidence_is_usable(
        state_cache.armed
    ) &&
    evidence_is_usable(
        state_cache.landed_state
    ) &&
    state_cache.armed.value &&
    state_cache.landed_state.value ==
        MAV_LANDED_STATE_IN_AIR;    

MissionChangeBudgetResult change_budget_result =
    evaluate_change_budget(
        mission_delta,
        mission_change_budget,
        proposal_authority
    );

cout << "CHANGE_BUDGET"
     << " within="
     << change_budget_result.within_budget
     << " higher_authority="
     << change_budget_result.requires_higher_authority
     << " reason="
     << change_budget_result.reason
     << endl;

cout << "REVISION_CAUSALITY="
     << revision_causality_name(
            causality.classification
        )
     << endl;

MissionAuthorizationResult authorization =
    evaluate_mission_authorization(
        canonical,
        mission_delta,
        causality,
        change_budget_result,
        mission_contract,
        proposal_authority,
        vehicle_in_flight,
        proposal_time_ms
    );

MissionDecisionRecord decision_record =
    make_mission_decision_record(
        proposal_record,
        mission_delta,
        change_budget_result,
        causality,
        proposal_authority,
        vehicle_in_flight,
        mission_contract,
        mission_change_budget,
        state_cache,
        authorization,
        proposal_time_ms
    );

if (!decision_record.evidence_usable &&
    authorization.decision ==
        MissionAuthorizationDecision::ALLOW) {

    authorization.decision =
        MissionAuthorizationDecision::DEFER;

    authorization.reason =
        "PX4_EVIDENCE_NOT_USABLE";

    decision_record.authorization =
        authorization;
}    

cout << "DECISION_RECORD"
     << " revision_id="
     << decision_record.proposal.revision.id
     << " decision="
     << mission_authorization_decision_name(
            decision_record.authorization.decision
        )
     << " evidence_usable="
     << decision_record.evidence_usable
     << endl;   

cout << "MISSION_AUTHORIZATION="
     << mission_authorization_decision_name(
            authorization.decision
        )
     << " reason="
     << authorization.reason
     << endl;
if (authorization.decision !=
        MissionAuthorizationDecision::ALLOW) {

    // reject the proposed revision locally
    reject_proposed_revision(
        mission_revisions
    );

    // tell qgc that draco rejected the mission
    mavlink_mission_ack_t deny_ack{};

    deny_ack.target_system =
        active_gcs_sysid;

    deny_ack.target_component =
        active_gcs_compid;

    deny_ack.type =
        MAV_MISSION_DENIED;

    deny_ack.mission_type =
        MAV_MISSION_TYPE_MISSION;

    deny_ack.opaque_id = 0;

    mavlink_message_t deny_message{};

    mavlink_msg_mission_ack_encode(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &deny_message,
        &deny_ack
    );

    uint8_t deny_buffer[
        MAVLINK_MAX_PACKET_LEN
    ];

    uint16_t deny_length =
        mavlink_msg_to_send_buffer(
            deny_buffer,
            &deny_message
        );

    sendto(
        sockfd,
        deny_buffer,
        deny_length,
        0,
        reinterpret_cast<sockaddr*>(
            &client_addr
        ),
        sizeof(client_addr)
    );

    cout << "DRACO_DENIED_MISSION"
         << " decision="
         << mission_authorization_decision_name(
                authorization.decision
            )
         << " reason="
         << authorization.reason
         << endl;

    // reset this gcs proposal transaction
    mission_upload.active = false;
    proposal_created_for_upload = false;
    px4_authorized_upload_active = false;
}
if (authorization.decision ==
        MissionAuthorizationDecision::ALLOW &&
    !px4_authorized_upload_active) {

    mavlink_mission_count_t px4_count{};

    px4_count.target_system = 1;
    px4_count.target_component = MAV_COMP_ID_AUTOPILOT1;
    px4_count.count = mission_upload.expected_count;
    px4_count.mission_type = MAV_MISSION_TYPE_MISSION;
    px4_count.opaque_id = 0;

    mavlink_message_t px4_count_message{};

    mavlink_msg_mission_count_encode(
        DRACO_SYSID,
        DRACO_COMPID,
        &px4_count_message,
        &px4_count
    );

    uint8_t px4_count_buffer[MAVLINK_MAX_PACKET_LEN];

    uint16_t px4_count_length =
        mavlink_msg_to_send_buffer(
            px4_count_buffer,
            &px4_count_message
        );

    ssize_t count_sent = sendto(
        px4_sockfd,
        px4_count_buffer,
        px4_count_length,
        0,
        reinterpret_cast<sockaddr*>(&px4_target_addr),
        sizeof(px4_target_addr)
    );

    if (count_sent ==
        static_cast<ssize_t>(px4_count_length)) {

        px4_authorized_upload_active = true;

        cout << "AUTHORIZED_MISSION_UPLOAD_STARTED"
             << endl;
    }
}
}   
    std::cout
        << "MISSION_ITEM"
        << " item_seq=" << item.seq
        << " command=" << item.command
        << " frame="
        << static_cast<int>(item.frame)
        << " x=" << item.x
        << " y=" << item.y
        << " z=" << item.z
        << " type="
        << static_cast<int>(item.mission_type)
        << std::endl;
}
        cout << "GCS MAVLink:"
             << " msgid=" << msg.msgid
             << " sysid=" << static_cast<int>(msg.sysid)
             << " compid=" << static_cast<int>(msg.compid)
             << " seq=" << static_cast<int>(msg.seq)
             << " payload_len=" << static_cast<int>(msg.len)
             << " family="
            << message_family_name(
            classify_message_family(parsed)
        )
            << " operation="
            << semantic_operation_name(
            classify_semantic_operation(parsed)
        )
             << endl;
        



    }
    
}
    
    
    char client_ip[INET_ADDRSTRLEN];//to store ip address of the client
    inet_ntop(//converting the ip address from network to printable form conversion
        AF_INET,
        &client_addr.sin_addr,
        client_ip,
        INET_ADDRSTRLEN
    );
    cout<<"sender ip:"<<client_ip<<endl;
    if (!gcs_mission_upload_packet) {

    ssize_t bytes_sent = sendto(
        px4_sockfd,
        buffer,
        bytes_received,
        0,
        reinterpret_cast<sockaddr*>(&px4_target_addr),
        sizeof(px4_target_addr)
    );

    if (bytes_sent == -1) {
        cout << "sendto failed: "
             << strerror(errno)
             << endl;
    } else {
        cout << "Sent "
             << bytes_sent
             << " bytes"
             << endl;
    }

} else {

    cout << "MISSION_UPLOAD_BUFFERED_BY_DRACO"
         << endl;
}
    }
    if (fds[1].revents & POLLIN) {bool px4_mission_control_packet = false;
    ssize_t px4_bytes_received = recvfrom(
    px4_sockfd,
    buffer,
    sizeof(buffer),
    0,
    reinterpret_cast<sockaddr*>(&px4_sender_addr),
    &px4_sender_len
    );
    if (px4_bytes_received == -1) {
    cout << "PX4 recvfrom failed: "
         << strerror(errno)
         << endl;
        continue;
} else {
    cout << "Received "
         << px4_bytes_received
         << " bytes from PX4 side"
         << endl;
         if (px4_bytes_received > 0 ) {
            auto parsed_message = parse_mavlink_data(// sending it to mavlink parser.h to check if its valid or not
                reinterpret_cast<const uint8_t*>(buffer),
                static_cast<size_t>(px4_bytes_received),
                MavlinkDirection::PX4_TO_GCS
            );
            for (const auto& parsed : parsed_message) {
                const auto& msg = parsed.message;
              
                update_state_cache(state_cache, parsed);
              if (msg.msgid ==
        MAVLINK_MSG_ID_MISSION_REQUEST_INT) {

    mavlink_mission_request_int_t request{};

    mavlink_msg_mission_request_int_decode(
        &msg,
        &request
    );

    cout << "MISSION_REQUEST"
         << " item_seq=" << request.seq
         << " type="
         << static_cast<int>(request.mission_type)
         << endl;

    if (px4_authorized_upload_active &&
        request.mission_type ==
            MAV_MISSION_TYPE_MISSION &&
        request.target_system == DRACO_SYSID) {

        px4_mission_control_packet = true;

        if (request.seq <
            mission_upload.items.size()) {

            mavlink_mission_item_int_t item =
                mission_upload.items[request.seq];

            item.target_system = 1;
            item.target_component =
                MAV_COMP_ID_AUTOPILOT1;

            mavlink_message_t item_message{};

            mavlink_msg_mission_item_int_encode(
                DRACO_SYSID,
                DRACO_COMPID,
                &item_message,
                &item
            );

            uint8_t item_buffer[
                MAVLINK_MAX_PACKET_LEN
            ];

            uint16_t item_length =
                mavlink_msg_to_send_buffer(
                    item_buffer,
                    &item_message
                );

            sendto(
                px4_sockfd,
                item_buffer,
                item_length,
                0,
                reinterpret_cast<sockaddr*>(
                    &px4_target_addr
                ),
                sizeof(px4_target_addr)
            );

            cout << "DRACO_TO_PX4_MISSION_ITEM seq="
                 << request.seq
                 << endl;
        }
    }
}
if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK) {

    mavlink_mission_ack_t ack{};

    mavlink_msg_mission_ack_decode(
        &msg,
        &ack
    );

    if (px4_authorized_upload_active &&
        ack.mission_type ==
            MAV_MISSION_TYPE_MISSION &&
        ack.target_system == DRACO_SYSID) {

        px4_mission_control_packet = true;

        cout << "PX4_MISSION_ACK"
             << " result="
             << static_cast<int>(ack.type)
             << endl;

        if (ack.type ==
                MAV_MISSION_ACCEPTED &&
            mission_upload_complete(
                mission_upload
            )) {

            CanonicalMission canonical =
                make_canonical_mission(
                    mission_upload
                );

            if (commit_proposed_revision(
                    mission_revisions
                )) {

                committed_canonical_mission =
                    canonical;

                cout << "CURRENT_REVISION"
                     << " id="
                     << mission_revisions.current->id
                     << " hash="
                     << mission_revisions.current->hash
                     << endl;

                if (mission_revisions.parent
                        .has_value()) {

                    cout << "PARENT_REVISION"
                         << " id="
                         << mission_revisions.parent->id
                         << " hash="
                         << mission_revisions.parent->hash
                         << endl;

                } else {

                    cout << "PARENT_REVISION none"
                         << endl;
                }
            }

        } else {

            reject_proposed_revision(
                mission_revisions
            );
        }


        // send px4 result back to the original gcs
        mavlink_mission_ack_t gcs_ack{};

        gcs_ack.target_system =
            active_gcs_sysid;

        gcs_ack.target_component =
            active_gcs_compid;

        gcs_ack.type = ack.type;

        gcs_ack.mission_type =
            MAV_MISSION_TYPE_MISSION;

        gcs_ack.opaque_id = ack.opaque_id;

        mavlink_message_t gcs_ack_message{};

        mavlink_msg_mission_ack_encode(
            1,
            MAV_COMP_ID_AUTOPILOT1,
            &gcs_ack_message,
            &gcs_ack
        );

        uint8_t gcs_ack_buffer[
            MAVLINK_MAX_PACKET_LEN
        ];

        uint16_t gcs_ack_length =
            mavlink_msg_to_send_buffer(
                gcs_ack_buffer,
                &gcs_ack_message
            );

        sendto(
            sockfd,
            gcs_ack_buffer,
            gcs_ack_length,
            0,
            reinterpret_cast<sockaddr*>(
                &client_addr
            ),
            sizeof(client_addr)
        );

        cout << "DRACO_TO_GCS_MISSION_ACK"
             << " result="
             << static_cast<int>(gcs_ack.type)
             << endl;

        px4_authorized_upload_active = false;
        mission_upload.active = false;
        proposal_created_for_upload = false;
    }
}

            
            cout << "Library MAVLink:"
             << " msgid=" << msg.msgid
             << " sysid=" << static_cast<int>(msg.sysid)
             << " compid=" << static_cast<int>(msg.compid)
             << " seq=" << static_cast<int>(msg.seq)
             << " payload_len=" << static_cast<int>(msg.len)
                << " family="
            << message_family_name(classify_message_family(parsed))
            << " operation="
            << semantic_operation_name(classify_semantic_operation(parsed))
          
             << endl;
    }
            
           
         }
}
    if (have_gcs &&
    !px4_mission_control_packet) {
    ssize_t bytes_sent_to_gcs = sendto(
    sockfd,
    buffer,
    px4_bytes_received,
    0,
    reinterpret_cast<sockaddr*>(&client_addr),
    sizeof(client_addr)
);
if (bytes_sent_to_gcs == -1) {
    cout << "Failed to send to GCS: "
         << strerror(errno)
         << endl;
} else {
    cout << "Sent "
         << bytes_sent_to_gcs
         << " bytes back to GCS"
         << endl;

}
    }
    }

refresh_state_freshness(state_cache);


}


}