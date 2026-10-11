#!/usr/bin/env python3
"""Bind exact P9 source, package, SDK, isolated consumer and regression receipts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import zipfile

BASE = "4a7124a4e5314ad14a000abe790a7394f3aefb0c"
REPO = Path(__file__).resolve().parents[2]


def sha(p):
    h = hashlib.sha256()
    with Path(p).open("rb") as f:
        for b in iter(lambda: f.read(1048576), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    w = a.work.resolve()
    isolated = json.loads((w / "p9-isolated.json").read_text())
    assert isolated["status"] == "PASS"
    seal = json.loads((w / "p9-cmake-seal.json").read_text())
    assert seal == dict(build="REJECTED", install="REJECTED", restored="PASS")
    sdk = w / "p9-rrc-install"
    provenance = json.loads((sdk / "share/nrforge-rrc/sdk-provenance.json").read_text())
    for name, h in provenance["source_inputs"].items():
        assert sha(REPO / name) == h, name
    identity = provenance["identity"]
    assert isolated["rrc"]["identity"] == identity
    archive = sha(sdk / "lib/libnrforge_rrc.a")
    assert archive == sha(w / "p9-rrc-sdk/build/libnrforge_rrc.a")
    cmake_receipt = (w / "p9-rrc-sdk/build/sdk-build-receipt.cmake").read_text()
    assert archive in cmake_receipt and identity["fingerprint"] in cmake_receipt
    generated = w / "p9-sdist-clean-build/generated"
    receipt = json.loads((generated / "bindings-receipt.json").read_text())
    for name, h in receipt["generated_sha256"].items():
        assert sha(generated / name) == h, name
    assert len(receipt["generated_sha256"]) == 68
    stage = json.loads((w / "p9-python-stage/staging-receipt.json").read_text())
    assert len(stage) == 12
    for name, h in stage.items():
        assert sha(REPO / "python/rrc" / name) == h, name
    artifacts = {}
    for path in sorted((w / "p9-delivery").iterdir()):
        entry = dict(sha256=sha(path), bytes=path.stat().st_size)
        if path.suffix == ".whl":
            with zipfile.ZipFile(path) as z:
                entry["files"] = {
                    n: hashlib.sha256(z.read(n)).hexdigest()
                    for n in z.namelist()
                    if not n.endswith("/")
                }
                for n, h in entry["files"].items():
                    if n.startswith("nrforge_rrc/"):
                        installed = w / "p9-final-venv/lib/python3.12/site-packages" / n
                        assert sha(installed) == h, n
                assert entry["files"]["nrforge_rrc/conversion-schema.json"] == sha(
                    generated / "conversion-schema.json"
                )
        else:
            with tarfile.open(path) as t:
                members = {
                    m.name.split("/", 1)[1]: t.extractfile(m).read()
                    for m in t
                    if m.isfile()
                }
            assert set(members) == set(stage) | {"staging-receipt.json", "PKG-INFO"}
            for n, h in stage.items():
                assert hashlib.sha256(members[n]).hexdigest() == h, n
            entry["files"] = {
                n: hashlib.sha256(v).hexdigest() for n, v in members.items()
            }
        artifacts[path.name] = entry
    retained = {}
    for path, summary in [
        (
            w
            / "e1-python-delivery/dist/nrforge_e1ap-0.1.0-cp312-cp312-linux_x86_64.whl",
            "tools/e1ap-python/verification-summary.json",
        ),
        (
            w / "e1-regression-wheels/nrforge_f1ap-0.1.0-cp312-cp312-linux_x86_64.whl",
            "tools/f1ap-python/verification-summary.json",
        ),
        (
            w
            / "e1-regression-wheels/NRForge_prj/nrforge_ngap-0.1.0-cp312-cp312-linux_x86_64.whl",
            "python/verification-summary.json",
        ),
    ]:
        bound = json.loads((REPO / summary).read_text())
        accepted = (
            bound["artifact"] if "artifact" in bound else bound["artifacts"][path.name]
        )
        assert sha(path) == accepted["sha256"], path.name
        retained[path.name] = dict(
            sha256=sha(path),
            accepted_receipt=summary,
            accepted_receipt_sha256=sha(REPO / summary),
        )
    regressions = {}
    for name, count in [
        ("libuper", 3),
        ("libaper", 10),
        ("libngap", 3),
        ("libasn1typed", 46),
        ("libasn1parser", 1),
    ]:
        path = w / ("p9-" + name + ".log")
        log = path.read_text()
        assert re.search(r"# TOTAL:\s*" + str(count) + r"\b", log) and re.search(
            r"# PASS:\s*" + str(count) + r"\b", log
        ), name
        assert re.search(r"# FAIL:\s*0\b", log) and re.search(
            r"# ERROR:\s*0\b", log
        ), name
        regressions[name] = dict(passed=count, log_sha256=sha(path))
    logs = {}
    for name, marker in [
        ("p9-rrc-sdk.log", "PASS sealed SDK build/install"),
        ("p9-sdist-wheel.log", "Successfully built"),
        ("p9-generator-tests.log", "Ran 5 tests"),
        ("p9-isolated.log", "PASS isolated installed"),
        ("p9-native-final.log", "PASS"),
    ]:
        path = w / name
        assert marker in path.read_text(), name
        logs[name] = sha(path)
    extension = next(
        (w / "p9-final-venv/lib/python3.12/site-packages/nrforge_rrc").glob(
            "_native*.so"
        )
    )
    dynamic = subprocess.check_output(["readelf", "-d", str(extension)], text=True)
    assert not any(x in dynamic for x in ("RPATH", "RUNPATH", "libnrforge"))
    exports = subprocess.check_output(
        ["nm", "-D", "-C", "--defined-only", str(extension)], text=True
    )
    assert (
        not any(" T nrforge::" in line for line in exports.splitlines())
        and "PyInit__native" in exports
    )
    paths = [
        REPO / "Makefile.am",
        REPO / "docs/rrc-python-sdk-contract-and-closeout.md",
    ]
    paths += [
        p
        for root in ("python/rrc", "tools/rrc-python")
        for p in (REPO / root).rglob("*")
        if p.is_file()
        and "__pycache__" not in p.parts
        and p.name not in ("verification-summary.json", "review.md")
    ]
    source = {str(p.relative_to(REPO)): sha(p) for p in sorted(paths)}
    preserved = 0
    for name in subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASE], cwd=REPO, text=True
    ).splitlines():
        if name == "Makefile.am":
            continue
        path = REPO / name
        content = os.readlink(path).encode() if path.is_symlink() else path.read_bytes()
        assert (
            subprocess.check_output(
                ["git", "hash-object", "--stdin"], cwd=REPO, input=content
            )
            .decode()
            .strip()
            == subprocess.check_output(
                ["git", "rev-parse", BASE + ":" + name], cwd=REPO, text=True
            ).strip()
        ), name
        preserved += 1
    fixture = json.loads((REPO / "python/rrc/tests/golden.json").read_text())
    assert len(fixture["cases"]) == 143 and len(fixture["supplemental"]) == 3
    result = dict(
        batch="RRC-P9",
        baseline_commit=BASE,
        branch="feature/f1ap-cpp-aper",
        status="VERIFIED_PENDING_INDEPENDENT_ACCEPTANCE",
        source_sha256=source,
        preserved_baseline_blobs=preserved,
        only_existing_file_change="Makefile.am distribution entries",
        sdk_identity=identity,
        sdk_archive_sha256=archive,
        sdk_provenance_sha256=sha(sdk / "share/nrforge-rrc/sdk-provenance.json"),
        sdk_source_inputs=provenance["source_inputs"],
        bindings_receipt=receipt,
        artifacts=artifacts,
        staged_source_sha256=stage,
        isolated_consumers=isolated,
        cmake_seal=seal,
        regressions=regressions,
        retained_aper_wheels=retained,
        logs_sha256=logs,
        qualification=dict(
            real_cases=143,
            supplemental_native_boundary_cases=3,
            supported_roots=64,
            blocked_roots=21,
            requested_roots=85,
            full_rrc_coverage=False,
            default_omissions=19,
            malformed_quota_checks=998,
            completion_modes=3,
            import_orders=24,
            qualification_sha256=fixture["qualification_sha256"],
        ),
        delivery_scope="CPython 3.12 Linux x86_64; finite P8 profile, no new wire qualification",
        benchmark_run=False,
        merge=False,
        elf_dynamic_sha256=hashlib.sha256(dynamic.encode()).hexdigest(),
        elf_exports_sha256=hashlib.sha256(exports.encode()).hexdigest(),
    )
    a.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(
        "PASS exact RRC-P9 delivery record",
        len(source),
        "source files",
        preserved,
        "preserved baseline blobs",
    )


if __name__ == "__main__":
    main()
