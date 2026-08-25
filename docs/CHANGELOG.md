# Changelog

All notable DRACO design and implementation changes are recorded here.

## [Unreleased]

### Implemented

- Added minimal `main.cpp` entry point.
- Added C++ bidirectional UDP gateway in `udp_gateway.cpp`.
- Added separate GCS-facing and PX4-facing sockets.
- Added `poll()`-based multiplexing for both traffic directions.
- Added GCS → DRACO → PX4 forwarding.
- Added PX4 → DRACO → GCS return forwarding.
- Verified local end-to-end round-trip UDP communication.
- Added `.gitignore` for the local compiled binary.
- Set up PX4-Autopilot and successfully launched PX4 SITL with Gazebo X500.
- Inspected active PX4 MAVLink instances in SITL.

### Research direction revised

- Replaced the original custom authenticated-command-envelope framing as the main research direction.
- Reframed DRACO as **IntentGuard**, a transparent raw-MAVLink/PX4 reference monitor for credential-valid command misuse.
- Added the Mission Intent Contract concept.
- Added mission revision / conflict tracking.
- Added fresh PX4 evidence as an explicit authorization input.
- Added three-valued `ALLOW / DENY / DEFER` policy decisions.
- Added parameter-risk classification and temporal command-interaction tracking.
- Added Command Effect Contracts for command → ACK/state-transition verification.
- Added unexplained-transition detection as an outcome-monitoring research direction.
- Defined initial protected operation families: mission changes, parameter writes, and position/setpoint commands.
- Defined DRACO-SemBench as the credential-valid attack and benign-scenario evaluation harness.
- Set the project implementation/evaluation freeze target to **30 September 2026**.

### Documentation replaced

- Added a root project README with current architecture and Mermaid diagrams.
- Replaced the stale system architecture document.
- Replaced the stale protocol document with the MAVLink mediation design.
- Replaced the original network-attacker-focused threat model with the credential-valid attacker model.
- Replaced the old phased roadmap with the September IntentGuard execution plan.
- Converted `docs/README.md` into a documentation index.

## Earlier concept

The repository originally described a custom HMAC/session/nonce/timestamp command envelope and a small simulated `ARM` / `DISARM` / `TAKEOFF` / `LAND` state machine. That concept is retained only as project history and is no longer the main implementation or novelty direction.
