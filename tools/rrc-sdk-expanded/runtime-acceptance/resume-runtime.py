from pathlib import Path
p=Path('/workspace/scratch/31a28362a951/NRForge-asn1c-p9/tools/rrc-sdk-expanded-consumer/isolate.py')
s=p.read_text()
s=s.replace('work.mkdir(parents=True, exist_ok=False)','assert work.is_dir()')
a=s.index('    dest = work / "installed-rrc"')
b=s.index('    prepared = ',a)
s=s[:a]+'''    copied["rrc"] = work / "installed-rrc"
    example = work / "example"
    source = work / "consumer"
'''+s[b:]
a=s.index('        run("configure", config)')
b=s.index('        counts = negatives = 0',a)
s=s[:a]+s[b:]
s=s.replace('        run(\n            "example_configure",','        run(\n            "example_configure",')
s=s.replace('        run(\n            "example_configure",', '        run(\n            "example_configure",')
s=s.replace('        if counts != 248:', '        if negatives != 5510:\n            raise ValueError("negative count mismatch: " + str(negatives))\n        if counts != 248:')
exec(compile(s,str(p),'exec'))
