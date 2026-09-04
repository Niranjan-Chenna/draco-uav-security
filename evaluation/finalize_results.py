"""Join measured gateway transfer/ACK events to scenario readbacks and export canonical CSV."""
import argparse
import csv
import json
import pathlib
from run_live import records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=pathlib.Path)
    args = parser.parse_args()
    combined = []
    for path in args.run.rglob('results.jsonl'):
        rows = records(path)
        for row in rows:
            if row.get('event_type') == 'live_scenario_result':
                stem = row['scenario_id'] + ('_' + row.get('variant', '') if row.get('variant') else '')
                events = records(path.parent / row['evaluation_variant'] / stem / 'events.jsonl')
                related = [e for e in events if e.get('revision_id') == row['revision_id']]
                row.setdefault('gcs_ack_result', row.get('px4_ack_result'))
                row['px4_upload_started'] = any(e.get('event_type') == 'authorized_upload_started' for e in related)
                row['px4_ack_result'] = next((e.get('px4_ack_result') for e in reversed(related)
                    if e.get('event_type') in ('revision_committed', 'revision_rejected')), None)
                if row['authorization_decision'] != 'ALLOW' and row['px4_upload_started']:
                    raise RuntimeError('non-ALLOW scenario started a PX4 upload')
            combined.append(row)
        path.write_text(''.join(json.dumps(r) + '\n' for r in rows))
        keys = sorted(set().union(*(r.keys() for r in rows)))
        with path.with_suffix('.csv').open('w', newline='') as output:
            writer = csv.DictWriter(output, fieldnames=keys)
            writer.writeheader()
            writer.writerows(rows)
    keys = sorted(set().union(*(r.keys() for r in combined)))
    with (args.run / 'decisions.csv').open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=keys)
        writer.writeheader()
        writer.writerows(combined)
    print('Joined', len(combined), 'recorded scenario/runtime/baseline records')


if __name__ == '__main__':
    main()
