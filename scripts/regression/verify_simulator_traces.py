"""Compare fresh normal/sanitized runs without overwriting research references."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

TESTS = ['oputiletest', 'bankingtest', 'multicoretest', 'tilingtest']


def capture(build, name):
    executable = build.resolve()/'tests'/'cpp'/name
    with tempfile.TemporaryDirectory(prefix='pim-trace-') as tmp:
        environment = dict(os.environ, PIM_ARTIFACTS_DIR=tmp)
        completed = subprocess.run([str(executable)], cwd=tmp, env=environment,
                                   capture_output=True, text=True, timeout=60)
        if completed.returncode:
            raise RuntimeError(f'{name} failed: {completed.stdout}\n{completed.stderr}')
        if 'Simulation stopped: Completed' not in completed.stdout:
            raise RuntimeError(f'{name} did not report workload completion')
        cycles = re.search(r'Evaluated cycles: (\d+)', completed.stdout)
        traces = sorted((Path(tmp) / 'runs').glob('*completed_events*.csv'))
        if not traces or cycles is None:
            raise RuntimeError(f'{name} did not produce cycles and event trace')
        events = sorted(line for p in traces for line in p.read_text().splitlines())
        return {'cycles': int(cycles[1]), 'events': len(events),
                'trace_sha256': hashlib.sha256('\n'.join(events).encode()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--normal', required=True, type=Path)
    parser.add_argument('--sanitized', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    report = {}
    for name in TESTS:
        normal, repeat, sanitized = capture(args.normal,name), capture(args.normal,name), capture(args.sanitized,name)
        if normal != repeat or normal != sanitized:
            raise AssertionError(f'{name}: normal={normal}, repeat={repeat}, sanitized={sanitized}')
        report[name] = {'normal': normal, 'repeat': repeat, 'sanitized': sanitized, 'equal': True}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as handle:
        json.dump(report,handle,indent=2)
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
