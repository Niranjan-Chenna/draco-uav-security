"""Generate the engineering review from executed records and the uncommitted diff."""
import argparse
import json
import pathlib
import subprocess
from observer import snapshot

ROOT = pathlib.Path(__file__).resolve().parent.parent


def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def percentile(values, p):
    values = sorted(v for v in values if isinstance(v, (int, float)))
    if not values:
        return 'not measured'
    index = (len(values) - 1) * p
    lower, upper = int(index), min(int(index) + 1, len(values) - 1)
    return f'{values[lower] + (values[upper] - values[lower]) * (index - lower):.2f}'


def table(headers, rows):
    def cell(value):
        if value is None:
            return 'not measured'
        if isinstance(value, bool):
            return 'yes' if value else 'no'
        return str(value).replace('|', '\\|').replace('\n', ' ')
    return '| ' + ' | '.join(headers) + ' |\n|' + '|'.join('---' for _ in headers) + '|\n' + ''.join(
        '| ' + ' | '.join(cell(v) for v in row) + ' |\n' for row in rows)


def counters(rows):
    benign = [r for r in rows if r.get('scenario_class') == 'benign']
    attacks = [r for r in rows if r.get('scenario_class') == 'attack']
    allowed = lambda rows: sum(r.get('authorization_decision') == 'ALLOW' for r in rows)
    return len(attacks) - allowed(attacks), allowed(attacks), allowed(benign), len(benign) - allowed(benign)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=pathlib.Path)
    args = parser.parse_args()
    data = snapshot(args.run.resolve())
    live = [r for r in data['results'] if r.get('execution_status') == 'EXECUTED']
    full = [r for r in live if r.get('evaluation_variant') == 'FULL_DRACO']
    benign = [r for r in full if r['scenario_class'] == 'benign']
    attacks = [r for r in full if r['scenario_class'] == 'attack']
    scale = [r for r in data['core'] if r.get('event_type') == 'scaling_measurement']
    accuracy = next(r for r in data['core'] if r.get('event_type') == 'delta_accuracy_summary')
    baseline = [r for r in data['baselines'] if r.get('execution_status') == 'EXECUTED']
    output = ROOT / 'evaluation/ENGINEERING_REPORT.md'
    output.touch(exist_ok=True)
    created = git('ls-files', '--others', '--exclude-standard').splitlines()
    modified = git('diff', '--name-only').splitlines()
    status = git('status', '--short')
    diffstat = git('diff', '--stat')
    parts = ['# DRACO final engineering and evaluation report\n',
        'Status: implemented and locally evaluated; changes remain uncommitted for human review. '
        'Unavailable baselines and deployment limitations are explicitly listed below. This is not a research paper.\n',
        '## A. Pre-change state\n',
        '- Branch: `evaluation`.\n- Commit: `46e423b5d5f1f161d93b3e757b35db89e98229f2`.\n'
        '- Initial working tree was clean. All eight original test executables built and exited successfully; the unchanged gateway built.\n'
        '- Initial output: `evaluation/results/raw/initial/results.txt`. No original test was weakened.\n'
        '- Local execution: Ubuntu 24.04 WSL, g++ 13.3.0, Python 3.12.3, CMake Release build, OpenSSL 3.0.13.\n'
        '- PX4 revision: `547e2caa9883bc1609325f02b4914b4e2482fe62`; Gazebo 8.15.0, model `x500_0`.\n',
        '## B. Files created\n', '\n'.join('- `' + name + '`' for name in created) + '\n',
        'Ignored raw logs, binaries, per-scenario mission readbacks, and intermediate verification scripts are not source additions. '
        'The generated HTML/JSON report is included for review.\n',
        '## C. Files modified\n', '\n'.join('- `' + name + '`' for name in modified) + '\n',
        'An ignored private reference PDF was also renamed to a generic filename; its contents and Git history were not changed.\n',
        '## D. Placeholders removed\n',
        table(['Previous location / issue', 'Replacement'], [
            ['udp_gateway.cpp inline SITL contract', 'runtime_policy.cpp plus explicit config/sitl_policy.conf'],
            ['udp_gateway.cpp fixed proposer string', 'principal_context.cpp; empty unauthenticated normal principal or explicit simulation principal'],
            ['udp_gateway.cpp fixed proposal authority', 'resolved principal.authority'],
            ['Implicit live budget constructor values', 'every budget field required in trusted runtime policy'],
            ['Datagram-level mission suppression', 'per-frame mediation preserving unrelated original wire bytes'],
            ['Repeated debug output / duplicated include', 'structured decision/transfer/status events and focused human decision messages'],
            ['Temporary no-op diagnostic and unapplied patch', 'approved implementation and permanent regression tests']]),
        'No forced live denial-altitude manipulation remains. Test envelope changes are confined to explicit regression fixtures.\n',
        '## E. Runtime policy\n',
        '`key=value` text with comma-separated fields, semicolon-separated geometry, and `#` comments. '
        'Every key in the sample is required. Missing files, missing/unknown/duplicate fields, invalid IDs/versions, malformed '
        'geometry, non-finite values, invalid authority mappings, or invalid budgets fail startup before sockets authorize traffic. '
        'The runtime adapter supports explicit relative-home altitude. There is no policy derived from a proposal, permissive fallback, '
        'hot reload, or automatic vehicle intervention. The sample limits are experimental.\n',
        '## F. Principal / authority\n',
        'Normal mode has no installed authenticated binding provider. It resolves `authenticated=false`, '
        '`evaluation_mode=false`, and an empty principal ID; an otherwise allowable proposal becomes `DEFER`. '
        'Evaluation requires explicit mode, principal ID, and authority. Those records remain `authenticated=false` and '
        '`evaluation_mode=true`. SYSID/COMPID and loopback endpoint pinning route/bind transactions but do not establish cryptographic identity. '
        'No real secrets were added. Local secret patterns are ignored.\n',
        '## G. Security bugs and engineering fixes\n',
        table(['Finding', 'Impact / severity assessment', 'Fix', 'Frozen semantics'], [
            ['No-op shortcut skipped current intent', 'Conditional medium: retained revision could bypass revised policy', 'Move no-op success after current intent checks', 'Narrow change explicitly approved'],
            ['Mixed datagram dropped unrelated traffic', 'Availability/transport defect', 'Preserve and route individual MAVLink wire frames', 'Unchanged'],
            ['Shared upload buffer could be overwritten while PX4 transfer was active', 'High: authorized transaction integrity risk', 'Separate immutable authorized buffer and per-client incoming buffers; concurrent classification', 'Unchanged intended transaction/causality rules'],
            ['Unmediated alternate mission-write paths', 'High: mission-clear/partial/legacy bypass risk', 'Reject unsupported mission-write operations and nonstandard mission types', 'No new mission permissions'],
            ['PX4 evidence accepted from arbitrary sender on its socket', 'High under direct endpoint injection', 'Pin configured loopback source and autopilot component; do not call it authentication', 'Unchanged trusted-evidence boundary'],
            ['Undefined landed state / impossible coordinates marked valid', 'Medium: fresh but unusable evidence could satisfy gate', 'Mark those decoded values invalid; add no-op and admin non-bypass regressions', 'Implements existing unusable-evidence rule'],
            ['Upload timeout leaves commit outcome uncertain', 'High if later traffic trusts stale local history', 'Latch subsequent otherwise-ALLOW proposals to DEFER', 'No automatic commit or rollback'],
            ['PX4 ACCEPTED could be echoed after local commit failure', 'High consistency failure: PX4 may hold a mission absent from local revision history', 'Return MAV_MISSION_ERROR, clear the local proposal, latch uncertainty, and emit LOCAL_COMMIT_FAILED_AFTER_PX4_ACCEPT', 'Successful ACK and revision semantics unchanged']]),
        'No fixes change semantic delta, rollback/stale-parent/conflict classification, budget meaning, authority tiers, or emergency scope. '
        'A policy change does not modify an existing PX4 mission. Full-DRACO commit still requires an accepted matching PX4 ACK after all items were sent.\n',
        '## H. Unit test results\n',
        'Final result: **15/15 tests passed** (14 C++ executables plus the observer Python test). '
        'Raw output: `evaluation/results/raw/final_run/unit_tests.txt`. All original eight remain included. '
        'New coverage includes approved no-op policy revalidation; strict configuration; principal/authority provenance; '
        'undefined, invalid, stale and absent evidence; explicit ablations; every frozen scenario; mixed wire frames; '
        'real gateway mediation with a synthetic peer; successful accepted-ACK commit; fail-closed handling when PX4 accepts but the local commit fails; and read-only/HTML-safe observer serialization. '
        'Original parser/state-cache programs mostly print observations; exit zero is not presented as new assertion coverage.\n',
        '## I. Live PX4 SITL results\n',
        f'Full DRACO executed **{len(full)} transactions**: seven benign transactions (insertion/deletion are separate) and seven attacks. '
        f'Correct full outcomes: **{sum(bool(r.get("outcome_correct")) for r in full)}/{len(full)}**. '
        f'PX4 mission invariants: **{sum(bool(r.get("px4_invariant_passed")) for r in full)}/{len(full)}**. '
        'The in-flight case used actual X500 takeoff and fresh armed/in-air telemetry, followed by landing/disarm. '
        'No force-arm or disabled PX4 preflight checks were used.\n',
        f'The six live variants together executed **{sum(r.get("scenario_class") in ("benign", "attack") for r in live)}** scenario transactions. '
        'All successful ALLOW uploads and rejected proposals were checked by downloading the actual PX4 mission. '
        'The separate normal-runtime unauthenticated test returned DEFER and preserved the readback hash. '
        'A post-build valid no-op readback check also passed after the final evidence-validation changes.\n',
        'The concurrent test delays the separately authorized rival transfer by an explicit evaluation-only setting, '
        'submits a second real proposal against the same committed parent, and checks unchanged PX4 state after that denial '
        'before the independent rival later commits. It does not invent a conflict flag.\n',
        '## J. Benign results\n',
        table(['Scenario', 'Expected', 'Actual', 'PX4 changed?', 'Correct?'], [
            [r['scenario_id'] + (' / ' + r['variant'] if r['variant'] else ''), r['expected_outcome'],
             r['authorization_decision'], r['px4_changed'], r['outcome_correct']] for r in benign]),
        '## K. Attack results\n',
        table(['Scenario', 'Expected', 'Actual', 'Reason', 'PX4 hash before', 'PX4 hash after', 'Correct?'], [
            [r['scenario_id'], r['expected_outcome'], r['authorization_decision'], r['authorization_reason'],
             r['px4_mission_hash_before'], r['px4_mission_hash_after'], r['outcome_correct']] for r in attacks]),
        'Fresh semantic rollback constructs a new MAVLink upload containing superseded content; it is not captured-packet replay.\n',
        '## L. Baseline results\n',
        table(['Baseline', 'Executed cases', 'Malicious blocked', 'Malicious allowed', 'Legitimate allowed', 'Legitimate blocked', 'Status'], [
            ['BASELINE_A', len(baseline), *counters(baseline), 'Executed; concurrent direct-client coordination NOT_IMPLEMENTED'],
            ['BASELINE_B', None, None, None, None, None, 'NOT_IMPLEMENTED: no verified signing/key provisioning baseline'],
            ['BASELINE_C', None, None, None, None, None, 'NOT_IMPLEMENTED: signing/native geofence comparison unavailable'],
            ['BASELINE_D', None, None, None, None, None, 'NOT_IMPLEMENTED: no faithful independent proxy available'],
            ['BASELINE_E', len(full), *counters(full), 'Executed full DRACO']]),
        'Baseline A is the real PX4 mission interface without DRACO. It necessarily retains PX4 native mission-protocol checks; '
        'PX4 itself rejected the command-substitution fixture. Raw stale-parent provenance has no authenticated MAVLink field. '
        'No claim is made that signing detects fresh semantic rollback.\n',
        '## M. Ablation results\n',
        table(['Variant', 'Executed', 'Malicious blocked', 'False negatives', 'Legitimate allowed', 'False positives', 'Readback invariants'], [
            [mode, len(group), *counters(group), f'{sum(bool(r.get("px4_invariant_passed")) for r in group)}/{len(group)}']
            for mode in ['FULL_DRACO', 'ABLATION_NO_DELTA', 'ABLATION_NO_INTENT', 'ABLATION_NO_CAUSALITY',
                         'ABLATION_NO_FRESH_EVIDENCE', 'ABLATION_NO_CHANGE_BUDGET']
            for group in [[r for r in live if r.get('evaluation_variant') == mode]]]),
        'The no-causality concurrent case is still DEFERred by the immutable active-transfer guard; that is not semantic conflict detection. '
        'The frozen suite supplies fresh evidence, so the freshness ablation has identical blocking counts here; the dedicated '
        'missing/stale/invalid evidence regressions test the boundary. Budget ablation likewise need not create a false negative '
        'when independent intent/causality gates still block the chosen attack.\n',
        '## N. Latency, scaling, and accuracy\n',
        'Microseconds; linear interpolation at `(n-1)*p`. These are recorded deterministic local samples, not confidence intervals.\n',
        table(['Full-DRACO live stage', 'n', 'p50', 'p95', 'p99'], [
            [name, len(full), *[percentile([r.get(key) for r in full], p) for p in (.5, .95, .99)]]
            for name, key in [('Canonicalization', 'canonicalization_latency_us'), ('Mission hash', 'mission_hash_latency_us'),
                ('Semantic delta', 'semantic_delta_latency_us'), ('Authorization/policy', 'authorization_latency_us'),
                ('Total decision', 'decision_latency_us'), ('Client transaction completion', 'transaction_duration_us')]]),
        table(['Mission items', 'Samples', 'p50 decision', 'p95', 'p99', 'Delta checks', 'Alignment checks'], [
            [size, len(group), *[percentile([r['decision_latency_us'] for r in group], p) for p in (.5, .95, .99)],
             f'{sum(r["delta_correct"] for r in group)}/{len(group)}', f'{sum(r["alignment_correct"] for r in group)}/{len(group)}']
            for size in (10, 50, 100, 500, 1000)
            for group in [[r for r in scale if r['mission_item_count'] == size]]]),
        f'The separate ten-case labeled delta corpus produced {accuracy["true_positive_labels"]} true-positive labels, '
        f'{accuracy["false_positive_labels"]} false-positive labels, {accuracy["false_negative_labels"]} false-negative labels; '
        f'precision {accuracy["precision"]:.3f}, recall {accuracy["recall"]:.3f}, F1 {accuracy["f1"]:.3f}. '
        f'Annotated alignment checks: {accuracy["alignment_correct"]}/{accuracy["alignment_checks"]}. '
        'These scores apply only to the explicit regression corpus. The scale benchmark changes one interior item; it does not '
        'claim worst-case replacement complexity or live transport performance for 1000 items.\n',
        '## O. Dashboard\n',
        'Dependency-light Python loopback HTTP observer with static HTML/JavaScript. It is GET-only, reads recorded files, '
        'and has no policy-editing, escalation, mission-send, or authorization actions. Variant selection only filters display. '
        'It shows connections/evidence, current mission, proposal/principal, semantic delta, causality, policy, decisions, '
        'PX4 transfer/ACK/commit flow, scenario outcomes, full readback hashes, counters, latency, and scaling charts.\n\n'
        '```bash\npython3 evaluation/observer.py --events evaluation/results/raw/final_run --port 8765\n```\n\n'
        'Open `http://127.0.0.1:8765`; standalone output is `evaluation/results/report/index.html`. '
        'Browser rendering and switching to the causality ablation were inspected; no browser errors were observed. '
        'No screenshot file was saved. Connection/evidence fields are the last recorded observations, not a claim of continuing connectivity.\n',
        '## P. Known limitations\n',
        '- No production authenticated GCS binding, signing baseline, durable revision journal, or automatic PX4 reconciliation on restart.\n'
        '- Expected-parent provenance is an explicit local evaluation sidecar; standard mission upload does not carry an authenticated parent. Normal uploads snapshot the current parent at start.\n'
        '- Geometry retains frozen point-based semantics; runtime altitude support is relative-home only. No flight-safety certification or policy-transition intervention.\n'
        '- Complete integer mission uploads only; maximum 1000 items; unsupported mission write forms fail closed. Frames split across UDP datagrams are dropped.\n'
        '- Other vehicle command families are outside this mission-revision gate. Trusted-host/PX4-side access is outside the authenticated GCS simulation.\n'
        '- Three-second evidence freshness remains frozen. Undefined landed states and impossible global coordinates now set invalid evidence.\n'
        '- Local scenario counts are small; scaling uses one changed waypoint and synthetic evidence. No general false-positive/negative population estimates are claimed.\n',
        '## Q. Unimplemented items\n',
        '- BASELINE_B, BASELINE_C, and BASELINE_D: explicitly NOT_IMPLEMENTED for the reasons above.\n'
        '- BASELINE_A concurrent-client scheduling: NOT_IMPLEMENTED; no substituted result.\n'
        '- Real authenticated principal binding and signing secrets: not available in this repository; normal mode remains closed.\n'
        '- No live stale-evidence attack extension was added to the frozen scenario population; freshness boundaries are covered by explicit regression tests.\n'
        '- No optional dashboard run controls, policy hot reload, durable-history recovery, or automatic aircraft intervention.\n',
        '## R. Git review\n',
        '`git status --short`:\n\n```text\n' + status + '\n```\n\n',
        '`git diff --stat` (tracked modifications only; newly created files above are untracked):\n\n```text\n' + diffstat + '\n```\n\n',
        'Significant diffs: the gateway transport is reorganized around separate immutable authorized and incoming transactions; '
        'trusted runtime provisioning and the shared measured decision pipeline replace live placeholders; the approved no-op '
        'success return moves after current intent validation; explicit evaluation-only ablations are added; invalid telemetry '
        'is marked unusable; accepted PX4 ACKs fail closed if local revision commit fails; and new runners, tests, structured output, '
        'and observer/report artifacts provide reproducible evaluation.\n\n'
        'The current-source branding/placeholder search found no prohibited occurrences. The active branch remains `evaluation`; '
        '`main` was not changed. No commit, push, force-push, or history rewrite was performed. Awaiting human review.\n']
    output.write_text('\n'.join(parts), encoding='utf-8')
    print(output)


if __name__ == '__main__':
    main()
