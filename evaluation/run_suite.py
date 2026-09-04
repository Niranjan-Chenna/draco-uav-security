"""Run the local experiment suite sequentially; shared simulator ports are never run in parallel."""
import argparse
import json
import pathlib
import subprocess
from run_live import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    subprocess.run([str(ROOT / 'build/core_runner'), str(output / 'core')], cwd=ROOT, check=True)
    modes = ['FULL_DRACO', 'ABLATION_NO_DELTA', 'ABLATION_NO_INTENT', 'ABLATION_NO_CAUSALITY',
             'ABLATION_NO_FRESH_EVIDENCE', 'ABLATION_NO_CHANGE_BUDGET']
    outcomes = []
    for mode in modes:
        print('RUN', mode, flush=True)
        result = subprocess.run(['python3', 'evaluation/run_live.py', '--fixtures', str(output / 'core'),
            '--output', str(output / 'live' / mode), '--mode', mode], cwd=ROOT)
        outcomes.append(dict(mode=mode, runner_exit=result.returncode))
    result = subprocess.run(['python3', 'evaluation/run_baseline.py', '--fixtures', str(output / 'core'),
        '--output', str(output / 'baseline')], cwd=ROOT)
    outcomes.append(dict(mode='BASELINE_A', runner_exit=result.returncode))
    result = subprocess.run(['python3', 'evaluation/check_runtime_invariant.py', '--mission', str(output / 'core/base.mission'),
        '--output', str(output / 'runtime')], cwd=ROOT)
    outcomes.append(dict(mode='NORMAL_UNAUTHENTICATED', runner_exit=result.returncode))
    result = subprocess.run([str(ROOT / 'build/delta_accuracy'), str(output / 'delta_accuracy')], cwd=ROOT)
    outcomes.append(dict(mode='DELTA_ACCURACY', runner_exit=result.returncode))
    subprocess.run(['python3', 'evaluation/finalize_results.py', str(output)], cwd=ROOT, check=True)
    (output / 'runner_status.json').write_text(json.dumps(outcomes, indent=2))
    return int(any(r['runner_exit'] != 0 for r in outcomes))


if __name__ == '__main__':
    raise SystemExit(main())
