#!/usr/bin/env python3
"""P11: fresh singleton codecs, frozen RRC matrix and shared APER regression."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate

BASE = "99784032a83ab55c3009800bdb31606eb6031000"
CPP = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wconversion",
       "-Wsign-conversion", "-pedantic-errors"]
LIBS = ["tools/developer_tree.o", "libasn1typed/.libs/libasn1typed_extract.a",
        "libasn1typed/.libs/libasn1typed.a", "libasn1fix/.libs/libasn1fix.a",
        "libasn1parser/.libs/libasn1parser.a", "libasn1common/.libs/libasn1common.a"]
ROOTS = ["Zero", "Four", "Negative", "Minimum", "Maximum", "UnsignedMaximum",
         "Alias", "Fields", "Pick", "Defaulted", "NamedList", "Extensible",
         "TooHigh", "TooLow", "InlineList"]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def status(root):
    return ("BLOCKED_EXTRACTION" if root["extract"] else
            "BLOCKED_RENDER" if root["render"] else "PASS")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    work = a.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    schema = a.asn1_root.resolve()
    authority = json.loads((REPO / "tools/rrc-readiness/source-manifest.json").read_text())
    authenticate(schema, authority)
    accepted = json.loads((REPO / "tools/rrc-python/verification-summary.json").read_text())
    # Capture CURRENT producer inputs. P9's accepted hashes remain immutable;
    # intentionally changed extraction code cannot authenticate against P9.
    paths = set(accepted["sdk_source_inputs"]) | set(LIBS) | {
        "tools/rrc-singleton/check.py", "tools/rrc-singleton/singletons.asn1",
        "tools/rrc-singleton/check_generated.cpp", "libasn1typed/check_asn1typed_extract.c",
        "tools/rrc-uper/probe.c", "tools/rrc-uper/readiness-rrc-p6.json",
        "tools/rrc-readiness/scan.py", "tools/rrc-readiness/source-manifest.json",
        "tools/rrc-readiness/rrc-rel18.modules", "tools/asn1typed_codec_coverage.c",
        "tools/rrc-readiness/check_aper_regression_p4.py"}
    inputs = {n: sha(REPO / n) for n in sorted(paths)}

    def run(command, name):
        with (work / (name + ".log")).open("wb") as log:
            subprocess.run(list(map(str, command)), cwd=REPO, stdout=log,
                           stderr=subprocess.STDOUT, check=True)

    def build(src, dest):
        run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DHAVE_CONFIG_H",
             *["-I" + str(REPO / d) for d in
               (".", "libasn1common", "libasn1parser", "libasn1fix", "libasn1typed", "tools")],
             REPO / src, *[REPO / n for n in LIBS], "-lm", "-o", work / dest], dest + "-build")

    build("tools/rrc-uper/probe.c", "probe")
    build("tools/asn1typed_codec_coverage.c", "aper-probe")

    def probe(directory, modules, source, roots):
        directory.mkdir()
        (directory / "roots.list").write_text("".join(m + " " + n + "\n" for m, n in roots))
        (directory / "headers").mkdir()
        with (directory / "raw.json").open("wb") as out, (directory / "probe.stderr").open("wb") as err:
            subprocess.run([str(work / "probe"), str(modules), str(source),
                            str(directory / "roots.list"), str(directory / "headers")],
                           stdout=out, stderr=err, check=True)
        data = json.loads((directory / "raw.json").read_text())
        assert data["owned_after_tree_destruction"]
        assert [(r["module"], r["root"]) for r in data["roots"]] == roots
        for i, r in enumerate(data["roots"]):
            r["status"] = status(r)
            header = directory / "headers" / f"r{i:03}.hpp"
            if r["status"] == "PASS":
                r["header_sha256"] = sha(header)
            else:
                assert not header.exists()
        return data

    (work / "modules.list").write_text("singletons.asn1\n")
    focused = probe(work / "focused", work / "modules.list", REPO / "tools/rrc-singleton",
                    [("SingletonCodec", n) for n in ROOTS])
    assert [r["status"] for r in focused["roots"]] == ["PASS"] * 11 + ["BLOCKED_EXTRACTION"] * 4
    include = work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    run(["g++", *CPP, "-I" + str(work / "include"),
         "-I" + str(work / "focused/headers"), REPO / "tools/rrc-singleton/check_generated.cpp",
         REPO / "libuper/runtime.cpp", "-o", work / "check"], "generated-build")
    run([work / "check"], "generated-test")
    run([REPO / "libasn1typed/check_asn1typed_extract"], "extract-test")

    prior = json.loads((REPO / "tools/rrc-uper/readiness-rrc-p6.json").read_text())
    ledger = prior["roots"] + prior["default_seed_roots"]
    assert len(ledger) == 85
    matrix = probe(work / "matrix", REPO / "tools/rrc-readiness/rrc-rel18.modules", schema,
                   [(r["module"], r["root"]) for r in ledger])
    transitions = []
    unchanged = 0
    affected = {"DL-CCCH-Message", "RRCReconfiguration", "RRCSetup", "SystemInformation"}
    for i, (old, new) in enumerate(zip(ledger, matrix["roots"], strict=True)):
        new["roles"] = old["roles"]
        if old["status"] == "PASS":
            assert new["status"] == "PASS" and new["header_sha256"] == old["header_sha256"]
            unchanged += 1
        elif new["root"] in affected:
            assert old["status"] == "BLOCKED_EXTRACTION"
            assert new["error"] != old["error"]
            if new["root"] == "SystemInformation":
                assert new["status"] == "PASS"
                unit = work / "system-information.cpp"
                unit.write_text(f'#include "r{i:03}.hpp"\n')
                run(["g++", *CPP, "-I" + str(work / "include"),
                     "-I" + str(work / "matrix/headers"), "-fsyntax-only", unit], "system-information-compile")
                new["strict_cpp20"] = "PASS"
                new["wire_qualification"] = "NOT_RUN"
            else:
                assert new["status"] == "BLOCKED_EXTRACTION"
            transitions.append({"module": new["module"], "root": new["root"],
                                "before": old["status"], "after": new["status"],
                                "previous_error": old["error"], "current_error": new["error"]})
        else:
            assert (new["status"], new["error"]) == (old["status"], old["error"])
    counts = dict(Counter(r["status"] for r in matrix["roots"]))
    assert counts == {"PASS": 65, "BLOCKED_EXTRACTION": 15, "BLOCKED_RENDER": 5}
    assert unchanged == 64 and len(transitions) == 4
    run([sys.executable, REPO / "tools/rrc-readiness/check_aper_regression_p4.py",
         "--asn1-root", schema, "--probe", work / "aper-probe", "--work", work / "aper",
         "--output", work / "aper-raw.json"], "aper-regression")
    aper = json.loads((work / "aper-raw.json").read_text())
    assert sum(p["messages"] for p in aper["profiles"].values()) == 361
    # Historical helper labels its own batch P4; identify the current invocation explicitly.
    aper["helper_batch"] = aper.pop("batch")
    aper["helper_baseline"] = aper.pop("baseline")
    aper.update(batch="RRC-P11", baseline=BASE)
    authenticate(schema, authority)
    assert inputs == {n: sha(REPO / n) for n in inputs}
    report = {"batch": "RRC-P11", "baseline": BASE, "source_authority": authority,
              "producer_inputs_sha256": inputs, "strict_cpp_flags": CPP,
              "focused": focused, "matrix": matrix, "summary": counts,
              "unchanged_supported_headers": unchanged, "transitions": transitions,
              "aper_regression": aper,
              "binary_sha256": {n: sha(work / n) for n in ("probe", "aper-probe", "check")},
              "log_sha256": {f.name: sha(f) for f in sorted(work.glob("*.log"))},
              "sdk_delivered_roots": 64, "benchmark_run": False,
              "limitations": ["SystemInformation has extraction/render and strict compilation only; no new wire qualification",
                              "P8/P9 SDK and qualification receipts remain unchanged; expanded delivery is deferred",
                              "Inline constrained collection elements and extensible singleton roots remain rejected"]}
    a.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"focused_pass": 11, "focused_rejected": 4, "matrix": counts,
                      "old_headers_unchanged": unchanged, "aper": aper["profiles"]}, sort_keys=True))


if __name__ == "__main__":
    main()
