#!/usr/bin/env python3
"""Run installed consumers while original build/schema/source paths are unavailable.

Use only after both builds and fixture preparation finish. Always restore renamed
paths, including on consumer failure. Work must be outside every hidden path.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import os
import uuid


def copy_sdk_file(source, destination):
    if Path(source).suffix == '.a':
        try:
            os.link(source, destination)
            return destination
        except OSError:
            pass
    return shutil.copy2(source, destination)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'f1-work', 'ng-work', 'schema', 'f1-prefix', 'ng-prefix', 'all-slots', 'work'):
        ap.add_argument('--'+name, type=Path, required=True)
    ap.add_argument('--cmake', default='cmake')
    ap.add_argument('--jobs', type=int, default=1)
    ap.add_argument('--linker', default='gold')
    args = ap.parse_args()
    work = args.work.resolve()
    hidden = [getattr(args,name).resolve() for name in
              ('repo','f1_work','ng_work','schema','f1_prefix','ng_prefix')]
    requested_hidden = list(hidden)
    hidden = [a for a in set(hidden) if not any(a.is_relative_to(b) for b in hidden if a != b)]
    if any(work.is_relative_to(path) for path in hidden):
        raise ValueError('consumer workspace must be outside hidden inputs')
    work.mkdir(parents=True, exist_ok=False)
    tools = work/'runner/tools'
    tools.mkdir(parents=True)
    for name in ('f1ap-sdk-consumer','ngap-sdk-consumer'):
        shutil.copytree(args.repo/'tools'/name,tools/name,ignore=shutil.ignore_patterns('__pycache__'))
    f1, ng, slots = work/'f1-input', work/'ng-input', work/'slot-input'
    shutil.copytree(args.f1_prefix,f1,copy_function=copy_sdk_file)
    shutil.copytree(args.ng_prefix,ng,copy_function=copy_sdk_file)
    shutil.copytree(args.all_slots,slots)
    renamed = []
    try:
        # Changing cwd permits restoring the repository even when the caller
        # initially invoked this script from inside that repository.
        os.chdir(work)
        for path in hidden:
            unavailable = path.with_name('.unavailable-'+path.name+'-'+uuid.uuid4().hex)
            if unavailable.exists(): raise ValueError('unavailable path already exists')
            path.rename(unavailable)
            renamed.append((path,unavailable))
        if any(path.exists() for path in requested_hidden): raise RuntimeError('original input still available')
        commands = [
            ['python3',str(tools/'f1ap-sdk-consumer/verify.py'),'--prefix',str(f1),'--ngap-prefix',str(ng),
             '--all-slots',str(slots),'--work',str(work/'f1-consumer'),'--output',str(work/'f1-summary.json'),
             '--jobs',str(args.jobs),'--cmake',args.cmake,'--linker',args.linker],
            ['python3',str(tools/'ngap-sdk-consumer/verify.py'),'--prefix',str(ng),
             '--work',str(work/'ng-consumer'),'--output',str(work/'ng-summary.json'),'--cmake',args.cmake,'--linker',args.linker]]
        for index, command in enumerate(commands):
            with (work/f'consumer-{index}.log').open('w') as log:
                subprocess.run(command,cwd=work,stdout=log,stderr=subprocess.STDOUT,check=True)
        (work/'isolation-summary.json').write_text(json.dumps({
            'status':'PASS','original_paths_unavailable':list(map(str,requested_hidden)),
            'f1ap':json.loads((work/'f1-summary.json').read_text()),
            'ngap':json.loads((work/'ng-summary.json').read_text())},indent=2)+'\n')
    finally:
        failures = []
        for original, unavailable in reversed(renamed):
            try:
                if original.exists():
                    # Some environments recreate empty cwd/build scaffolds.
                    # Remove only directories whose entire tree is empty.
                    if original.is_symlink() or not original.is_dir() or any(p.is_symlink() or not p.is_dir() for p in original.rglob('*')):
                        raise RuntimeError('refusing to overwrite recreated content: '+str(original))
                    shutil.rmtree(original)
                unavailable.rename(original)
            except Exception as error:
                failures.append(str(error))
        if failures: raise RuntimeError('restore failed: '+ '; '.join(failures))
    print('PASS both installed consumers with original source/schema/build/install paths unavailable')


if __name__ == '__main__':
    main()
