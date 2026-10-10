"""Native aligned PER collection checks; expected oracle differences stay explicit."""
import argparse
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument("--build", type=Path)
parser.add_argument("--generated", type=Path)
parser.add_argument("--cxx", default="c++")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
build = (args.build or repo).resolve()
source = Path(__file__).resolve().parent
for package, version in (("pycrate", "0.7.11"), ("asn1tools", "0.167.0")):
    if importlib.metadata.version(package) != version:
        raise RuntimeError(f"Require {package}=={version}; assess profiles before upgrading")
temp = tempfile.TemporaryDirectory(prefix="n8-native-")
work = Path(temp.name)
generated = args.generated.resolve() if args.generated else work / "generated"
if not args.generated:
    generated.mkdir()
    subprocess.run([str(build / "libasn1typed/check_asn1typed_collection_render"),
                    str(repo / "libasn1typed/fixtures/collection-generation-n8.asn1"),
                    "CollectionGeneration", "collectiontest", str(generated)], check=True)
subprocess.run(shlex.split(args.cxx) + ["-std=c++20", "-Wall", "-Wextra", "-Werror",
               "-pedantic-errors", "-Wconversion", "-Wsign-conversion", "-I" + str(repo / "libaper"),
               "-I" + str(generated), str(source / "driver.cpp"), str(repo / "libaper/runtime.cpp"),
               "-o", str(work / "driver")], check=True)

from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator
import asn1tools

types = {"Bits": [0, 1, 2, 3], "Words": [0, 1, 2, 3], "Pairs": [1, 2],
         "Picks": [0, 1, 2, 3], "Nested": [0, 1, 2], "FixedThree": [3], "Empty": [0],
         "ShortEight": [0, 1, 127, 128, 253, 254], "OctetEight": [0, 1, 127, 128, 254, 255],
         "WideSixteen": [0, 1, 127, 128, 255, 256], "FullSixteen": [0, 1, 255, 256, 65534, 65535],
         "ExtPairs": [0, 1, 2], "ZeroBitTriple": [3]}
sender = (source / "sender.asn1").read_text()
wrappers = []
for residue in range(1, 8):
    for name in types:
        fields = [f"p{i} BOOLEAN" for i in range(residue)] + [f"values {name}"]
        wrappers.append(f"P{residue}{name} ::= SEQUENCE {{ " + ", ".join(fields) + " }")
schema = sender.replace("END", "\n".join(wrappers) + "\nEND")
compile_text(schema)
PycrateGenerator(str(work / "oracle.py"))
spec = importlib.util.spec_from_file_location("n8_native_oracle", work / "oracle.py")
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
primary = native.CollectionNativeSender
secondary = asn1tools.compile_string(schema, "per")

def pair(i, pattern):
    value = {"value": (i * 17 + pattern) % 256}
    presence = (i + pattern) % 3
    if presence:
        value["marker"] = presence == 2
    return value

def value(name, count, pattern):
    if name == "ZeroBitTriple":
        return [{} for _ in range(count)]
    if name == "Words":
        return [(i * 17 + pattern) % 256 for i in range(count)]
    if name == "Pairs":
        return [pair(i, pattern) for i in range(count)]
    if name == "Picks":
        return [("pair", pair(i, pattern)) if (i + pattern) % 2
                else ("flag", (i + pattern) % 3 == 0) for i in range(count)]
    if name == "Nested":
        return [value("Bits", (i + pattern) % 4, pattern + i) for i in range(count)]
    if name == "ExtPairs":
        return [{"value": (i * 17 + pattern) % 256} for i in range(count)]
    return [(i + pattern) % 3 == 0 for i in range(count)]

cases = []
for name, counts in types.items():
    for residue in range(8):
        for count in counts:
            for pattern in range(3):
                v = value(name, count, pattern)
                tn = name if not residue else f"P{residue}{name}"
                val = v if not residue else dict([(f"p{i}", True) for i in range(residue)] + [("values", v)])
                cases.append((name, residue, count, pattern, tn, val))

def wire_for(oracle, name, val):
    if oracle == "pycrate":
        obj = getattr(primary, name)
        obj.set_val(val)
        return obj.to_aper()
    return secondary.encode(name, val)

def decoded(oracle, name, wire):
    if oracle == "pycrate":
        obj = getattr(primary, name)
        obj.from_aper(wire)
        return obj.get_val()
    return secondary.decode(name, wire)

profiles = {}
for oracle in ("pycrate", "asn1tools"):
    wires = [wire_for(oracle, case[4], case[5]) for case in cases]
    requests = [f"B {name} {residue} {count} {pattern} {wire.hex() or '-'}\n"
                for (name, residue, count, pattern, _, _), wire in zip(cases, wires)]
    run = subprocess.run([str(work / "driver")], input="".join(requests), text=True,
                         capture_output=True, check=True)
    lines = run.stdout.splitlines()
    if len(lines) != len(cases):
        raise RuntimeError("Driver case count differs")
    matches, mismatches = [], []
    for index, (line, wire, case) in enumerate(zip(lines, wires, cases)):
        parts = line.split()
        if len(parts) == 2 and parts[1] == "OK" and parts[0] == wire.hex():
            if decoded(oracle, case[4], bytes.fromhex(parts[0])) != case[5]:
                raise RuntimeError((oracle, "decoded field mismatch", index))
            matches.append(index)
        else:
            mismatches.append({"case": index, "type": case[0], "residue": case[1],
                               "octet_count": len(wire), "sha256": hashlib.sha256(wire).hexdigest(),
                               "actual": line})
    profiles[oracle] = {"matched_case_ids": matches, "mismatches": mismatches}

# Separate sender additions must become owned opaque sidecars, not known receiver fields.
unknown = []
for residue in range(8):
    for count in (1, 2):
        for pattern in range(3):
            elements = value("ExtPairs", count, pattern)
            shown = []
            for i, element in enumerate(elements):
                if (i + pattern) % 3:
                    element["extraFlag"] = (i + pattern) % 3 == 2
                    shown.append(f"({element['value']}:1,0=" + ("80" if element["extraFlag"] else "00") + ")")
                else:
                    shown.append(f"({element['value']}:0)")
            tn = "ExtPairs" if not residue else f"P{residue}ExtPairs"
            val = elements if not residue else dict([(f"p{i}", True) for i in range(residue)] + [("values", elements)])
            unknown.append((residue, count, pattern, tn, val, "[" + ",".join(shown) + "]"))
for oracle in ("pycrate", "asn1tools"):
    requests = []
    for residue, count, pattern, name, val, _ in unknown:
        wire = wire_for(oracle, name, val)
        requests.append(f"U ExtPairs {residue} {count} {pattern} {wire.hex()}\n")
    run = subprocess.run([str(work / "driver")], input="".join(requests), text=True,
                         capture_output=True, check=True)
    if run.stdout.splitlines() != [item[5] for item in unknown]:
        raise RuntimeError((oracle, "opaque extension records disagree"))
    profiles[oracle]["owned_unknown_extension_cases"] = len(unknown)

observed = {"cases_each_oracle": len(cases), "profile": profiles}
accepted = json.loads((source / "accepted-profile.json").read_text())
unresolved = int(observed != accepted)
result = {"production_disagreements_unresolved": unresolved, "observed": observed,
          "byte_normalization": False, "versions": {"pycrate": "0.7.11", "asn1tools": "0.167.0"}}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2) + "\n")
if unresolved:
    raise RuntimeError("Exact native oracle profile changed; investigate observed cases before acceptance")
print(json.dumps({"cases_each_oracle": len(cases), "matched_each": {k: len(v["matched_case_ids"]) for k, v in profiles.items()},
                  "owned_unknown_extension_cases_each": len(unknown), "unresolved": unresolved}))
