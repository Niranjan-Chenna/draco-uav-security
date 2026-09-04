"""Measure direct plain-MAVLink mission uploads to the owned loopback PX4 instance."""
import argparse
import json
import pathlib
from run_live import ROOT, client, records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    success = True
    with (args.output / 'results.jsonl').open('w') as stream:
        for case in records(args.fixtures / 'scenarios.jsonl'):
            result = dict(case, baseline='BASELINE_A', measurement_scope='DIRECT_LOCAL_PX4_SITL',
                          event_type='baseline_result')
            stem = case['scenario_id'] + ('_' + case['variant'] if case['variant'] else '')
            try:
                if case['concurrent']:
                    result.update(execution_status='NOT_IMPLEMENTED',
                                  limitation='direct-client concurrent ownership coordination is not implemented')
                else:
                    if case['in_flight']:
                        result['takeoff'] = client('flight', 'takeoff', 18570, 14550)
                    if case['historical_parent']:
                        setup = client('upload', args.fixtures / 'base.mission', 18570, 14550)
                        if setup['ack_result'] != 0:
                            raise RuntimeError('baseline history setup rejected')
                    setup = client('upload', args.fixtures / case['starting_file'], 18570, 14550)
                    if setup['ack_result'] != 0:
                        raise RuntimeError('baseline starting mission rejected')
                    before = client('download', args.output / (stem + '_before.mission'), 18570, 14550)
                    proposed_hash = client('hash', args.fixtures / case['proposed_file'])['hash']
                    actual = client('upload', args.fixtures / case['proposed_file'], 18570, 14550)
                    after = client('download', args.output / (stem + '_after.mission'), 18570, 14550)
                    accepted = actual['ack_result'] == 0
                    result.update(execution_status='EXECUTED', px4_ack_result=actual['ack_result'],
                                  authorization_decision='ALLOW' if accepted else 'PX4_REJECTED',
                                  px4_mission_hash_before=before['hash'], px4_mission_hash_after=after['hash'],
                                  proposal_hash=proposed_hash, px4_changed=before['hash'] != after['hash'],
                                  px4_invariant_passed=after['hash'] == proposed_hash if accepted else before['hash'] == after['hash'])
            except Exception as error:
                result.update(execution_status='ERROR', error=str(error))
            finally:
                if case['in_flight']:
                    try:
                        result['landing'] = client('flight', 'land', 18570, 14550)
                    except Exception as error:
                        result['landing_error'] = str(error)
            stream.write(json.dumps(result) + '\n')
            if result['execution_status'] == 'ERROR' or result.get('landing_error'):
                success = False
            stream.flush()
            print(case['scenario_id'], result['execution_status'], result.get('authorization_decision'), flush=True)
        for baseline, explanation in {
            'BASELINE_B': 'no independently verified MAVLink signing/key provisioning baseline is installed',
            'BASELINE_C': 'signing baseline and independently controlled native geofence configuration are unavailable',
            'BASELINE_D': 'no faithful external stateful-proxy implementation is available in this repository',
        }.items():
            stream.write(json.dumps(dict(event_type='baseline_availability', baseline=baseline,
                                         execution_status='NOT_IMPLEMENTED', reason=explanation)) + '\n')
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
