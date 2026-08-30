# DRACO — UAV-Side MAVLink Security Gateway

DRACO is an active research prototype built around a C++ UDP gateway placed transparently between QGroundControl and PX4 SITL.

This public repository intentionally documents **only work that has already been implemented or directly verified**. Detailed future research phases and unpublished design ideas are kept out of the public repository while development is ongoing.

## Current Implementation Status

Four implementation milestones have been completed and directly verified:

- Phase 0 — Transport Foundation ✅
- Phase 1 — Transparent QGroundControl ↔ DRACO ↔ PX4 SITL Path ✅
- Phase 2 — MAVLink Parsing and Semantic Classification ✅
- Phase 3 — Trusted PX4 Evidence and Security Context ✅

DRACO currently provides a transparent bidirectional MAVLink path while parsing and classifying observed traffic, maintaining a PX4-derived evidence cache, tracking evidence freshness, and creating frozen evidence snapshots without re-encoding the forwarded frames.

## Verified Data Path

```mermaid
flowchart LR
    QGC["QGroundControl<br/>Windows GCS"]
    GCS["DRACO GCS-facing socket<br/>UDP :14560"]
    PARSER["MAVLink parser +<br/>semantic classifier"]
    CACHE["PX4 evidence<br/>StateCache"]
    PX4SIDE["DRACO PX4-facing socket<br/>UDP :14550"]
    PX4["PX4 SITL + Gazebo X500<br/>MAVLink UDP :18570"]

    QGC <--> GCS
    GCS --> PARSER
    PARSER --> GCS
    PARSER --> CACHE
    GCS <--> PX4SIDE
    PX4SIDE <--> PX4
```

The tested communication path is:

```text
QGroundControl
      ↕
DRACO UDP :14560
      ↕
DRACO UDP :14550
      ↕
PX4 SITL UDP :18570
```

Parsing, classification and state observation operate alongside the transport path. Forwarding continues to use the original received byte buffer rather than a reconstructed MAVLink frame.

## Phase 0 — Transport Foundation

The following transport work has been implemented and tested:

- C++ project builds successfully under Ubuntu 24.04 on WSL2
- Minimal `main.cpp` entry point launches the gateway
- Separate `udp_gateway.cpp` contains the UDP gateway logic
- GCS-facing IPv4 UDP socket bound to UDP `:14560`
- PX4-facing IPv4 UDP socket bound to UDP `:14550`
- `poll()` monitors both sockets in a single thread
- GCS → DRACO → PX4 forwarding implemented
- PX4 → DRACO → remembered GCS endpoint forwarding implemented
- Bidirectional UDP communication verified

## Phase 1 — Transparent Real-GCS Path

The transparent QGroundControl/PX4 path has been directly verified with PX4 SITL and Gazebo X500:

- PX4-Autopilot SITL installed and built
- Gazebo X500 simulation launched successfully
- active PX4 MAVLink UDP instance inspected using `mavlink status`
- DRACO connected to the real PX4 SITL MAVLink path
- QGroundControl configured as the real Ground Control Station
- genuine QGroundControl MAVLink traffic received by DRACO
- QGroundControl packets forwarded unchanged to PX4 SITL
- PX4 telemetry forwarded through DRACO back to QGroundControl
- QGroundControl successfully discovered the simulated X500 through DRACO
- normal heartbeat and telemetry display verified
- ARM and DISARM commands successfully passed through DRACO
- PX4 `COMMAND_ACK` messages (`msgid 77`) returned through DRACO
- stopping DRACO caused QGroundControl to enter `Comms Lost`
- restarting DRACO restored communication and returned QGroundControl to `Ready`

During testing, the active PX4 MAVLink instance reported:

```text
mode: Normal
MAVLink version: 2
transport protocol: UDP (18570, remote port: 14550)
```

## Phase 2 — MAVLink Parsing and Semantic Classification

Phase 2 replaced manual header inspection with the generated MAVLink library parser and added a separate semantic classification layer.

### MAVLink Parser

The parser is implemented in:

```text
mavlink_parser.h
mavlink_parser.cpp
```

Implemented behavior includes:

- generated MAVLink headers from the PX4 build are used instead of hard-coded message numbers
- incoming bytes are fed through the standard MAVLink parser
- only frames reported as successfully framed are emitted as parsed messages
- parsing is performed for both GCS → PX4 and PX4 → GCS traffic
- each parsed frame carries an explicit direction tag
- parsed metadata includes MAVLink message ID, system ID, component ID, sequence number and payload length
- multiple complete MAVLink frames can be returned from an input buffer
- original received bytes remain separate from parsed representations and are forwarded without re-encoding

Directions are represented as:

```text
GCS_TO_PX4
PX4_TO_GCS
```

### Message Families

The semantic classifier groups recognized traffic into broad message families:

```text
READ_ONLY
COMMAND
PARAMETER
MISSION
POSITION_OR_SETPOINT
MODE_OR_CONTROL
ACK_OR_RESPONSE
STATE_EVIDENCE
OTHER
```

PX4-originated state-evidence classification currently includes messages such as heartbeat, system status, attitude, attitude quaternion, global position, local position and extended system state.

### Semantic Operations

Recognized traffic is further translated into security-relevant semantic operations:

```text
READ_ONLY
ARM
DISARM
TAKEOFF
LAND
RTL
MODE_CHANGE
DIRECT_CONTROL
PARAMETER_WRITE
MISSION_CHANGE
POSITION_TARGET
ACK_OR_RESPONSE
STATE_EVIDENCE
UNKNOWN_WRITE
OTHER
```

`COMMAND_LONG` and `COMMAND_INT` messages are decoded and their `MAV_CMD` values are mapped to semantic operations.

Examples currently handled include:

- `MAV_CMD_COMPONENT_ARM_DISARM` → `ARM` or `DISARM`
- `MAV_CMD_NAV_TAKEOFF` → `TAKEOFF`
- `MAV_CMD_NAV_LAND` → `LAND`
- `MAV_CMD_NAV_RETURN_TO_LAUNCH` → `RTL`
- `MAV_CMD_DO_SET_MODE` → `MODE_CHANGE`
- `MAV_CMD_DO_REPOSITION` → `POSITION_TARGET`
- `MAV_CMD_REQUEST_MESSAGE` → `READ_ONLY`

Known parameter writes, mission-changing messages, position targets, mode changes and direct-control messages are also mapped independently of `COMMAND_LONG`/`COMMAND_INT`.

Unrecognized commands inside command containers are conservatively classified as `UNKNOWN_WRITE`. After recognized harmless and read-only protocol traffic is handled, remaining unclassified GCS-side traffic is also currently labeled `UNKNOWN_WRITE` by the Phase 2 classifier. This is a classification result only; Phase 2 does not yet claim policy enforcement or command blocking.

### Verified Live Classification

ARM and DISARM were exercised through QGroundControl after the parser and classifier were integrated.

Observed output included:

```text
GCS MAVLink: msgid=76 ... family=COMMAND operation=ARM
Library MAVLink: msgid=77 ... family=ACK_OR_RESPONSE operation=ACK_OR_RESPONSE

GCS MAVLink: msgid=76 ... family=COMMAND operation=DISARM
Library MAVLink: msgid=77 ... family=ACK_OR_RESPONSE operation=ACK_OR_RESPONSE
```

This verifies that semantic observation did not break the command/response path.

Normal QGroundControl service traffic was also observed and classified without treating known harmless messages as writes. Examples include:

```text
HEARTBEAT     → OTHER
PING          → OTHER
REQUEST_EVENT → READ_ONLY
```

### Incomplete-Frame Test

A small parser test is kept in:

```text
tests/parser_test.cpp
```

The test supplies an intentionally incomplete MAVLink 2 frame to the parser. The verified result was:

```text
parsed messages: 0
```

This confirms that incomplete input is not promoted into a valid `ParsedMavlinkMessage`.

## Phase 3 — Trusted PX4 Evidence and Security Context

Phase 3 adds a dedicated PX4-derived evidence layer implemented in:

```text
state_cache.h
state_cache.cpp
```

The cache is updated from parsed PX4 → GCS traffic and stores both evidence values and security-relevant metadata.

### Cached Evidence

The current `StateCache` includes:

- armed/disarmed state derived from PX4 heartbeat evidence
- MAVLink base mode, PX4 custom mode and system status
- decoded PX4 control mode fields and an offboard-active indicator
- landed/airborne state from `EXTENDED_SYS_STATE`
- global latitude, longitude, altitude, relative altitude and NED velocity components from `GLOBAL_POSITION_INT`
- local NED position and velocity from `LOCAL_POSITION_NED`
- mission sequence, total mission items and mission state from `MISSION_CURRENT`
- a conservative failsafe indicator derived from PX4 heartbeat system status
- system-health bitmasks from `SYS_STATUS`
- estimator flags and consistency ratios from `ESTIMATOR_STATUS`

Global and local position are intentionally retained separately because they serve different evidence roles. Raw MAVLink units are preserved in the cache, including millimetres and centimetres per second for `GLOBAL_POSITION_INT` and metres/metres per second for `LOCAL_POSITION_NED`.

### Evidence Metadata

Each cached evidence field carries:

```text
value
source_sysid
source_compid
observed_at
age
valid
freshness
```

Security timekeeping uses `std::chrono::steady_clock`, providing a monotonic clock that is not affected by wall-clock changes.

Freshness states are represented explicitly as:

```text
FRESH
STALE
INVALID
UNKNOWN
```

A generic freshness helper applies the same metadata rule across all evidence-field value types. The current prototype freshness threshold is 3000 ms.

The gateway `poll()` timeout is 100 ms so freshness can continue to advance even when no network packet arrives. This allows evidence to transition from `FRESH` to `STALE` when the PX4 source becomes silent rather than leaving cached evidence fresh indefinitely.

A live stale-state test produced:

```text
local_freshness=0 age_ms=0
local_freshness=1 age_ms=3041
```

### Evidence Usability

A shared helper defines the current evidence-usability invariant:

```text
valid == true AND freshness == FRESH → usable
otherwise                             → not usable
```

Unit-test output verified:

```text
unknown usable: 0
fresh usable: 1
stale usable: 0
invalid usable: 0
```

This prevents unknown, stale or invalid evidence from being silently treated as trustworthy.

### Evidence Snapshot

Phase 3 also introduces `EvidenceSnapshot`.

A snapshot copies the current `StateCache`, records a monotonic capture time and refreshes freshness on the copied evidence. The intended use is to provide one frozen evidence view for a single security decision instead of reading a live cache that may change between individual field accesses.

A unit test verified that the live cache can change while the snapshot remains unchanged:

```text
live armed: 1
snapshot armed: 0
```

### Verified Live Evidence

Live PX4 SITL/QGroundControl testing directly verified:

- ARM → DISARM state changes
- PX4 base/custom/system-status updates
- landed-state transition through takeoff and landing
- global position, local position, altitude and velocity changes
- mission-state decoding with no mission stored
- mission-state decoding after uploading a five-item QGroundControl mission
- control/offboard-state decoding
- system-health updates
- estimator-status updates
- `FRESH` → `STALE` transition when PX4 telemetry stops
- snapshot immutability after the live cache changes

The final integration smoke test kept the transparent QGroundControl ↔ DRACO ↔ PX4 path operational through normal flight activity including arming, takeoff, movement, landing and disarming while the Phase 3 evidence layer remained active.

## Command Path Verification

The current regression path remains:

```text
QGroundControl
      ↓
DRACO UDP gateway
      ↓
MAVLink parser
      ↓
semantic classifier
      ↓
PX4-derived evidence observation
      ↓
original frame forwarded to PX4
      ↓
PX4 response / telemetry
      ↓
DRACO
      ↓
QGroundControl
```

Evidence observation does not replace or re-encode the forwarded MAVLink frame.

## Repository Layout

```text
.
├── main.cpp
├── udp_gateway.cpp
├── mavlink_parser.h
├── mavlink_parser.cpp
├── semantic_classifier.h
├── semantic_classifier.cpp
├── state_cache.h
├── state_cache.cpp
├── tests/
│   ├── parser_test.cpp
│   └── state_cache_test.cpp
├── .gitignore
├── LICENSE
├── README.md
└── docs/
    ├── README.md
    ├── system-architecture.md
    ├── protocol-design.md
    ├── threat-model.md
    ├── development-roadmap.md
    └── CHANGELOG.md
```

## Current Development State

DRACO now has a verified transparent real-GCS MAVLink path, direction-aware protocol parsing, semantic classification and a PX4-derived evidence layer with freshness and snapshot semantics.

The current public implementation can:

- receive and forward live MAVLink traffic bidirectionally
- parse valid MAVLink frames using generated MAVLink headers
- preserve the original received frame bytes for forwarding
- distinguish GCS → PX4 from PX4 → GCS traffic
- classify broad MAVLink message families
- decode `COMMAND_LONG` and `COMMAND_INT`
- map recognized commands and write operations into semantic operations
- identify selected PX4-originated state-evidence messages
- cache armed, mode, landed, position, velocity, mission, health and estimator evidence
- retain source system/component identifiers and monotonic observation timestamps
- track evidence age and `FRESH`/`STALE`/`INVALID`/`UNKNOWN` state
- reject unknown, stale or invalid evidence from the shared usability helper
- create immutable per-decision evidence snapshots
- conservatively label unclassified GCS-side traffic
- reject incomplete input from the parsed-message output
- preserve the live QGroundControl ↔ PX4 command and telemetry path while observation is active

Further research mechanisms are intentionally not described in the public repository until they have been implemented and experimentally verified.

## Documentation Policy

Public documentation is deliberately limited to completed implementation, verified observations, reproducible current-state behavior and experimentally demonstrated milestones. Unimplemented research mechanisms, future experimental phases and unpublished design details are intentionally kept outside the public repository.

## License

MIT License. See [LICENSE](LICENSE).
