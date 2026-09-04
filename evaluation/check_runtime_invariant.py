"""Verify normal unauthenticated mode against actual PX4 mission readback."""
import argparse
import json
import pathlib
import subprocess
from run_live import ROOT, client, wait_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mission', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    with (args.output / 'gateway.log').open('w') as log:
        gateway = subprocess.Popen([str(ROOT / 'build/draco'), '--policy', str(ROOT / 'config/sitl_policy.conf'),
            '--results', str(args.output)], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        try:
            event_file = args.output / 'events.jsonl'
            wait_for(event_file, lambda e: e.get('event_type') == 'status' and e.get('evidence_usable'))
            before = client('download', args.output / 'before.mission')
            ack = client('upload', args.mission)
            decision = wait_for(event_file, lambda e: e.get('event_type') == 'decision')
            after = client('download', args.output / 'after.mission')
            correct = (decision['authorization_decision'] == 'DEFER' and
                       decision['authorization_reason'] == 'PRINCIPAL_NOT_AUTHENTICATED' and
                       before['hash'] == after['hash'] and not decision['principal_authenticated'] and
                       not decision['evaluation_mode'] and ack['ack_result'] != 0)
            decision.update(event_type='runtime_invariant_result', scenario_id='NORMAL_UNAUTHENTICATED',
                scenario_class='runtime', evaluation_variant='RUNTIME_CHECKS', execution_status='EXECUTED',
                measurement_scope='LOCAL_PX4_SITL', expected_outcome='DEFER', outcome_correct=correct,
                px4_mission_hash_before=before['hash'], px4_mission_hash_after=after['hash'],
                px4_invariant_passed=before['hash'] == after['hash'], px4_ack_result=None,
                gcs_ack_result=ack['ack_result'], px4_upload_started=False)
            (args.output / 'results.jsonl').write_text(json.dumps(decision) + '\n')
            print(json.dumps(decision))
            return 0 if correct else 1
        finally:
            gateway.terminate()
            gateway.wait(timeout=5)


if __name__ == '__main__':
    raise SystemExit(main())
