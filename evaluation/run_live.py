"""Run deterministic loopback mission transactions against project-owned PX4 SITL."""
import argparse
import csv
import json
import pathlib
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent


def records(path):
    if not path.exists():
        return []
    result = []
    for line in path.read_text().splitlines():
        try:
            result.append(json.loads(line))
        except json.JSONDecodeError:
            pass  # a writer may still be completing the final line.
    return result


def client(action, path, target=14560, local=14600):
    command = [str(ROOT / 'build/mission_client'), action, str(path)]
    if action != 'hash':
        command += [str(target), str(local)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=30)
    output = json.loads(result.stdout.strip().splitlines()[-1])
    if result.returncode not in (0, 3):
        raise RuntimeError(output.get('error', result.stderr))
    return output


def wait_for(path, predicate, timeout=12):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for event in reversed(records(path)):
            if predicate(event):
                return event
        time.sleep(.1)
    raise TimeoutError('required structured event did not arrive')


def context(directory, scenario, parent, local=14600):
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / f'{local}.conf'
    temporary = destination.with_suffix('.tmp')
    temporary.write_text(f'scenario_id={scenario}\nexpected_parent_hash={parent}\n')
    temporary.replace(destination)


def run_case(scenario, fixtures, output, mode, normal=False):
    stem = scenario['scenario_id'] + ('_' + scenario['variant'] if scenario['variant'] else '')
    directory = output / mode / stem
    directory.mkdir(parents=True, exist_ok=False)
    contexts = directory / 'context'
    event_file = directory / 'events.jsonl'
    command = [str(ROOT / 'build/draco'), '--policy', str(ROOT / 'config/sitl_policy.conf'),
               '--results', str(directory)]
    if not normal:
        command += ['--evaluation', '--principal', 'sitl-normal-operator', '--authority', 'NORMAL_OPERATOR',
                    '--mode', mode, '--evaluation-context', str(contexts)]
    if scenario['concurrent']:
        command += ['--evaluation-upload-delay-ms', '2000']
    result = dict(scenario, evaluation_variant=mode, measurement_scope='LOCAL_PX4_SITL',
                  px4_mission_hash_before=None, px4_mission_hash_after=None, outcome_correct=None)
    with (directory / 'gateway.log').open('w') as logfile:
        gateway = subprocess.Popen(command, cwd=ROOT, stdout=logfile, stderr=subprocess.STDOUT)
        try:
            status = wait_for(event_file, lambda e: e.get('event_type') == 'status' and e.get('evidence_usable'))
            if scenario['in_flight']:
                result['takeoff'] = client('flight', 'takeoff', local=14602)
                status = wait_for(event_file, lambda e: e.get('event_type') == 'status' and e.get('evidence_usable') and
                                  e.get('armed') == 'armed' and e.get('landed_state') == '2')
            parent = ''
            if scenario['historical_parent']:
                context(contexts, 'SETUP_A', '')
                setup = client('upload', fixtures / 'base.mission')
                if setup['ack_result'] != 0:
                    raise RuntimeError('PX4 did not accept history setup A')
                parent = setup['proposal_hash']
            context(contexts, 'SETUP_STARTING', parent)
            setup = client('upload', fixtures / scenario['starting_file'])
            if setup['ack_result'] != 0:
                raise RuntimeError('PX4 did not accept starting mission')
            before = client('download', directory / 'px4_before.mission')
            proposed = client('hash', fixtures / scenario['proposed_file'])['hash']
            if scenario['scenario_id'] != 'ATTACK_STALE_PARENT':
                parent = setup['proposal_hash']
            context(contexts, scenario['scenario_id'], parent)
            rival = None
            if scenario['concurrent']:
                context(contexts, 'CONCURRENT_RIVAL', setup['proposal_hash'], local=14601)
                rival = subprocess.Popen([str(ROOT / 'build/mission_client'), 'upload',
                    str(fixtures / 'BENIGN_SMALL_CORRECTION_proposed.mission'), '14560', '14601'],
                    cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                wait_for(event_file, lambda e: e.get('event_type') == 'authorized_upload_pending' and
                         e.get('scenario_id') == 'CONCURRENT_RIVAL')
            begin = time.monotonic_ns()
            actual = client('upload', fixtures / scenario['proposed_file'])
            duration = (time.monotonic_ns() - begin) / 1000
            decision = wait_for(event_file, lambda e: e.get('event_type') == 'decision' and e.get('scenario_id') == scenario['scenario_id'])
            after = client('download', directory / 'px4_after.mission')
            if rival:
                rival_stdout, rival_stderr = rival.communicate(timeout=30)
                if rival.returncode != 0:
                    raise RuntimeError('independent authorized rival failed: ' + rival_stdout + rival_stderr)
                result['concurrent_rival_ack'] = json.loads(rival_stdout)['ack_result']
            allowed = decision['authorization_decision'] == 'ALLOW'
            related = [e for e in records(event_file) if e.get('revision_id') == decision['revision_id']]
            started = any(e.get('event_type') == 'authorized_upload_started' for e in related)
            px4_ack = next((e.get('px4_ack_result') for e in reversed(related)
                            if e.get('event_type') in ('revision_committed', 'revision_rejected')), None)
            invariant = after['hash'] == proposed if allowed and actual['ack_result'] == 0 else before['hash'] == after['hash']
            correct = (decision['authorization_decision'] == scenario['expected_decision'] and
                       decision['authorization_reason'] == scenario['expected_reason'] and
                       decision['causality_class'] == scenario['expected_causality'] and invariant and
                       (not allowed or actual['ack_result'] == 0))
            result.update(decision)
            result.update(event_type='live_scenario_result', execution_status='EXECUTED',
                          measurement_scope='LOCAL_PX4_SITL', scenario_class=scenario['scenario_class'],
                          variant=scenario['variant'], expected_outcome=scenario['expected_decision'],
                          expected_reason=scenario['expected_reason'], gcs_ack_result=actual['ack_result'],
                          px4_ack_result=px4_ack, px4_upload_started=started,
                          px4_mission_hash_before=before['hash'], px4_mission_hash_after=after['hash'],
                          px4_changed=before['hash'] != after['hash'], px4_invariant_passed=invariant,
                          outcome_correct=correct, transaction_duration_us=duration)
            return result
        finally:
            if scenario['in_flight'] and gateway.poll() is None:
                try:
                    result['landing'] = client('flight', 'land', local=14602)
                except Exception as error:
                    result['landing_error'] = str(error)
            gateway.terminate()
            try:
                gateway.wait(timeout=5)
            except subprocess.TimeoutExpired:
                gateway.kill()
                gateway.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=pathlib.Path, default=ROOT / 'evaluation/results/raw/core')
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--mode', default='FULL_DRACO', choices=['FULL_DRACO', 'ABLATION_NO_DELTA',
        'ABLATION_NO_INTENT', 'ABLATION_NO_CAUSALITY', 'ABLATION_NO_FRESH_EVIDENCE', 'ABLATION_NO_CHANGE_BUDGET'])
    parser.add_argument('--scenario')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    scenarios = records(args.fixtures / 'scenarios.jsonl')
    success = True
    with (args.output / 'results.jsonl').open('a') as output:
        for scenario in scenarios:
            if args.scenario and scenario['scenario_id'] != args.scenario:
                continue
            try:
                result = run_case(scenario, args.fixtures, args.output, args.mode)
            except Exception as error:
                result = dict(scenario, execution_status='ERROR', error=str(error),
                              evaluation_variant=args.mode, measurement_scope='LOCAL_PX4_SITL')
            output.write(json.dumps(result) + '\n')
            output.flush()
            print(scenario['scenario_id'], scenario['variant'], result.get('execution_status'),
                  result.get('authorization_decision'), result.get('px4_invariant_passed'), result.get('error', ''), flush=True)
            if result['execution_status'] in ('ERROR', 'NOT_RUN') or result.get('landing_error') or (args.mode == 'FULL_DRACO' and
                    result['execution_status'] == 'EXECUTED' and not result['outcome_correct']):
                success = False
    all_results = records(args.output / 'results.jsonl')
    keys = sorted(set().union(*(r.keys() for r in all_results)))
    with (args.output / 'results.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=keys)
        writer.writeheader()
        writer.writerows(all_results)
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
