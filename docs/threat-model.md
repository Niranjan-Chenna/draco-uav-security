# DRACO Threat Model — Public Current-State Note

This public document intentionally avoids publishing the project's detailed future research threat model while implementation is ongoing.

## Current verified boundary

The implemented system currently places DRACO between an external/GCS-facing UDP path and PX4 SITL.

```text
External/GCS-facing UDP path
        |
        v
      DRACO
        |
        v
    PX4 SITL
```

DRACO has already demonstrated bidirectional forwarding and reception of live PX4 MAVLink 2 traffic.

## Current security interpretation

At the present implementation stage:

- network source address information is treated as routing metadata, not cryptographic identity;
- successful UDP receipt does not imply sender trust;
- DRACO is not yet claiming command authorization or attack prevention;
- PX4 remains responsible for flight-control and safety behaviour;
- public repository claims are limited to behaviour that has been implemented and directly tested.

## Public scope

Detailed adversary capabilities, planned enforcement mechanisms, unpublished attack scenarios, and future evaluation design are intentionally kept out of the public repository until those components are implemented and validated.
