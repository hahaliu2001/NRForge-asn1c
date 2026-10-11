#!/usr/bin/env python3
"""Authenticated P6 generation matrix; blocked closures are explicit evidence."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
READINESS = REPO / "tools/rrc-readiness"
sys.path.insert(0, str(READINESS))
from scan import authenticate


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    manifest = json.loads((READINESS / "source-manifest.json").read_text())
    authenticate(a.asn1_root, manifest)
    p4 = json.loads((READINESS / "readiness-rrc-p4.json").read_text())
    rows = p4["roots"] + p4["default_seed_roots"]
    requested = [(r["module"], r["root"]) for r in rows]
    if len(requested) != 85 or len(set(requested)) != 85:
        raise ValueError("root accounting drift")
    a.work.mkdir(parents=True, exist_ok=False)
    roots = a.work / "roots.list"
    roots.write_text("".join(f"{m}\t{n}\n" for m, n in requested))
    raw = subprocess.check_output(
        [
            str(a.probe.resolve()),
            str(READINESS / "rrc-rel18.modules"),
            str(a.asn1_root.resolve()),
            str(roots.resolve()),
            str(a.work.resolve()),
        ]
    )
    (a.work / "raw.json").write_bytes(raw)
    result = json.loads(raw)
    if (
        not result["owned_after_tree_destruction"]
        or [(r["module"], r["root"]) for r in result["roots"]] != requested
    ):
        raise ValueError("owned root accounting drift")
    include = a.work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    flags = [
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wconversion",
        "-Wsign-conversion",
        "-pedantic-errors",
    ]
    for i, (r, old) in enumerate(zip(result["roots"], rows)):
        r["roles"] = old["roles"]
        r["status"] = (
            "BLOCKED_EXTRACTION"
            if r["extract"]
            else "BLOCKED_RENDER" if r["render"] else "PASS"
        )
        if r["status"] != "PASS":
            if not r["error"] or (a.work / f"r{i:03}.hpp").exists():
                raise ValueError("blocked root leaked output or diagnostic missing")
            continue
        header = a.work / f"r{i:03}.hpp"
        r["header_sha256"] = hashlib.sha256(header.read_bytes()).hexdigest()
        unit = a.work / f"r{i:03}.cpp"
        unit.write_text(f'#include "r{i:03}.hpp"\n')
        done = subprocess.run(
            [
                "c++",
                *flags,
                "-I" + str(include.parent.resolve()),
                "-fsyntax-only",
                str(unit),
            ],
            capture_output=True,
            text=True,
        )
        (a.work / f"r{i:03}.log").write_text(done.stdout + done.stderr)
        if done.returncode:
            raise RuntimeError(
                f'strict compile failed: {r["root"]}: {done.stderr[:1000]}'
            )
        r["strict_cpp20"] = "PASS"
    fixtures = json.loads((READINESS / "fixtures-manifest.json").read_text())[
        "fixtures"
    ]
    for f in fixtures:
        i = requested.index((f["module"], f["asn1_type"]))
        r = result["roots"][i]
        if r["status"] != "PASS":
            r["inherited_bare_fixture"] = "BLOCKED_EXTRACTION"
            continue
        values = ",".join(
            "std::byte{0x" + f["hex"][j : j + 2] + "}"
            for j in range(0, len(f["hex"]), 2)
        )
        unit = a.work / f"bare{i:03}.cpp"
        unit.write_text(
            f"""#include "r{i:03}.hpp"
int main(){{ namespace g=nrforge_generated::r{i:03};std::vector<std::byte>b={{{values}}};auto d=g::decode(b);if(!d||d.value().semantic_bits!={f['semantic_bits']})return 1;auto e=g::encode(d.value().value);return !e||e.value().octets!=b||e.value().semantic_bits!={f['semantic_bits']}; }}
"""
        )
        binary = a.work / f"bare{i:03}"
        subprocess.run(
            [
                "c++",
                *flags,
                "-I" + str(include.parent.resolve()),
                str(unit),
                str(REPO / "libuper/runtime.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary.resolve())], check=True)
        r["inherited_bare_fixture"] = "decode/reencode PASS"
    authenticate(a.asn1_root, manifest)
    selected = result["roots"][:80]
    report = dict(
        batch="RRC-P6",
        baseline="7e63ac6123f621dd3287db22b93de338117be3d5",
        combined_sha256=manifest["combined_sha256"],
        owned_after_tree_destruction=True,
        selected_root_count=80,
        channel_identities=12,
        payload_identities=66,
        bare_seed_identities=4,
        auxiliary_scope="All six frozen modules parsed/fixed; NR-UE-Variables, NR-Sidelink-Preconf and NR-Sidelink-DiscoveryMessage provide closure dependencies, no standalone channel framing claim.",
        strict_flags=flags,
        summary=dict(Counter(r["status"] for r in selected)),
        role_summary={
            role: dict(Counter(r["status"] for r in selected if role in r["roles"]))
            for role in ("channel", "payload", "bare_seed")
        },
        roots=selected,
        default_seed_roots=result["roots"][80:],
        full_rrc_wire_coverage=False,
        next_gate="RRC-P7 independent full-wire qualification; P8 SDK sealing",
    )
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["summary"]))


if __name__ == "__main__":
    main()
