"""Command evidence and lossless JSON serialization for the terminal campaign."""
import hashlib
import json
import os
from pathlib import Path
import signal
import shlex
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def json_value(value):
    if isinstance(value, bytes):
        return {'bytes_hex': value.hex()}
    if isinstance(value, tuple):
        return {'tuple': [json_value(x) for x in value]}
    if isinstance(value, list):
        return [json_value(x) for x in value]
    if isinstance(value, dict):
        return {str(k): json_value(v) for k, v in value.items()}
    return value


def write(path, value):
    Path(path).write_text(json.dumps(json_value(value), indent=2, sort_keys=True) + '\n')


def command(directory, args, *, cwd=None, data=None, timeout=900, live=False):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    args = [str(x) for x in args]
    write(directory/'command.json', {'argv': args, 'cwd': str(cwd) if cwd else None,
                                     'timeout_seconds': timeout})
    (directory/'command.txt').write_text(shlex.join(args) + '\n')
    (directory/'status').write_text('RUNNING\n')
    if data is not None:
        (directory/'stdin.txt').write_text(data)
    print('RUN', directory.name, flush=True)
    with (directory/'stdout.txt').open('w') as out, (directory/'stderr.txt').open('w') as err:
        try:
            if live:
                if data is not None:
                    raise ValueError('Live command does not accept stdin')
                process = subprocess.Popen(args, cwd=cwd, stdin=subprocess.DEVNULL,
                    text=True, stdout=out, stderr=err, start_new_session=True)
                try:
                    deadline = time.monotonic()+timeout
                    with (directory/'stdout.txt').open() as reader:
                        while process.poll() is None:
                            print(reader.read(), end='', flush=True)
                            if time.monotonic() >= deadline:
                                raise subprocess.TimeoutExpired(args, timeout)
                            time.sleep(0.2)
                        print(reader.read(), end='', flush=True)
                    result = subprocess.CompletedProcess(args, process.wait())
                except BaseException:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
                    raise
            else:
                result = subprocess.run(args, cwd=cwd, input=data, text=True,
                                        stdout=out, stderr=err, timeout=timeout)
        except BaseException:
            (directory/'status').write_text('FAIL_OR_INTERRUPTED\n')
            raise
    (directory/'status').write_text(f'EXIT {result.returncode}\n')
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}); see {directory}')
    return (directory/'stdout.txt').read_text()
