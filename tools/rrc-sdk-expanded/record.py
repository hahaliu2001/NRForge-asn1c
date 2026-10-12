#!/usr/bin/env python3
"""Bind exact installed closure, isolation, seal and regression receipts for P17."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
BASE = "4132fc6c3a574b7a21a1d1aa3989d3a07a713272"


def sha(p):
    h = hashlib.sha256()
    with Path(p).open("rb") as f:
        for b in iter(lambda: f.read(1048576), b""):
            h.update(b)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for k in (
        "sdk-work",
        "schema",
        "consumer-input",
        "prefix",
        "isolated",
        "seal",
        "repeat",
        "output",
    ):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    g = a.sdk_work / "generated"
    manifest = json.loads((g / "manifest.json").read_text())
    provenance = json.loads((g / "sdk-provenance.json").read_text())
    identity = provenance["identity"]
    sys.path.insert(0,str(REPO / "tools/rrc-readiness"))
    from scan import authenticate
    authenticate(a.schema, provenance["source_authority"])
    for name, h in provenance["source_inputs"].items():
        if sha(REPO / name) != h:
            raise ValueError("producer input drift " + name)
    generated = {str(x.relative_to(g)): sha(x) for x in g.rglob("*") if x.is_file()}
    repeated = {
        str(x.relative_to(a.repeat / "generated")): sha(x)
        for x in (a.repeat / "generated").rglob("*")
        if x.is_file()
    }
    if generated != repeated:
        raise ValueError("repeat generation differs")
    preparation = json.loads((a.consumer_input / "preparation.json").read_text())
    for name, h in preparation["bound_inputs_sha256"].items():
        if sha(Path(name)) != h:
            raise ValueError("consumer preparation input drift " + name)
    if sha(a.consumer_input / "native.py") != preparation["native_module_sha256"]:
        raise ValueError("native oracle drift")
    actual_consumers = {p.name:sha(p) for p in (a.consumer_input / "consumer").iterdir() if p.is_file()}
    if actual_consumers != preparation["consumer_files"]:
        raise ValueError("consumer source closure drift")
    commands = json.loads((a.sdk_work / "build/compile_commands.json").read_text())
    if len(commands) != 88:
        raise ValueError("producer TU closure")
    for command in commands:
        for flag in ["-Werror", "-Wconversion", "-Wsign-conversion", "-pedantic-errors", "--param=ggc-min-expand=5", "--param=ggc-min-heapsize=16384"]:
            if flag not in command["command"]:
                raise ValueError("producer flags drift " + flag)
    archive = a.prefix / "lib/libnrforge_rrc.a"
    archive_hash = sha(archive)
    if archive_hash != sha(a.sdk_work / "build/libnrforge_rrc.a"):
        raise ValueError("installed archive drift")
    receipt = (a.sdk_work / "build/sdk-build-receipt.cmake").read_text()
    if identity["fingerprint"] not in receipt or archive_hash not in receipt:
        raise ValueError("archive receipt mismatch")
    supported = [r for r in manifest["roots"] if r["status"] == "PASS"]
    readiness = json.loads((REPO / "tools/rrc-null-open/readiness.json").read_text())["matrix"]["roots"]
    for i, (entry, accepted) in enumerate(zip(manifest["roots"], readiness)):
        if (entry["module"],entry["root"]) != (accepted["module"],accepted["root"]) or sha(a.sdk_work / "raw" / f"r{i:03}.hpp") != accepted["header_sha256"] or entry["qualified_header_sha256"] != accepted["header_sha256"]:
            raise ValueError("P16 raw header preservation drift")
    if len(supported) != 85 or len(manifest["roots"]) != 85:
        raise ValueError("root ledger closure")
    members = subprocess.check_output(["ar","t",str(archive)],text=True).splitlines()
    if members != ["runtime.cpp.o","sdk_identity.cpp.o"] + [f"r{i:03}.cpp.o" for i in range(85)]:
        raise ValueError("archive object inventory closure")
    symbols = subprocess.check_output(
        ["nm", "-C", "--defined-only", str(archive)], text=True
    )
    strong_symbols = {
        line.split(" T ", 1)[1] for line in symbols.splitlines() if " T " in line
    }
    for r in supported:
        for function in ("encode", "decode"):
            if not any(
                symbol.startswith(r["namespace"] + "::" + function + "(")
                for symbol in strong_symbols
            ):
                raise ValueError("public wrapper missing " + r["root"] + function)
    expected = {
        "include/" + str(x.relative_to(g / "include"))
        for x in (g / "include").rglob("*.hpp")
    }
    expected |= {
        "lib/libnrforge_rrc.a",
        "share/nrforge-rrc/manifest.json",
        "share/nrforge-rrc/sdk-provenance.json",
    }
    expected |= {
        "lib/cmake/NRForgeRRC/" + n
        for n in (
            "NRForgeRRCConfig.cmake",
            "NRForgeRRCConfigVersion.cmake",
            "NRForgeRRCTargets.cmake",
            "NRForgeRRCTargets-noconfig.cmake",
        )
    }
    installed = {
        str(x.relative_to(a.prefix)): sha(x) for x in a.prefix.rglob("*") if x.is_file()
    }
    if set(installed) != expected:
        raise ValueError("installed file closure")
    for name in expected:
        if name.startswith("include/") and installed[name] != sha(g / name):
            raise ValueError("public header install drift")
    for name in ("manifest.json", "sdk-provenance.json"):
        if installed["share/nrforge-rrc/" + name] != sha(g / name):
            raise ValueError("installed metadata drift " + name)
    restoration = json.loads((a.isolated / "original-path-restoration.json").read_text())
    if not restoration["restored"] or any(not Path(row["original"]).exists() or Path(row["hidden"]).exists() for row in restoration["paths"]):
        raise ValueError("isolated original path restoration incomplete")
    isolation = json.loads((a.isolated / "verification.json").read_text())
    if (
        isolation["status"] != "PASS"
        or isolation["independent_real_cases"] != 248
    ):
        raise ValueError("isolated consumer gate")
    if isolation["consumer_source_sha256"] != preparation["consumer_files"] or isolation["qualification_sha256"] != preparation["qualification_sha256"]:
        raise ValueError("isolated consumer preparation receipt mismatch")
    if isolation["public_root_executables"] != 85 or isolation["completion_modes"] != ["octets","contained_bits","exact"] or isolation["public_quota_scope"] != ["depth","work_bits","input_octets"]:
        raise ValueError("isolated public contract mismatch")
    expected_names = set(preparation["expected_cases"])
    actual_names = {p.name for p in (a.isolated / "build").glob("root[0-9][0-9][0-9]") if p.is_file()}
    if expected_names != {f"root{i:03}" for i in range(85)} or actual_names != expected_names:
        raise ValueError("isolated root executable closure mismatch")
    consumer_commands = json.loads((a.isolated / "build/compile_commands.json").read_text())
    if len(consumer_commands) != 85 or {Path(c["file"]).stem for c in consumer_commands} != expected_names:
        raise ValueError("isolated public compiler command closure mismatch")
    for command in consumer_commands:
        for flag in ["-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wsign-conversion", "-pedantic-errors", "-std=c++20", "--param=ggc-min-expand=5", "--param=ggc-min-heapsize=16384"]:
            if flag not in command["command"]:
                raise ValueError("isolated public compiler flags drift " + flag)
    consumer_translation_units = len(list((a.isolated / "build").rglob("*.o.d"))) + len(list((a.isolated / "example-build").rglob("*.o.d")))
    if consumer_translation_units != 86:
        raise ValueError("isolated dependency TU closure mismatch")
    public_negative_checks = 0
    for name in sorted(expected_names):
        rows = (a.isolated / (name + ".log")).read_text().splitlines()
        cases = preparation["expected_cases"][name]
        expected_wires = [f'CASE {c["case"]} {c["wire_hex"]} {c["semantic_bits"]}' for c in cases]
        if [row for row in rows if row.startswith("CASE ")] != expected_wires:
            raise ValueError("isolated public wire transcript mismatch " + name)
        expected_negatives = [(c["semantic_bits"] if c["semantic_bits"] <= 256 else 4) + 4 + int(c["semantic_bits"] % 8 != 0) for c in cases]
        actual_negatives = [int(row.split()[1]) for row in rows if row.startswith("NEG ")]
        if actual_negatives != expected_negatives:
            raise ValueError("isolated public negative transcript mismatch " + name)
        public_negative_checks += sum(actual_negatives)
    if isolation["public_negative_checks"] != public_negative_checks or public_negative_checks != 5510:
        raise ValueError("isolated public negative count mismatch")
    if isolation["relocated_files"]["rrc"] != installed:
        raise ValueError("relocated RRC differs from install")
    if set(isolation["relocated_files"]) != {"rrc"}:
        raise ValueError("P17 installed acceptance must contain only RRC")
    seal = json.loads((a.seal / "verification.json").read_text())
    if seal["status"] != "PASS" or len(seal["cases"]) != 10:
        raise ValueError("seal negative gate")
    seal_copy = json.loads((a.seal / "archive-copy-receipt.json").read_text())
    if seal_copy["archive_sha256"] != archive_hash or not seal_copy["restored_after_mutation"]:
        raise ValueError("restored seal archive receipt mismatch")
    seal_cases = {c["case"]: c for c in seal["cases"]}
    expected_cases = {"sealed_baseline","source_mutation","generated_mutation","extra_generated_input","archive_mutation","missing_receipt","missing_generated_input","restored_baseline","missing_encode_wrapper","missing_decode_wrapper"}
    if set(seal_cases) != expected_cases or any(c["status"] != "PASS" or c["expected_accept"] != (name in {"sealed_baseline","restored_baseline"}) for name,c in seal_cases.items()):
        raise ValueError("seal expected case closure mismatch")
    log = REPO / "libuper/test-suite.log"
    for key, count in [("TOTAL",3),("PASS",3),("FAIL",0),("ERROR",0)]:
        if not re.search(r"# " + key + r":\s*" + str(count) + r"\b",log.read_text()):
            raise ValueError("UPER native regression failed")
    checks = [{"scope": "public installed SDK", "status": "PASS", "cases": 248,
               "native_oracle": "authenticated pycrate0.7.11 preparation; unavailable at execution"},
              {"scope":"native UPER regression", "status":"PASS", "passed":3,
               "test_suite_sha256":sha(log), "command_log_sha256":sha(REPO.parent / "p17-libuper.log")}]
    previous = {}
    for record in subprocess.check_output(
        ["git", "-C", str(REPO), "ls-tree", "-r", "-z", BASE]
    ).split(b"\0"):
        if not record:
            continue
        info, path = record.split(b"\t", 1)
        mode, kind, oid = info.decode().split()
        name = path.decode()
        previous[name] = (mode, oid)
    regular = [name for name, (mode, oid) in previous.items() if mode != "120000"]
    observed = dict(
        zip(
            regular,
            subprocess.check_output(
                ["git", "-C", str(REPO), "hash-object", "--stdin-paths"],
                input=("\n".join(str(REPO / name) for name in regular) + "\n").encode(),
            )
            .decode()
            .splitlines(),
        )
    )
    for name, (mode, oid) in previous.items():
        if mode == "120000":
            data = os.readlink(REPO / name).encode()
            observed[name] = hashlib.sha1(
                b"blob " + str(len(data)).encode() + b"\0" + data
            ).hexdigest()
    changed = [name for name, (mode, oid) in previous.items() if observed[name] != oid]
    if changed != ["Makefile.am"]:
        raise ValueError("unexpected baseline source change " + repr(changed))
    files = sorted(
        p
        for folder in ("tools/rrc-sdk-expanded", "tools/rrc-sdk-expanded-consumer")
        for p in (REPO / folder).rglob("*")
        if p.is_file()
        and p not in (REPO / "tools/rrc-sdk-expanded/verification.json", REPO / "tools/rrc-sdk-expanded/review.md")
        and "__pycache__" not in p.parts
    )
    files += [
        REPO / "Makefile.am",
        REPO / "cmake/NrforgeRrcSdkExpanded.cmake",
        REPO / "docs/rrc-cpp-sdk-expanded-p17-closeout.md",
    ]
    sources = {str(p.relative_to(REPO)): sha(p) for p in files}
    actual_branch = subprocess.check_output(["git", "-C", str(REPO), "branch", "--show-current"], text=True).strip()
    actual_head = subprocess.check_output(["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True).strip()
    summary = dict(
        batch="RRC-P17",
        baseline_commit=BASE,
        target_branch="feature/f1ap-cpp-aper",
        checkout_state=dict(head=actual_head, branch=actual_branch or None, detached=not bool(actual_branch)),
        status="PASS",
        full_rrc_wire_coverage=False,
        independent_review_status="PENDING",
        delivery_status="technical acceptance passed; pending independent review and publication",
        source_sha256=sources,
        sdk_identity=identity,
        installed_files=installed,
        manifest=manifest,
        public_archive_wrappers=170,
        producer_inputs=len(provenance["source_inputs"]),
        public_consumer_preparation=preparation,
        preparation_receipt_sha256=sha(a.consumer_input / "preparation.json"),
        consumer_translation_units=consumer_translation_units,
        consumer_compile_commands_sha256=sha(a.isolated / "build/compile_commands.json"),
        producer_compile_commands_sha256=sha(a.sdk_work / "build/compile_commands.json"),
        memory_control="GCC GC expand5/heapsize16384, lld threads1, one build job",
        repeated_generation=dict(
            status="PASS",
            exact_files=len(generated),
            all_generated_bytes_identical=True,
        ),
        isolation={k: v for k, v in isolation.items() if k != "relocated_files"},
        seal=seal,
        seal_logs_sha256={p.name:sha(p) for p in sorted(a.seal.glob("*.log"))},
        seal_archive_copy_receipt=seal_copy,
        checks=checks,
        preserved_baseline=dict(
            unchanged_git_blobs=len(previous) - 1,
            sole_changed_existing_file="Makefile.am: EXTRA_DIST only",
            production_codec_changes=False,
            p16_qualification_sha256=sha(
                REPO / "tools/rrc-wire-expanded/qualification.json"
            ),
        ),
        evidence_sha256={
            str(p.relative_to(a.sdk_work)): sha(p)
            for p in [
                a.sdk_work / "probe-build.log",
                a.sdk_work / "matrix.json",
                a.sdk_work / "configure.log",
                a.sdk_work / "build.log",
                a.sdk_work / "install.log",
                a.sdk_work / "build/sdk-build-receipt.cmake",
            ]
        },
        isolation_receipt_sha256=sha(a.isolated / "verification.json"),
        restoration_journal_sha256=sha(a.isolated / "original-path-restoration.json"),
        seal_receipt_sha256=sha(a.seal / "verification.json"),
        limits=[
            "P16 finite 85-root profile; no blocked selected roots; full RRC coverage not claimed.",
            "RRC-only installed SDK acceptance; four-protocol coexistence belongs to P18.",
            "RRC codec algorithms unchanged; wrappers and packaging only.",
            "No stable cross-toolchain binary ABI/authenticity claim.",
            "P18 Python expansion, benchmark and merge NOT_RUN.",
        ],
        independent_review="Exact canonical snapshot approval recorded in review.md before commit/push",
    )
    a.output.write_text(json.dumps(summary, indent=2) + "\n")
    snapshot = dict(source_sha256=sources, verification_sha256=sha(a.output))
    canonical = json.dumps(snapshot, sort_keys=True, separators=(",", ":"))
    (REPO.parent / "p17-final-snapshot.json").write_text(canonical)
    print(
        json.dumps(
            dict(
                source_files=len(sources),
                snapshot_sha256=hashlib.sha256(canonical.encode()).hexdigest(),
                verification_sha256=sha(a.output),
                installed_files=len(installed),
            )
        )
    )


if __name__ == "__main__":
    main()
