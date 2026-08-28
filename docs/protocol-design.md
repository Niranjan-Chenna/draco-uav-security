# DRACO MAVLink Traffic Inspection — Verified Current State

This public document records only protocol-level behaviour that has already been directly observed in the current implementation.

## Transport

DRACO currently forwards UDP datagrams between its external/GCS-facing endpoint and PX4-facing endpoint.

PX4 SITL is running MAVLink 2 over UDP. The relevant tested PX4 instance reported:

```text
local UDP port: 18570
remote UDP port: 14550
MAVLink version: 2
```

## Verified MAVLink 2 observations

Live PX4 traffic received by DRACO has been inspected directly.

Observed/verified header elements include:

- magic byte `0xFD` for MAVLink 2;
- payload length from byte 1;
- sequence number from byte 4;
- system ID from byte 5;
- component ID from byte 6;
- 24-bit message ID reconstructed from bytes 7-9.

For unsigned MAVLink 2 frames observed during testing, received frame size matched:

```text
10-byte header
+ payload length
+ 2-byte checksum
```

## Current implementation note

The manual byte inspection was used only to verify that DRACO is receiving valid live MAVLink 2 framing from PX4 SITL. It is not intended to become a complete hand-written MAVLink implementation.

## Public scope

Future semantic parsing, authorization logic, state reasoning, and research mechanisms are intentionally not described here until they are implemented and tested.
