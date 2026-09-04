# DRACO — UAV mission-revision security gateway

DRACO mediates mission proposals between a GCS and a project-owned PX4 simulator.
It reconstructs and canonicalizes each proposal, evaluates semantic delta,
revision causality, independent mission intent, change budget, authority, and
fresh PX4 evidence. Only `ALLOW` starts a separate DRACO→PX4 mission transaction.
Revision history commits only after a matching accepted PX4 `MISSION_ACK`.

This repository contains a research prototype and a local evaluation harness.
It does not provide production GCS authentication or certify flight safety.

## Verified engineering state

- Explicit, strict runtime policy loading from `config/sitl_policy.conf`.
- Principal resolution with fail-closed normal runtime and deliberately selected
  simulation principals.
- Individual MAVLink frame mediation, immutable authorized upload buffers, and
  accepted-ACK commit handling.
- Automated frozen benign/adversarial scenarios, five evaluation-only ablations,
  a plain-MAVLink baseline, actual PX4 mission readbacks, JSONL/CSV measurements,
  and a read-only browser observer.
- All eight original test executables passed before changes. The expanded C++
  regression suite also passes; see the engineering report for the final count.
- Full DRACO was exercised against local PX4 SITL / Gazebo X500, including actual
  takeoff, in-flight replanning, and landing. See the recorded results rather
  than treating these small experiments as a general security accuracy claim.

[Engineering report](evaluation/ENGINEERING_REPORT.md) ·
[Standalone HTML results](evaluation/results/report/index.html) ·
[Structured report data](evaluation/results/report/data.json)

## Preserved security rules

`ROLLBACK`, `STALE_PARENT`, and `CONCURRENT_CONFLICT` remain hard denials.
`SECURITY_ADMIN` cannot override those failures or hard intent violations.
Emergency authority remains scoped to the configured emergency policy.
Required evidence must be valid and fresh for full-DRACO authorization.

An exact re-upload retains the `NO_OP_REUPLOAD` causality classification and
consumes no change budget, but must satisfy the **current** contract. This narrow
current-policy revalidation change was explicitly approved. It applies only to
new proposals: a policy change does not clear, alter, or intervene in an already
committed PX4 mission. No hot policy reload or vehicle intervention is provided.

## Build and test

The gateway and mission client use POSIX UDP sockets. Run them in Linux or WSL;
the observer and static report also work on Windows. Requirements are C++17,
CMake, OpenSSL development files, Python 3, and PX4's generated MAVLink headers.
No JSON/YAML framework or Python package installation is required.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMAVLINK_INCLUDE_DIR="$HOME/PX4-Autopilot/build/px4_sitl_default/mavlink"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

Tests keep assertions enabled in Release builds. `gateway_transport_test` uses
an explicitly synthetic local peer on ports 14850, 14860, 14862, and 18871. These
tests are not presented as live PX4 measurements.

## Trusted runtime policy

```bash
./build/draco --policy config/sitl_policy.conf
```

The dependency-free format is `key=value`, with `#` comments, comma-separated
lists/points, and semicolon-separated corridor points or excluded regions.
`none` explicitly represents an empty excluded-region or optional authority/
emergency-command list. Every documented key in the sample file is required.
Unknown/duplicate/missing keys, invalid versions/IDs, non-finite numbers,
malformed geometry, invalid authorities, or invalid budgets stop startup with
an error. There is no generated policy or permissive fallback.

The policy file is trusted host-administered configuration, not input supplied
by a mission proposer. Protect its filesystem ownership and permissions.
The sample's limits are experiment-specific values, not universal safety limits.
The runtime adapter supports `RELATIVE_HOME` altitude only; it defers unsupported
positional frames instead of silently converting altitude references. Region
membership and corridor checks retain the frozen horizontal point semantics;
the separate altitude envelope supplies vertical restrictions.

## Principals and evaluation mode

No authenticated binding provider is installed. Normal mode resolves an empty,
unauthenticated principal; an otherwise allowable mission proposal becomes
`DEFER / PRINCIPAL_NOT_AUTHENTICATED`. It never authorizes by SYSID, COMPID, IP,
port, hostname, or a GCS display name.

For controlled simulation only:

```bash
./build/draco --policy config/sitl_policy.conf \
  --evaluation --principal sitl-normal-operator --authority NORMAL_OPERATOR \
  --results evaluation/results/raw/manual
```

Every decision records the principal ID, authority, `authenticated=false`, and
`evaluation_mode=true`. An explicit simulation principal represents the threat
model; it is not cryptographic authentication. Other available tiers are
`EMERGENCY_AUTHORITY` and `SECURITY_ADMIN`.

`FULL_DRACO` is the default. `--mode ABLATION_NO_DELTA`, `ABLATION_NO_INTENT`,
`ABLATION_NO_CAUSALITY`, `ABLATION_NO_FRESH_EVIDENCE`, and
`ABLATION_NO_CHANGE_BUDGET` require `--evaluation`. They do not delete modules.
The no-fresh-evidence ablation deliberately removes that gate only in simulation.

## Local PX4 SITL

In a separate WSL terminal:

```bash
cd "$HOME/PX4-Autopilot"
HEADLESS=1 make px4_sitl gz_x500
```

DRACO uses loopback endpoints only:

```text
mission client → DRACO :14560 → DRACO :14550 ↔ PX4 :18570
```

The three ports can be explicitly changed with `--gcs-port`, `--px4-local-port`,
and `--px4-remote-port`. Incoming PX4 evidence is limited to the configured
loopback endpoint and autopilot component. Endpoint pinning is transport
provenance, not authentication. Do not connect this harness to real aircraft.

## Run the automated evaluation

Start the local X500 as above, stop any separately running DRACO gateway, then:

```bash
python3 evaluation/run_suite.py --output evaluation/results/raw/my_run
python3 evaluation/observer.py --events evaluation/results/raw/my_run \
  --report evaluation/results/report
```

Choose a new output directory for each run. The suite manages its own gateway
processes and runs shared simulator ports sequentially. In-flight scenarios
issue ordinary simulator takeoff/arm commands and land afterward. No force-arm
or disabled PX4 preflight checks are used. The direct baseline requires port
14550 to be free. Failed commands/readbacks are recorded as errors.

For core-only measurements without PX4:

```bash
./build/core_runner evaluation/results/raw/core_run
./build/delta_accuracy evaluation/results/raw/delta_run
```

Core measurements explicitly label synthetic evidence. Scaling uses 30 samples
at each of 10, 50, 100, 500, and 1000 items, with one interior waypoint move.
The separate labeled delta corpus covers ten change cases; its precision,
recall, F1, and alignment figures are limited to that corpus.

The mission client supports deterministic integer-item upload, download, and
hashing, for example:

```bash
./build/mission_client upload evaluation/results/raw/core_run/base.mission 14560 14600
./build/mission_client download evaluation/results/raw/readback.mission 14560 14600
```

The frozen scenario identifiers are defined in `evaluation/scenarios.cpp`.
Insertion and deletion are separate transactions under `BENIGN_INSERT_DELETE`.
Rollback submits old content in a newly constructed MAVLink transaction.

MAVLink mission upload does not carry an authenticated expected-parent hash.
Normal gateway operation snapshots the parent when upload starts. For explicit
stale-parent simulation, `--evaluation-context DIR` reads scenario/parent fields
from a local file named for the client's UDP port. This is an evaluation fixture,
not a new authenticated wire protocol. The concurrent case uses two real
proposals against one parent and an explicit evaluation-only upload delay;
`--evaluation-upload-delay-ms` is rejected outside evaluation mode.

## Observer and result interpretation

```bash
python3 evaluation/observer.py --events evaluation/results/raw/my_run --port 8765
```

Open `http://127.0.0.1:8765`. The observer shows recorded endpoints, evidence,
mission/proposal hashes, principal and policy provenance, delta, causality,
decisions, transfer/ACK events, counters, scenario tables, and latency/scaling.
Its only selector filters displayed result variants. It cannot change policy,
escalate authority, authorize traffic, or send mission commands.

The static `evaluation/results/report/index.html` embeds its data and opens
without a server. JSONL gateway events, per-run CSV, consolidated `decisions.csv`,
and before/after mission files remain under the ignored raw directory. Report
numbers come from these executed records. Non-ALLOW invariants compare actual
PX4 readback hashes; accepted ALLOW proposals compare readback to the proposed
canonical hash. GCS rejection ACKs are distinguished from PX4 ACKs.

## Limitations

- Results are a small deterministic local experiment, not a deployment claim.
- Baselines B/C/D are `NOT_IMPLEMENTED`: verified signing/key provisioning and a
  faithful independent stateful proxy are unavailable. No signing measurements
  or credentials are fabricated. Direct baseline A's concurrent case is also
  marked unimplemented; its other cases are executed against real PX4.
- The frozen suite uses fresh evidence, so its no-fresh-evidence ablation alone
  does not quantify stale-evidence attacks. Separate regressions verify missing,
  stale, undefined landed-state, and invalid-coordinate evidence fails closed.
- Revision history is in memory, is not recovered from PX4 on restart, and is
  not durable. A PX4 upload timeout latches authorization closed for uncertain
  commit state. An accepted PX4 ACK followed by an unexpected local revision
  commit failure also returns `MAV_MISSION_ERROR` to the GCS and latches the
  same uncertainty gate. Restarting is not a substitute for independent
  reconciliation.
- Only complete standard mission uploads of at most 1000 items are supported.
  Legacy/partial/clear/set-current mission-write paths and other mission types
  are rejected. Complete unrelated MAVLink frames preserve their received bytes;
  malformed frames and frames split across UDP datagrams are not forwarded.
- The gateway does not mediate all vehicle commands or protect against an
  attacker with direct access to the trusted PX4-side socket or host policy files.
- Freshness remains the frozen three-second threshold. PX4 feasibility and
  geofence behavior are independent of the DRACO contract.
- Local secret files belong in ignored `config/local/` or `config/*.secret`.
  This implementation neither requires nor installs real signing secrets.

Work remains uncommitted on `evaluation` for human review. `main` is unchanged.

## License

MIT. See [LICENSE](LICENSE).
