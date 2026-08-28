# DRACO Development Progress

This public document records **completed and verified milestones only**. Detailed future implementation phases and unpublished research plans are intentionally kept out of the public repository.

## Completed milestones

### Development environment

- [x] WSL2 + Ubuntu 24.04 development environment configured.
- [x] GCC/G++ toolchain installed and verified.
- [x] PX4-Autopilot source installed.
- [x] PX4 SITL with Gazebo X500 built and launched successfully.

### C++ gateway foundation

- [x] Minimal `main.cpp` entry point created.
- [x] Gateway logic separated into `udp_gateway.cpp`.
- [x] GCS-facing IPv4 UDP socket implemented.
- [x] PX4-facing IPv4 UDP socket implemented.
- [x] Both sockets bound successfully.
- [x] `poll()` added for single-threaded monitoring of both directions.
- [x] GCS → DRACO → PX4-side forwarding implemented.
- [x] PX4-side → DRACO → GCS forwarding implemented.
- [x] Complete bidirectional UDP round-trip verified with local test endpoints.

### PX4 SITL integration

- [x] Active PX4 MAVLink instance inspected with `mavlink status`.
- [x] Relevant PX4 SITL UDP ports identified.
- [x] DRACO PX4-facing socket connected to the real PX4 SITL MAVLink path.
- [x] Continuous live PX4 UDP traffic received by DRACO.

### MAVLink traffic inspection

- [x] MAVLink 2 framing confirmed using the `0xFD` magic byte.
- [x] Payload length inspected from live frames.
- [x] 24-bit message ID reconstructed from the MAVLink 2 header.
- [x] System ID inspected.
- [x] Component ID inspected.
- [x] Sequence number inspected.

## Public documentation policy

Only completed work, directly observed behaviour, and verified implementation status are documented here. Future milestones are intentionally omitted until they are implemented and tested.
