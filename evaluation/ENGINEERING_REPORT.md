# DRACO final engineering and evaluation report

Status: implemented and locally evaluated; changes remain uncommitted for human review. Unavailable baselines and deployment limitations are explicitly listed below. This is not a research paper.

## A. Pre-change state

- Branch: `evaluation`.
- Commit: `46e423b5d5f1f161d93b3e757b35db89e98229f2`.
- Initial working tree was clean. All eight original test executables built and exited successfully; the unchanged gateway built.
- Initial output: `evaluation/results/raw/initial/results.txt`. No original test was weakened.
- Local execution: Ubuntu 24.04 WSL, g++ 13.3.0, Python 3.12.3, CMake Release build, OpenSSL 3.0.13.
- PX4 revision: `547e2caa9883bc1609325f02b4914b4e2482fe62`; Gazebo 8.15.0, model `x500_0`.

## B. Files created

- `CMakeLists.txt`
- `config/sitl_policy.conf`
- `evaluation/ENGINEERING_REPORT.md`
- `evaluation/check_runtime_invariant.py`
- `evaluation/core_runner.cpp`
- `evaluation/delta_accuracy.cpp`
- `evaluation/finalize_results.py`
- `evaluation/mission_client.cpp`
- `evaluation/observer.html`
- `evaluation/observer.py`
- `evaluation/results/report/data.json`
- `evaluation/results/report/index.html`
- `evaluation/run_baseline.py`
- `evaluation/run_live.py`
- `evaluation/run_suite.py`
- `evaluation/scenarios.cpp`
- `evaluation/scenarios.h`
- `evaluation/security_review/README.md`
- `evaluation/write_report.py`
- `evaluation_mode.cpp`
- `evaluation_mode.h`
- `gateway_ack_reconciliation.cpp`
- `gateway_ack_reconciliation.h`
- `gateway_options.h`
- `mission_pipeline.cpp`
- `mission_pipeline.h`
- `principal_context.cpp`
- `principal_context.h`
- `runtime_policy.cpp`
- `runtime_policy.h`
- `structured_events.cpp`
- `structured_events.h`
- `tests/evaluation_scenarios_test.cpp`
- `tests/gateway_ack_reconciliation_test.cpp`
- `tests/gateway_transport_test.cpp`
- `tests/mixed_frame_test.cpp`
- `tests/no_op_revalidation_test.cpp`
- `tests/observer_test.py`
- `tests/runtime_provisioning_test.cpp`

Ignored raw logs, binaries, per-scenario mission readbacks, and intermediate verification scripts are not source additions. The generated HTML/JSON report is included for review.

## C. Files modified

- `.gitignore`
- `README.md`
- `main.cpp`
- `mavlink_parser.cpp`
- `mavlink_parser.h`
- `mission_authorization.cpp`
- `mission_authorization.h`
- `mission_decision_record.h`
- `mission_reconstructor.cpp`
- `state_cache.cpp`
- `udp_gateway.cpp`

An ignored private reference PDF was also renamed to a generic filename; its contents and Git history were not changed.

## D. Placeholders removed

| Previous location / issue | Replacement |
|---|---|
| udp_gateway.cpp inline SITL contract | runtime_policy.cpp plus explicit config/sitl_policy.conf |
| udp_gateway.cpp fixed proposer string | principal_context.cpp; empty unauthenticated normal principal or explicit simulation principal |
| udp_gateway.cpp fixed proposal authority | resolved principal.authority |
| Implicit live budget constructor values | every budget field required in trusted runtime policy |
| Datagram-level mission suppression | per-frame mediation preserving unrelated original wire bytes |
| Repeated debug output / duplicated include | structured decision/transfer/status events and focused human decision messages |
| Temporary no-op diagnostic and unapplied patch | approved implementation and permanent regression tests |

No forced live denial-altitude manipulation remains. Test envelope changes are confined to explicit regression fixtures.

## E. Runtime policy

`key=value` text with comma-separated fields, semicolon-separated geometry, and `#` comments. Every key in the sample is required. Missing files, missing/unknown/duplicate fields, invalid IDs/versions, malformed geometry, non-finite values, invalid authority mappings, or invalid budgets fail startup before sockets authorize traffic. The runtime adapter supports explicit relative-home altitude. There is no policy derived from a proposal, permissive fallback, hot reload, or automatic vehicle intervention. The sample limits are experimental.

## F. Principal / authority

Normal mode has no installed authenticated binding provider. It resolves `authenticated=false`, `evaluation_mode=false`, and an empty principal ID; an otherwise allowable proposal becomes `DEFER`. Evaluation requires explicit mode, principal ID, and authority. Those records remain `authenticated=false` and `evaluation_mode=true`. SYSID/COMPID and loopback endpoint pinning route/bind transactions but do not establish cryptographic identity. No real secrets were added. Local secret patterns are ignored.

## G. Security bugs and engineering fixes

| Finding | Impact / severity assessment | Fix | Frozen semantics |
|---|---|---|---|
| No-op shortcut skipped current intent | Conditional medium: retained revision could bypass revised policy | Move no-op success after current intent checks | Narrow change explicitly approved |
| Mixed datagram dropped unrelated traffic | Availability/transport defect | Preserve and route individual MAVLink wire frames | Unchanged |
| Shared upload buffer could be overwritten while PX4 transfer was active | High: authorized transaction integrity risk | Separate immutable authorized buffer and per-client incoming buffers; concurrent classification | Unchanged intended transaction/causality rules |
| Unmediated alternate mission-write paths | High: mission-clear/partial/legacy bypass risk | Reject unsupported mission-write operations and nonstandard mission types | No new mission permissions |
| PX4 evidence accepted from arbitrary sender on its socket | High under direct endpoint injection | Pin configured loopback source and autopilot component; do not call it authentication | Unchanged trusted-evidence boundary |
| Undefined landed state / impossible coordinates marked valid | Medium: fresh but unusable evidence could satisfy gate | Mark those decoded values invalid; add no-op and admin non-bypass regressions | Implements existing unusable-evidence rule |
| Upload timeout leaves commit outcome uncertain | High if later traffic trusts stale local history | Latch subsequent otherwise-ALLOW proposals to DEFER | No automatic commit or rollback |
| PX4 ACCEPTED could be echoed after local commit failure | High consistency failure: PX4 may hold a mission absent from local revision history | Return MAV_MISSION_ERROR, clear the local proposal, latch uncertainty, and emit LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT | Successful ACK and revision semantics unchanged |

No fixes change semantic delta, rollback/stale-parent/conflict classification, budget meaning, authority tiers, or emergency scope. A policy change does not modify an existing PX4 mission. Full-DRACO commit still requires an accepted matching PX4 ACK after all items were sent.

## H. Unit test results

Final result: **15/15 tests passed** (14 C++ executables plus the observer Python test). Raw output: `evaluation/results/raw/final_run/unit_tests.txt`. All original eight remain included. New coverage includes approved no-op policy revalidation; strict configuration; principal/authority provenance; undefined, invalid, stale and absent evidence; explicit ablations; every frozen scenario; mixed wire frames; real gateway mediation with a synthetic peer; successful accepted-ACK commit; fail-closed handling when PX4 accepts but the local commit fails; and read-only/HTML-safe observer serialization. Original parser/state-cache programs mostly print observations; exit zero is not presented as new assertion coverage.

## I. Live PX4 SITL results

Full DRACO executed **14 transactions**: seven benign transactions (insertion/deletion are separate) and seven attacks. Correct full outcomes: **14/14**. PX4 mission invariants: **14/14**. The in-flight case used actual X500 takeoff and fresh armed/in-air telemetry, followed by landing/disarm. No force-arm or disabled PX4 preflight checks were used.

The six live variants together executed **84** scenario transactions. All successful ALLOW uploads and rejected proposals were checked by downloading the actual PX4 mission. The separate normal-runtime unauthenticated test returned DEFER and preserved the readback hash. A post-build valid no-op readback check also passed after the final evidence-validation changes.

The concurrent test delays the separately authorized rival transfer by an explicit evaluation-only setting, submits a second real proposal against the same committed parent, and checks unchanged PX4 state after that denial before the independent rival later commits. It does not invent a conflict flag.

## J. Benign results

| Scenario | Expected | Actual | PX4 changed? | Correct? |
|---|---|---|---|---|
| BENIGN_NO_OP | ALLOW | ALLOW | no | yes |
| BENIGN_SMALL_CORRECTION | ALLOW | ALLOW | yes | yes |
| BENIGN_INSERT_DELETE / insert | ALLOW | ALLOW | yes | yes |
| BENIGN_INSERT_DELETE / delete | ALLOW | ALLOW | yes | yes |
| BENIGN_ALTITUDE_CORRECTION | ALLOW | ALLOW | yes | yes |
| BENIGN_DETOUR | ALLOW | ALLOW | yes | yes |
| BENIGN_IN_FLIGHT_REPLAN | ALLOW | ALLOW | yes | yes |

## K. Attack results

| Scenario | Expected | Actual | Reason | PX4 hash before | PX4 hash after | Correct? |
|---|---|---|---|---|---|---|
| ATTACK_SEMANTIC_ROLLBACK | DENY | DENY | SEMANTIC_ROLLBACK_DETECTED | d9646035f171ac18661a9f2d2b00ee08fdf340e1ce5c6122f7343d736d969a88 | d9646035f171ac18661a9f2d2b00ee08fdf340e1ce5c6122f7343d736d969a88 | yes |
| ATTACK_STALE_PARENT | DENY | DENY | STALE_PARENT_REVISION | d9646035f171ac18661a9f2d2b00ee08fdf340e1ce5c6122f7343d736d969a88 | d9646035f171ac18661a9f2d2b00ee08fdf340e1ce5c6122f7343d736d969a88 | yes |
| ATTACK_CONCURRENT_CONFLICT | DENY | DENY | CONCURRENT_REVISION_CONFLICT | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | yes |
| ATTACK_DESTINATION_DIVERSION | REQUIRE_HIGHER_AUTHORITY | REQUIRE_HIGHER_AUTHORITY | DESTINATION_CHANGE_OUTSIDE_BUDGET | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | yes |
| ATTACK_COMMAND_SUBSTITUTION | DENY | DENY | COMMAND_NOT_ALLOWED | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | yes |
| ATTACK_MAJOR_REPLACEMENT | REQUIRE_HIGHER_AUTHORITY | REQUIRE_HIGHER_AUTHORITY | UNRELATED_REPLACEMENT_REQUIRES_ADMIN | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | yes |
| ATTACK_OUTSIDE_INTENT | DENY | DENY | MISSION_CORRIDOR_VIOLATION | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | e4fab81c3eba9b272a9c36763eef95a2ace09aea8ce598e28dffee0f31d118b6 | yes |

Fresh semantic rollback constructs a new MAVLink upload containing superseded content; it is not captured-packet replay.

## L. Baseline results

| Baseline | Executed cases | Malicious blocked | Malicious allowed | Legitimate allowed | Legitimate blocked | Status |
|---|---|---|---|---|---|---|
| BASELINE_A | 13 | 1 | 5 | 7 | 0 | Executed; concurrent direct-client coordination NOT_IMPLEMENTED |
| BASELINE_B | not measured | not measured | not measured | not measured | not measured | NOT_IMPLEMENTED: no verified signing/key provisioning baseline |
| BASELINE_C | not measured | not measured | not measured | not measured | not measured | NOT_IMPLEMENTED: signing/native geofence comparison unavailable |
| BASELINE_D | not measured | not measured | not measured | not measured | not measured | NOT_IMPLEMENTED: no faithful independent proxy available |
| BASELINE_E | 14 | 7 | 0 | 7 | 0 | Executed full DRACO |

Baseline A is the real PX4 mission interface without DRACO. It necessarily retains PX4 native mission-protocol checks; PX4 itself rejected the command-substitution fixture. Raw stale-parent provenance has no authenticated MAVLink field. No claim is made that signing detects fresh semantic rollback.

## M. Ablation results

| Variant | Executed | Malicious blocked | False negatives | Legitimate allowed | False positives | Readback invariants |
|---|---|---|---|---|---|---|
| FULL_DRACO | 14 | 7 | 0 | 7 | 0 | 14/14 |
| ABLATION_NO_DELTA | 14 | 5 | 2 | 7 | 0 | 14/14 |
| ABLATION_NO_INTENT | 14 | 5 | 2 | 7 | 0 | 14/14 |
| ABLATION_NO_CAUSALITY | 14 | 5 | 2 | 7 | 0 | 14/14 |
| ABLATION_NO_FRESH_EVIDENCE | 14 | 7 | 0 | 7 | 0 | 14/14 |
| ABLATION_NO_CHANGE_BUDGET | 14 | 7 | 0 | 7 | 0 | 14/14 |

The no-causality concurrent case is still DEFERred by the immutable active-transfer guard; that is not semantic conflict detection. The frozen suite supplies fresh evidence, so the freshness ablation has identical blocking counts here; the dedicated missing/stale/invalid evidence regressions test the boundary. Budget ablation likewise need not create a false negative when independent intent/causality gates still block the chosen attack.

## N. Latency, scaling, and accuracy

Microseconds; linear interpolation at `(n-1)*p`. These are recorded deterministic local samples, not confidence intervals.

| Full-DRACO live stage | n | p50 | p95 | p99 |
|---|---|---|---|---|
| Canonicalization | 14 | 0.61 | 0.74 | 0.78 |
| Mission hash | 14 | 45.56 | 59.70 | 60.53 |
| Semantic delta | 14 | 5.70 | 15.74 | 21.00 |
| Authorization/policy | 14 | 2.61 | 3.39 | 3.59 |
| Total decision | 14 | 56.68 | 70.70 | 71.14 |
| Client transaction completion | 14 | 24319.40 | 35883.78 | 36949.77 |

| Mission items | Samples | p50 decision | p95 | p99 | Delta checks | Alignment checks |
|---|---|---|---|---|---|---|
| 10 | 30 | 27.05 | 49.82 | 59.37 | 30/30 | 30/30 |
| 50 | 30 | 108.35 | 136.15 | 184.10 | 30/30 | 30/30 |
| 100 | 30 | 229.01 | 311.70 | 340.44 | 30/30 | 30/30 |
| 500 | 30 | 2679.11 | 3426.55 | 3865.93 | 30/30 | 30/30 |
| 1000 | 30 | 8658.87 | 10217.43 | 11627.96 | 30/30 | 30/30 |

The separate ten-case labeled delta corpus produced 13 true-positive labels, 0 false-positive labels, 0 false-negative labels; precision 1.000, recall 1.000, F1 1.000. Annotated alignment checks: 5/5. These scores apply only to the explicit regression corpus. The scale benchmark changes one interior item; it does not claim worst-case replacement complexity or live transport performance for 1000 items.

## O. Dashboard

Dependency-light Python loopback HTTP observer with static HTML/JavaScript. It is GET-only, reads recorded files, and has no policy-editing, escalation, mission-send, or authorization actions. Variant selection only filters display. It shows connections/evidence, current mission, proposal/principal, semantic delta, causality, policy, decisions, PX4 transfer/ACK/commit flow, scenario outcomes, full readback hashes, counters, latency, and scaling charts.

```bash
python3 evaluation/observer.py --events evaluation/results/raw/final_run --port 8765
```

Open `http://127.0.0.1:8765`; standalone output is `evaluation/results/report/index.html`. Browser rendering and switching to the causality ablation were inspected; no browser errors were observed. No screenshot file was saved. Connection/evidence fields are the last recorded observations, not a claim of continuing connectivity.

## P. Known limitations

- No production authenticated GCS binding, signing baseline, durable revision journal, or automatic PX4 reconciliation on restart.
- Expected-parent provenance is an explicit local evaluation sidecar; standard mission upload does not carry an authenticated parent. Normal uploads snapshot the current parent at start.
- Geometry retains frozen point-based semantics; runtime altitude support is relative-home only. No flight-safety certification or policy-transition intervention.
- Complete integer mission uploads only; maximum 1000 items; unsupported mission write forms fail closed. Frames split across UDP datagrams are dropped.
- Other vehicle command families are outside this mission-revision gate. Trusted-host/PX4-side access is outside the authenticated GCS simulation.
- Three-second evidence freshness remains frozen. Undefined landed states and impossible global coordinates now set invalid evidence.
- Local scenario counts are small; scaling uses one changed waypoint and synthetic evidence. No general false-positive/negative population estimates are claimed.

## Q. Unimplemented items

- BASELINE_B, BASELINE_C, and BASELINE_D: explicitly NOT_IMPLEMENTED for the reasons above.
- BASELINE_A concurrent-client scheduling: NOT_IMPLEMENTED; no substituted result.
- Real authenticated principal binding and signing secrets: not available in this repository; normal mode remains closed.
- No live stale-evidence attack extension was added to the frozen scenario population; freshness boundaries are covered by explicit regression tests.
- No optional dashboard run controls, policy hot reload, durable-history recovery, or automatic aircraft intervention.

## R. Git review

`git status --short`:

```text
M .gitignore
 M README.md
 M main.cpp
 M mavlink_parser.cpp
 M mavlink_parser.h
 M mission_authorization.cpp
 M mission_authorization.h
 M mission_decision_record.h
 M mission_reconstructor.cpp
 M state_cache.cpp
 M udp_gateway.cpp
?? CMakeLists.txt
?? config/
?? evaluation/
?? evaluation_mode.cpp
?? evaluation_mode.h
?? gateway_ack_reconciliation.cpp
?? gateway_ack_reconciliation.h
?? gateway_options.h
?? mission_pipeline.cpp
?? mission_pipeline.h
?? principal_context.cpp
?? principal_context.h
?? runtime_policy.cpp
?? runtime_policy.h
?? structured_events.cpp
?? structured_events.h
?? tests/evaluation_scenarios_test.cpp
?? tests/gateway_ack_reconciliation_test.cpp
?? tests/gateway_transport_test.cpp
?? tests/mixed_frame_test.cpp
?? tests/no_op_revalidation_test.cpp
?? tests/observer_test.py
?? tests/runtime_provisioning_test.cpp
```


`git diff --stat` (tracked modifications only; newly created files above are untracked):

```text
.gitignore                |    8 +-
 README.md                 |  708 +++++++------------------
 main.cpp                  |   64 ++-
 mavlink_parser.cpp        |   30 +-
 mavlink_parser.h          |    3 +-
 mission_authorization.cpp |   42 +-
 mission_authorization.h   |    6 +-
 mission_decision_record.h |   11 +
 mission_reconstructor.cpp |    4 +-
 state_cache.cpp           |   14 +-
 udp_gateway.cpp           | 1284 +++++++++++++--------------------------------
 11 files changed, 668 insertions(+), 1506 deletions(-)
```


Significant diffs: the gateway transport is reorganized around separate immutable authorized and incoming transactions; trusted runtime provisioning and the shared measured decision pipeline replace live placeholders; the approved no-op success return moves after current intent validation; explicit evaluation-only ablations are added; invalid telemetry is marked unusable; accepted PX4 ACKs fail closed if local revision commit fails; and new runners, tests, structured output, and observer/report artifacts provide reproducible evaluation.

The current-source branding/placeholder search found no prohibited occurrences. The active branch remains `evaluation`; `main` was not changed. No commit, push, force-push, or history rewrite was performed. Awaiting human review.
