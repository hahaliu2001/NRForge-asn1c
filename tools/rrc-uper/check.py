#!/usr/bin/env python3
"""Rebuild P6 probe and synthetic generated codecs, without benchmarks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[2]
FLAGS = [
    "-std=c++20",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wconversion",
    "-Wsign-conversion",
    "-pedantic-errors",
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--sanitize", action="store_true")
    a = p.parse_args()
    a.work.mkdir(parents=True, exist_ok=False)
    work = a.work.resolve()
    subprocess.run(
        ["make", "-C", "libasn1typed", "check_asn1typed_uper"], cwd=REPO, check=True
    )
    subprocess.run(
        [str(REPO / "libasn1typed/check_asn1typed_uper")], cwd=REPO, check=True
    )
    probe = work / "probe"
    subprocess.run(
        [
            "gcc",
            "-std=gnu11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DHAVE_CONFIG_H",
            *[
                "-I" + str(REPO / d)
                for d in (
                    ".",
                    "libasn1common",
                    "libasn1parser",
                    "libasn1fix",
                    "libasn1typed",
                    "tools",
                )
            ],
            str(REPO / "tools/rrc-uper/probe.c"),
            str(REPO / "tools/developer_tree.o"),
            *[
                str(REPO / d)
                for d in (
                    "libasn1typed/.libs/libasn1typed_extract.a",
                    "libasn1typed/.libs/libasn1typed.a",
                    "libasn1fix/.libs/libasn1fix.a",
                    "libasn1parser/.libs/libasn1parser.a",
                    "libasn1common/.libs/libasn1common.a",
                )
            ],
            "-lm",
            "-o",
            str(probe),
        ],
        check=True,
    )
    (work / "modules.list").write_text("compound.asn1\n")
    (work / "roots.list").write_text("UperFixture Root\n")
    raw = subprocess.check_output(
        [
            str(probe),
            str(work / "modules.list"),
            str(REPO / "tools/rrc-uper"),
            str(work / "roots.list"),
            str(work),
        ]
    )
    data = json.loads(raw)
    assert data["owned_after_tree_destruction"] and data["roots"][0]["render"] == 0
    (work / "raw.json").write_bytes(raw)
    include = work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    flags = FLAGS + (
        ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
        if a.sanitize
        else []
    )
    for name, source in [
        ("compound", "libuper/check_compound.cpp"),
        ("generated", "tools/rrc-uper/check_generated.cpp"),
    ]:
        exe = work / name
        subprocess.run(
            [
                "c++",
                *flags,
                "-I" + str(include.parent),
                "-I" + str(work),
                str(REPO / "libuper/runtime.cpp"),
                str(REPO / source),
                "-o",
                str(exe),
            ],
            check=True,
        )
        result = subprocess.check_output([str(exe)], text=True)
        (work / (name + ".log")).write_text(result)
        print(result, end="")
    (work / "result.json").write_text(
        json.dumps(
            dict(
                status="PASS",
                strict_cpp20=True,
                sanitized=a.sanitize,
                generated_header_sha256=hashlib.sha256(
                    (work / "r000.hpp").read_bytes()
                ).hexdigest(),
                scope="Synthetic compound behavior and inherited P4 mapping; independent full RRC wire is P7.",
            ),
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
