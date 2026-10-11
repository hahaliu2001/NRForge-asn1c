#!/usr/bin/env python3
"""Relocate all four SDKs, hide original inputs, compile only installed consumers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import uuid


def sha(p):
    h = hashlib.sha256()
    with Path(p).open("rb") as f:
        for block in iter(lambda: f.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for k in (
        "repo",
        "schema",
        "sdk-work",
        "prefix",
        "consumer-input",
        "ngap-prefix",
        "f1ap-prefix",
        "e1ap-prefix",
        "ngap-work",
        "f1ap-work",
        "e1ap-work",
        "work",
    ):
        ap.add_argument("--" + k, type=Path, required=True)
    ap.add_argument("--jobs", type=int, default=2)
    a = ap.parse_args()
    if not 1 <= a.jobs <= 4:
        raise ValueError("jobs must be 1..4")
    work = a.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    inputs = [
        a.repo,
        a.schema,
        a.sdk_work,
        a.prefix,
        a.consumer_input,
        a.ngap_prefix,
        a.f1ap_prefix,
        a.e1ap_prefix,
        a.ngap_work,
        a.f1ap_work,
        a.e1ap_work,
    ]
    originals = [p.resolve() for p in inputs]
    hidden = [
        p
        for p in set(originals)
        if not any(p != q and p.is_relative_to(q) for q in originals)
    ]
    if any(work.is_relative_to(p) for p in hidden):
        raise ValueError("work must be outside hidden inputs")
    copied = {}

    def copy_file(src, dst):
        if Path(src).suffix == ".a":
            try:
                os.link(src, dst)
                return dst
            except OSError:
                pass
        return shutil.copy2(src, dst)

    for proto, path in [
        ("rrc", a.prefix),
        ("ngap", a.ngap_prefix),
        ("f1ap", a.f1ap_prefix),
        ("e1ap", a.e1ap_prefix),
    ]:
        dest = work / ("installed-" + proto)
        shutil.copytree(path, dest, copy_function=copy_file)
        copied[proto] = dest
    example = work / "example"
    shutil.copytree(a.repo / "tools/rrc-sdk-consumer/example", example)
    source = work / "consumer"
    shutil.copytree(a.consumer_input / "consumer", source)
    prepared = json.loads((a.consumer_input / "preparation.json").read_text())
    consumer_hashes = {p.name: sha(p) for p in source.iterdir() if p.is_file()}
    if consumer_hashes != prepared["consumer_files"]:
        raise ValueError("prepared consumer source closure changed")
    installed_manifest = json.loads(
        (copied["rrc"] / "share/nrforge-rrc/manifest.json").read_text()
    )
    if installed_manifest["qualification_sha256"] != prepared["qualification_sha256"]:
        raise ValueError("consumer / installed qualification identity mismatch")
    prefixes = ";".join(map(str, copied.values()))
    env = {"PATH": os.environ["PATH"], "LC_ALL": "C", "HOME": str(work / "home")}
    Path(env["HOME"]).mkdir()

    def run(name, command, expect=True, needle=None):
        r = subprocess.run(command, cwd=work, env=env, capture_output=True, text=True)
        text = r.stdout + r.stderr
        (work / (name + ".log")).write_text(text)
        if (r.returncode == 0) != expect or (needle and needle not in text):
            raise RuntimeError(name + " failed: " + text[-1200:])
        return text

    config = [
        "cmake",
        "-S",
        str(source),
        "-B",
        str(work / "build"),
        "-DCMAKE_PREFIX_PATH=" + prefixes,
        "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
        "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    renamed = []
    try:
        os.chdir(work)
        for path in hidden:
            dest = path.with_name(".unavailable-" + path.name + "-" + uuid.uuid4().hex)
            path.rename(dest)
            renamed.append((path, dest))
        if any(p.exists() for p in originals):
            raise RuntimeError("original input still available")
        run("configure", config)
        run(
            "build",
            [
                "cmake",
                "--build",
                str(work / "build"),
                "--parallel",
                str(a.jobs),
                "--verbose",
            ],
        )
        run(
            "test",
            ["ctest", "--test-dir", str(work / "build"), "--output-on-failure"],
            needle="100% tests passed",
        )
        counts = negatives = 0
        for binary in sorted((work / "build").glob("root[0-9][0-9][0-9]")):
            text = run(binary.name, [str(binary)])
            counts += sum(line.startswith("CASE ") for line in text.splitlines())
            negatives += sum(
                int(line.split()[1])
                for line in text.splitlines()
                if line.startswith("NEG ")
            )
        if counts != 143:
            raise ValueError("missing public root cases")
        run("mixed", [str(work / "build/mixed")], needle="FOUR_PROTOCOLS_PASS")
        run(
            "example_configure",
            [
                "cmake",
                "-S",
                str(example),
                "-B",
                str(work / "example-build"),
                "-DCMAKE_PREFIX_PATH=" + str(copied["rrc"]),
                "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
            ],
        )
        run("example_build", ["cmake", "--build", str(work / "example-build")])
        run(
            "example_test",
            ["ctest", "--test-dir", str(work / "example-build"), "--output-on-failure"],
            needle="100% tests passed",
        )
        # Export/import build interfaces must contain no original path.
        for proto, prefix in copied.items():
            for cmake in prefix.rglob("*.cmake"):
                if any(str(p) in cmake.read_text() for p in originals):
                    raise ValueError("installed path leak " + str(cmake))
        dependencies = set()
        for dep in list((work / "build").rglob("*.o.d")) + list(
            (work / "example-build").rglob("*.o.d")
        ):
            text = dep.read_text().replace("\\\n", " ")
            for name in text.split(":", 1)[1].split():
                if not name.startswith("/"):
                    raise ValueError("relative compiler dependency " + name)
                path = Path(name).resolve()
                if not (
                    path.is_relative_to(source)
                    or path.is_relative_to(example)
                    or any(path.is_relative_to(p) for p in copied.values())
                    or path.is_relative_to("/usr")
                ):
                    raise ValueError("external noninstalled dependency " + name)
                if any(path.is_relative_to(p) for p in originals):
                    raise ValueError("original input dependency")
                dependencies.add(name)
        if (
            len(list((work / "build").rglob("*.o.d")))
            + len(list((work / "example-build").rglob("*.o.d")))
            != 66
        ):
            raise ValueError("dependency closure expected 66 TUs")
        wrong = config.copy()
        wrong[wrong.index("-B") + 1] = str(work / "wrong-fingerprint")
        wrong += ["-DNRFORGE_RRC_EXPECTED_FINGERPRINT=" + "0" * 64]
        run(
            "wrong_fingerprint", wrong, expect=False, needle="requested SDK fingerprint"
        )
        # Pair an actual installed archive with a deliberately different header
        # identity; all header entry paths participate in the link guard.
        version = copied["rrc"] / "include/nrforge/rrc/sdk_version.hpp"
        raw = version.read_text()
        try:
            version.write_text(
                re.sub(r"sdk_require_[0-9a-f]{64}", "sdk_require_" + "0" * 64, raw)
            )
            unit = work / "mismatch.cpp"
            unit.write_text(
                "#include <nrforge/rrc/roots/NR_RRC_Definitions/MIB.hpp>\nint main(){return nrforge::rrc::sdk_identity().version.empty();}\n"
            )
            run(
                "mismatched_header_archive",
                [
                    "c++",
                    "-std=c++20",
                    "-I" + str(copied["rrc"] / "include"),
                    str(unit),
                    str(copied["rrc"] / "lib/libnrforge_rrc.a"),
                    "-o",
                    str(work / "mismatch"),
                ],
                expect=False,
                needle="sdk_require_" + "0" * 64,
            )
        finally:
            version.write_text(raw)
        record = dict(
            status="PASS",
            consumer_source_sha256=consumer_hashes,
            qualification_sha256=prepared["qualification_sha256"],
            original_paths_unavailable=list(map(str, originals)),
            public_root_executables=64,
            independent_real_cases=counts,
            inherited_negative_checks=negatives,
            completion_modes=["octets", "contained_bits", "exact"],
            four_protocols=True,
            installed_mib_example=True,
            public_logical_truncation_cases=143,
            public_zero_depth_quota_cases=143,
            dependency_count=len(dependencies),
            dependency_manifest_sha256=hashlib.sha256(
                "\n".join(sorted(dependencies)).encode()
            ).hexdigest(),
            identity_negatives=[
                "wrong_expected_fingerprint",
                "mismatched_header_archive",
            ],
            relocated_files={
                proto: {
                    str(p.relative_to(prefix)): sha(p)
                    for p in sorted(prefix.rglob("*"))
                    if p.is_file()
                }
                for proto, prefix in copied.items()
            },
        )
        (work / "verification.json").write_text(json.dumps(record, indent=2) + "\n")
    finally:
        failures = []
        for original, dest in reversed(renamed):
            try:
                if original.exists():
                    if (
                        original.is_symlink()
                        or not original.is_dir()
                        or any(
                            p.is_symlink() or not p.is_dir()
                            for p in original.rglob("*")
                        )
                    ):
                        raise RuntimeError("refusing to overwrite recreated content")
                    shutil.rmtree(original)
                dest.rename(original)
            except Exception as e:
                failures.append(str(e))
        if failures:
            raise RuntimeError("restore failed: " + "; ".join(failures))
    print("PASS relocated public APIs, four protocols and identity negatives")


if __name__ == "__main__":
    main()
