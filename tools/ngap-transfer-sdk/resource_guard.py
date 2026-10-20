#!/usr/bin/env python3
"""Run the whole build descendant tree under a verified cgroup v2 hard limit."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

MAX_MEMORY = 10 * 1024**3


def verify():
    lines = Path('/proc/self/cgroup').read_text().splitlines()
    relative = next((line[3:] for line in lines if line.startswith('0::')), None)
    if relative is None:
        raise RuntimeError('cgroup v2 is required; no unbounded fallback')
    group = Path('/sys/fs/cgroup')/relative.lstrip('/')
    memory = (group/'memory.max').read_text().strip()
    tasks = (group/'pids.max').read_text().strip()
    if memory == 'max' or int(memory) > MAX_MEMORY or tasks == 'max' or int(tasks) > 128:
        raise RuntimeError('Build scope lacks the required total memory/tasks limits')
    if (group/'memory.swap.max').read_text().strip() != '0':
        raise RuntimeError('Build scope must not consume swap')
    quota, period = (group/'cpu.max').read_text().split()
    if quota == 'max' or int(quota)*2 > int(period)*3:
        raise RuntimeError('Build scope lacks the CPU quota')
    return dict(cgroup=str(group), memory_max=int(memory), tasks_max=int(tasks),
                swap_max=0, cpu_quota=quota, cpu_period=int(period),
                _cgroup_path=str(group))


def record_accounting(identity):
    """Record post-build accounting from the same cgroup as the build."""
    group = Path(identity.pop('_cgroup_path'))
    identity['memory_peak'] = int((group/'memory.peak').read_text().strip())
    events_text = (group/'memory.events').read_text()
    identity['memory_events'] = {
        key: int(value) for key, value in (line.split() for line in events_text.splitlines())
    }
    identity['memory_events_raw'] = events_text
    return identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inside', action='store_true')
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('a command is required')
    evidence = args.evidence.expanduser().resolve()
    if args.inside:
        identity = verify()
        (evidence/'resource-limit.json').write_text(json.dumps(identity, indent=2)+'\n')
        result = 1
        try:
            result = subprocess.run(command).returncode
        finally:
            record_accounting(identity)
            (evidence/'resource-limit.json').write_text(json.dumps(identity, indent=2)+'\n')
        raise SystemExit(result)
    evidence.mkdir(parents=True, exist_ok=False)
    status = evidence/'guard-status.json'
    status.write_text(json.dumps(dict(status='RUNNING', command=command))+'\n')
    print('Evidence:', evidence, 'Logs: scope.stdout / scope.stderr', flush=True)
    unit = 'nrforge-transfer-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    argv = ['systemd-run', '--user', '--scope', '--unit='+unit,
        '--property=MemoryAccounting=yes', '--property=MemoryMax='+str(MAX_MEMORY),
        '--property=MemorySwapMax=0', '--property=TasksMax=128',
        '--property=CPUQuota=150%', sys.executable, str(Path(__file__).resolve()),
        '--inside', '--evidence', str(evidence), '--', *command]
    result = 1
    try:
        # The supervising parent remains outside the build scope so a cgroup
        # OOM can still leave a conclusive failure record. No automatic retry.
        with (evidence/'scope.stdout').open('w') as out, (evidence/'scope.stderr').open('w') as err:
            result = subprocess.run(argv, stdin=subprocess.DEVNULL, stdout=out, stderr=err).returncode
        if result == 0 and not (evidence/'resource-limit.json').is_file():
            result = 1
    finally:
        status.write_text(json.dumps(dict(status='EXIT', exit_code=result, command=argv), indent=2)+'\n')
        print('Resource scope exit:', result, 'Evidence:', evidence, flush=True)
    raise SystemExit(result)


if __name__ == '__main__':
    main()
