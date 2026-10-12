#!/usr/bin/env python3
"""Exercise the actual shared CMake seal against copied immutable SDK inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("repo", "sdk-work", "work"):
        p.add_argument("--" + k, type=Path, required=True)
    p.add_argument("--discard-archive-copy-after-pass", action="store_true")
    a = p.parse_args()
    a.work.mkdir(parents=True, exist_ok=False)
    work = a.work.resolve()
    original = a.sdk_work.resolve()
    g = work / "generated"
    shutil.copytree(original / "generated", g)
    provenance = json.loads((g / "sdk-provenance.json").read_text())
    source = work / "source"
    for name in provenance["source_inputs"]:
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(a.repo / name, target)
    archive = work / "libnrforge_rrc.a"
    shutil.copy2(original / "build/libnrforge_rrc.a", archive)
    receipt = work / "receipt.cmake"
    shutil.copy2(original / "build/sdk-build-receipt.cmake", receipt)
    command = [
        "cmake",
        "-DNRFORGE_SOURCE_ROOT=" + str(source),
        "-DNRFORGE_GENERATED_ROOT=" + str(g),
        "-DNRFORGE_SDK_ARCHIVE=" + str(archive),
        "-DNRFORGE_SDK_RECEIPT=" + str(receipt),
        "-P",
        str(a.repo.resolve() / "cmake/VerifyNgapSdk.cmake"),
    ]
    cases = []

    def check(name, expected, needle=""):
        r = subprocess.run(command, capture_output=True, text=True)
        text = r.stdout + r.stderr
        (work / (name + ".log")).write_text(text)
        if (r.returncode == 0) != expected or (not expected and needle not in text):
            raise RuntimeError(name + ": unexpected verification result " + text[-500:])
        cases.append(dict(case=name, status="PASS", expected_accept=expected))

    check("sealed_baseline", True)
    for name, path, needle in [
        (
            "source_mutation",
            source / "libuper/runtime.hpp",
            "SOURCE fingerprint mismatch",
        ),
        (
            "generated_mutation",
            g / "include/nrforge/rrc/runtime.hpp",
            "GENERATED fingerprint mismatch",
        ),
    ]:
        raw = path.read_bytes()
        try:
            path.write_bytes(raw + b"\n// mutation\n")
            check(name, False, needle)
        finally:
            path.write_bytes(raw)
    extra = g / "unexpected.hpp"
    try:
        extra.write_text("// unexpected\n")
        check("extra_generated_input", False, "file set differs")
    finally:
        extra.unlink()
    raw = archive.read_bytes()
    try:
        archive.write_bytes(raw + b"X")
        check("archive_mutation", False, "archive differs")
    finally:
        archive.write_bytes(raw)
    saved = receipt.read_bytes()
    try:
        receipt.unlink()
        check("missing_receipt", False, "no successful build receipt")
    finally:
        receipt.write_bytes(saved)
    missing = g / "include/nrforge/rrc/rrc.hpp"
    raw = missing.read_bytes()
    try:
        missing.unlink()
        check("missing_generated_input", False, "file set differs")
    finally:
        missing.write_bytes(raw)
    check("restored_baseline", True)
    manifest = json.loads((g / "manifest.json").read_text())
    entry = next(
        r
        for r in manifest["roots"]
        if r["module"] == "NR-RRC-Definitions" and r["root"] == "MIB"
    )
    index = entry["index"]
    text = (g / f"r{index:03}.cpp").read_text()
    for name in ("encode", "decode"):
        # Deliberately omit the wrapper definition from a copied root object.
        # Link the real volatile anchor against that object and unchanged actual
        # runtime/identity objects, excluding the original root definition.
        lines = [
            line
            for line in text.splitlines()
            if not (
                " " + name + "(" in line and "return " + name + "_unchecked" in line
            )
        ]
        if len(lines) != len(text.splitlines()) - 1:
            raise ValueError("wrapper omission fixture shape")
        source_file = work / (name + ".cpp")
        source_file.write_text("\n".join(lines) + "\n")
        obj = work / (name + ".o")
        subprocess.run(
            [
                "c++",
                "-std=c++20",
                "-I" + str(g / "include"),
                "-c",
                str(source_file),
                "-o",
                str(obj),
            ],
            check=True,
        )
        lib = work / (name + ".a")
        subprocess.run(
            [
                "ar",
                "rcs",
                str(lib),
                str(obj),
                str(original / "build/CMakeFiles/nrforge_rrc.dir/runtime.cpp.o"),
                str(original / "build/CMakeFiles/nrforge_rrc.dir/sdk_identity.cpp.o"),
            ],
            check=True,
        )
        main_file = work / "main.cpp"
        main_file.write_text(
            "namespace nrforge::rrc::detail {void root_anchor_"
            + str(index)
            + "() noexcept;} int main(){nrforge::rrc::detail::root_anchor_"
            + str(index)
            + "();}\n"
        )
        result = subprocess.run(
            ["c++", str(main_file), str(lib), "-o", str(work / name)],
            capture_output=True,
            text=True,
        )
        (work / ("missing_" + name + "_wrapper.log")).write_text(
            result.stdout + result.stderr
        )
        if (
            not result.returncode
            or entry["namespace"] + "::" + name + "(" not in result.stderr
        ):
            raise RuntimeError("missing wrapper not rejected")
        cases.append(
            dict(
                case="missing_" + name + "_wrapper",
                status="PASS",
                expected_accept=False,
            )
        )
    (work / "verification.json").write_text(
        json.dumps(dict(status="PASS", cases=cases), indent=2) + "\n"
    )
    def sha(path):
        h = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1048576), b""):
                h.update(chunk)
        return h.hexdigest()
    archive_hash = sha(archive)
    if archive_hash != sha(original / "build/libnrforge_rrc.a"):
        raise ValueError("dedicated archive not restored after seal checks")
    (work / "archive-copy-receipt.json").write_text(json.dumps(dict(
        archive_sha256=archive_hash, restored_after_mutation=True,
        dedicated_copy_removed_after_pass_to_reclaim_space=a.discard_archive_copy_after_pass
    ), indent=2) + "\n")
    if a.discard_archive_copy_after_pass:
        archive.unlink()
    print("PASS eight seal checks and two public wrapper omission negatives")


if __name__ == "__main__":
    main()
