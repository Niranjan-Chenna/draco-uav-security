# DRACO Threat Model

**Status: Initial threat model — planned mechanisms are not yet implemented or validated**

## Scope

DRACO is studying command authentication and authorization at the application/security layer between a Ground Control Station and a simulated UAV command interface. The initial model focuses on hostile manipulation of command traffic and incorrect command acceptance.

This phase does not attempt to comprehensively model physical capture, radio-frequency interference or jamming, traffic analysis, hardware faults, sensor spoofing, supply-chain compromise, firmware exploitation, ground-station malware, denial of service, or the safety and control dynamics of a real aircraft. These exclusions do not imply that the threats are unimportant; they constrain the first research questions to a tractable scope.

## Assets and security properties

The planned system aims to protect:

- authenticity of command origin within an authorized session;
- integrity of every security-relevant command-envelope field;
- freshness and one-time acceptance of eligible commands;
- authorization of commands against the current UAV state;
- separation between untrusted channel input and the firmware interface; and
- useful, non-secret security decision records for later analysis.

Availability and confidentiality may be considered in later phases, but they are not the primary guarantees of the initial command-authentication design.

## Threats and planned security mechanisms

| Threat | Intended attacker action | Planned security mechanism | Intended mitigation |
| --- | --- | --- | --- |
| Command spoofing | Construct a packet that appears to originate from the authorized GCS. | HMAC authentication with session-associated secret material. | Reject packets whose authentication tag cannot be verified. |
| Replay of a valid command | Capture a legitimate packet and retransmit it later. | Nonce tracking plus timestamp freshness validation. | Reject previously used nonces and packets outside the permitted freshness window. |
| Packet modification | Alter a command or security field while a packet is in transit. | HMAC integrity verification over a canonical envelope. | Detect any authenticated-field modification before forwarding the command. |
| Unauthorized session | Send a command without an active, recognized session. | Session validation and session-to-key binding. | Reject packets with missing, expired, unknown, or improperly bound sessions. |
| Stale command | Delay or retain a packet until its context may no longer be valid. | Timestamp validation. | Reject packets outside the planned acceptance window. |
| Invalid command transition | Request an operation that is not allowed in the current UAV state. | State-aware command authorization. | Reject commands absent from the permitted transition set for the current state. |
| Malformed packet | Supply incomplete, ambiguous, oversized, or incorrectly encoded input. | Strict parsing and schema validation. | Reject the packet before security-sensitive processing or state changes. |

All entries describe intended mitigations. No security mechanism in this table has yet been implemented, tested, or shown to prevent the corresponding threat.

## Trust assumptions

The initial design assumes:

- the GCS and UAV gateway begin with or can establish appropriate shared secret material through a future trusted provisioning or session-establishment process;
- the trusted endpoints, their cryptographic libraries, and their random-number sources behave correctly for the purpose of the first simulation;
- the ground environment protects active session secrets from the modeled network adversary;
- the UAV security gateway is the enforced entry point for external commands;
- the current UAV state is available accurately to the state-validation component;
- clocks are sufficiently synchronized for a future, explicitly defined freshness policy; and
- the communication channel provides no inherent guarantee of origin, integrity, freshness, ordering, or delivery.

Key compromise, malicious trusted insiders, endpoint takeover, and rollback of persisted security state are outside the first model and should be considered in later threat-model revisions.

## Security boundaries

```text
Trusted Ground Environment
        |
        | Trust Boundary: authenticated envelope enters an untrusted medium
        v
Potentially Hostile Communication Channel
        |
        | Trust Boundary: all received packet data requires validation
        v
Protected UAV Command Interface
        |
        | Only accepted commands may cross
        v
UAV Firmware / Flight-Control Simulation
```

The channel is deliberately treated as hostile for future attack simulation. An adversary may be able to observe, retain, reorder, duplicate, modify, and inject packets. The architecture should not interpret successful delivery as evidence of authenticity.

## Adversary model

The future adversary simulator may attempt to:

- capture a legitimate command packet;
- replay an old packet;
- modify the command or another envelope field;
- construct a forged packet without valid authentication material;
- send commands without a valid session; and
- send commands inconsistent with the UAV's current state.

The initial adversary is modeled as having control over the communication channel but not access to uncompromised secret keys or trusted endpoint memory. Experiments should clearly state when they relax any of these assumptions. This repository does not contain attack code.

## Proposed security events

Future experiments are expected to record the validation stage and decision using stable reason identifiers such as:

| Proposed reason | Meaning |
| --- | --- |
| `INVALID_SESSION` | The packet does not reference an acceptable active session. |
| `STALE_TIMESTAMP` | The packet timestamp is outside the permitted freshness window. |
| `REPLAY_DETECTED` | The nonce violates the session replay policy. |
| `INVALID_HMAC` | Authentication-tag verification fails. |
| `INVALID_STATE_TRANSITION` | The command is not allowed in the current UAV state. |
| `MALFORMED_PACKET` | The packet cannot be parsed under the expected schema. |

Reason granularity will be reviewed to ensure logs remain useful without exposing an attacker-facing validation oracle.

## Residual risks and limitations

Even if the planned mechanisms are implemented correctly, the initial design would not by itself address:

- compromise or theft of shared keys;
- a malicious or compromised authorized GCS;
- attacks that prevent packet delivery;
- confidentiality of command contents;
- unsafe but state-valid authorized commands;
- vulnerabilities inside the firmware or cryptographic implementation; or
- physical, RF, sensor, and hardware attacks outside the modeled interface.

These limitations will inform later protocol refinement and evaluation criteria.
