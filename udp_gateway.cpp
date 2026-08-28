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
void run_gateway () {
    using namespace std;
    int sockfd = socket(AF_INET,SOCK_DGRAM,0); //// create ipv4 udp socket
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
    int ready=poll(fds,2,-1);

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
    cout<<"sender port:"<<ntohs(client_addr.sin_port)<<endl; // printing the port number of the client from network to host conversion
    ssize_t bytes_sent = sendto( //sendto is a function that sends data
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
    }
    if (fds[1].revents & POLLIN) {
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
    if (have_gcs) {
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


}


}