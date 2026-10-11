#!/usr/bin/env python3
"""Bind exact installed closure, isolation, seal and regression receipts for P8."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

REPO = Path(__file__).resolve().parents[2]
BASE = "0bc5b860ffc11378f88dbf5a0bef07b1d0c8f908"


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
        "prefix",
        "isolated",
        "seal",
        "repeat",
        "ngap-work",
        "f1ap-work",
        "e1ap-prefix",
        "output",
    ):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    g = a.sdk_work / "generated"
    manifest = json.loads((g / "manifest.json").read_text())
    provenance = json.loads((g / "sdk-provenance.json").read_text())
    identity = provenance["identity"]
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
    archive = a.prefix / "lib/libnrforge_rrc.a"
    archive_hash = sha(archive)
    if archive_hash != sha(a.sdk_work / "build/libnrforge_rrc.a"):
        raise ValueError("installed archive drift")
    receipt = (a.sdk_work / "build/sdk-build-receipt.cmake").read_text()
    if identity["fingerprint"] not in receipt or archive_hash not in receipt:
        raise ValueError("archive receipt mismatch")
    supported = [r for r in manifest["roots"] if r["status"] == "PASS"]
    if len(supported) != 64 or len(manifest["roots"]) != 85:
        raise ValueError("root ledger closure")
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
    isolation = json.loads((a.isolated / "verification.json").read_text())
    if (
        isolation["status"] != "PASS"
        or isolation["independent_real_cases"] != 143
        or not isolation["four_protocols"]
    ):
        raise ValueError("isolated consumer gate")
    if isolation["relocated_files"]["rrc"] != installed:
        raise ValueError("relocated RRC differs from install")
    for proto, work in (("ngap", a.ngap_work), ("f1ap", a.f1ap_work)):
        relocated = isolation["relocated_files"][proto]
        name = "lib/libnrforge_" + proto + ".a"
        if relocated[name] != sha(work / "build" / Path(name).name):
            raise ValueError("relocated archive differs from fresh build " + proto)
        for metadata in ("sdk-provenance.json",):
            name = "share/nrforge-" + proto + "/" + metadata
            if relocated[name] != sha(work / "generated" / metadata):
                raise ValueError("relocated metadata differs from fresh build " + proto)
    for name, h in isolation["relocated_files"]["e1ap"].items():
        if sha(a.e1ap_prefix / name) != h:
            raise ValueError("relocated E1AP differs from accepted install " + name)
    seal = json.loads((a.seal / "verification.json").read_text())
    if seal["status"] != "PASS" or len(seal["cases"]) != 10:
        raise ValueError("seal negative gate")
    checks = []
    for directory, n in [
        ("libuper", 3),
        ("libaper", 10),
        ("libngap", 3),
        ("libasn1typed", 46),
        ("libasn1parser", 1),
    ]:
        log = REPO / directory / "test-suite.log"
        text = log.read_text()
        for key, count in [("TOTAL", n), ("PASS", n), ("FAIL", 0), ("ERROR", 0)]:
            if not re.search(r"# " + key + r":\s*" + str(count) + r"\b", text):
                raise ValueError("regression suite " + directory)
        checks.append(
            dict(
                command="make -j4 -C " + directory + " check",
                total=n,
                passed=n,
                status="PASS",
                test_suite_sha256=sha(log),
                command_log_sha256=sha(REPO.parent / ("p8-" + directory + ".log")),
            )
        )
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
        for folder in ("tools/rrc-sdk", "tools/rrc-sdk-consumer")
        for p in (REPO / folder).rglob("*")
        if p.is_file()
        and p.name not in ("verification-summary.json", "review.md")
        and "__pycache__" not in p.parts
    )
    files += [
        REPO / "Makefile.am",
        REPO / "cmake/NrforgeRrcSdk.cmake",
        REPO / "docs/rrc-cpp-sdk-contract.md",
        REPO / "docs/rrc-cpp-sdk-closeout.md",
    ]
    sources = {str(p.relative_to(REPO)): sha(p) for p in files}
    summary = dict(
        batch="RRC-P8",
        baseline_commit=BASE,
        branch="feature/f1ap-cpp-aper",
        status="PASS",
        full_rrc_wire_coverage=False,
        source_sha256=sources,
        sdk_identity=identity,
        installed_files=installed,
        manifest=manifest,
        public_archive_wrappers=128,
        producer_inputs=len(provenance["source_inputs"]),
        repeated_generation=dict(
            status="PASS",
            exact_files=len(generated),
            all_generated_bytes_identical=True,
        ),
        isolation={k: v for k, v in isolation.items() if k != "relocated_files"},
        relocated_other_protocols={
            proto: dict(
                files=len(mapping),
                closure_sha256=hashlib.sha256(
                    json.dumps(mapping, sort_keys=True, separators=(",", ":")).encode()
                ).hexdigest(),
            )
            for proto, mapping in isolation["relocated_files"].items()
            if proto != "rrc"
        },
        seal=seal,
        checks=checks,
        protocol_builds=[
            dict(
                protocol=proto,
                mode="fresh full installed SDK",
                registry_messages=n,
                archive_sha256=sha(work / ("build/libnrforge_" + proto + ".a")),
                compile_commands_sha256=sha(work / "build/compile_commands.json"),
                build_log_sha256=sha(
                    work / ("build-final.log" if proto == "f1ap" else "build.log")
                ),
                memory_control=(
                    "GCC GC flags on UEContextModificationRequest only; single build job"
                    if proto == "f1ap"
                    else "two build jobs"
                ),
            )
            for proto, work, n in [
                ("ngap", a.ngap_work, 131),
                ("f1ap", a.f1ap_work, 158),
            ]
        ],
        e1ap=dict(
            mode="reused accepted E1-P5 installed SDK, not rebuilt",
            registry_messages=72,
            provenance_sha256=sha(
                a.e1ap_prefix / "share/nrforge-e1ap/sdk-provenance.json"
            ),
            archive_sha256=sha(a.e1ap_prefix / "lib/libnrforge_e1ap.a"),
        ),
        preserved_baseline=dict(
            unchanged_git_blobs=len(previous) - 1,
            sole_changed_existing_file="Makefile.am: EXTRA_DIST only",
            production_codec_changes=False,
            p7_qualification_sha256=sha(
                REPO / "tools/rrc-wire-qualification/qualification.json"
            ),
        ),
        evidence_sha256={
            str(p.relative_to(a.sdk_work)): sha(p)
            for p in [
                a.sdk_work / "focused.log",
                a.sdk_work / "matrix.json",
                a.sdk_work / "configure.log",
                a.sdk_work / "build.log",
                a.sdk_work / "install.log",
                a.sdk_work / "build/sdk-build-receipt.cmake",
            ]
        },
        isolation_receipt_sha256=sha(a.isolated / "verification.json"),
        seal_receipt_sha256=sha(a.seal / "verification.json"),
        limits=[
            "P7 finite value/reference limitations retained; SIB1 blocked; 21 roots unsupported.",
            "No new 361-message APER wire campaign; coexistence uses three accepted populated setup wires plus MIB.",
            "RRC codec algorithms unchanged; wrappers and packaging only.",
            "No stable cross-toolchain binary ABI/authenticity claim.",
            "P9 Python SDK, benchmark and merge NOT_RUN.",
        ],
        independent_review="Exact canonical snapshot approval recorded in review.md before commit/push",
    )
    a.output.write_text(json.dumps(summary, indent=2) + "\n")
    snapshot = dict(source_sha256=sources, verification_sha256=sha(a.output))
    canonical = json.dumps(snapshot, sort_keys=True, separators=(",", ":"))
    (REPO.parent / "p8-final-snapshot.json").write_text(canonical)
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
