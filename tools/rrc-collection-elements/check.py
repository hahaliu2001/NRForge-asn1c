#!/usr/bin/env python3
"""P12: fresh constrained collection codecs, frozen RRC matrix and shared APER regression."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate

BASE = "82ff13ff3172640887c6c847a258e1b8a2a07d13"
CPP = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wconversion",
       "-Wsign-conversion", "-pedantic-errors"]
LIBS = ["tools/developer_tree.o", "libasn1typed/.libs/libasn1typed_extract.a",
        "libasn1typed/.libs/libasn1typed.a", "libasn1fix/.libs/libasn1fix.a",
        "libasn1parser/.libs/libasn1parser.a", "libasn1common/.libs/libasn1common.a"]
ROOTS = ["Integers", "Singletons", "Bits", "BoundedBits", "Fields", "Pick",
         "Nested", "Grouped", "Wide", "Union", "ExtInts", "BadExtSingleton",
         "BadOverflow", "BadExcept", "BadNamedBits", "BadNamedUse", "BadOctets"]


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
        "tools/rrc-collection-elements/check.py", "tools/rrc-collection-elements/collections.asn1",
        "tools/rrc-collection-elements/check_generated.cpp", "libasn1typed/check_asn1typed_extract.c",
        "libasn1typed/check_asn1typed_collection_elements.c",
        "libasn1typed/check_asn1typed_collection_elements",
        "libasn1typed/check_asn1typed_extract",
        "tools/rrc-singleton/readiness.json",
        "tools/rrc-uper/probe.c", "tools/rrc-uper/readiness-rrc-p6.json",
        "tools/rrc-readiness/scan.py", "tools/rrc-readiness/source-manifest.json",
        "tools/rrc-readiness/rrc-rel18.modules", "tools/asn1typed_codec_coverage.c",
        "tools/rrc-readiness/check_aper_regression_p4.py"}
    inputs = {n: sha(REPO / n) for n in sorted(paths)}

    def run(command, name, env=None):
        with (work / (name + ".log")).open("wb") as log:
            subprocess.run(list(map(str, command)), cwd=REPO, stdout=log,
                           stderr=subprocess.STDOUT, check=True, env=env)

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

    (work / "modules.list").write_text("collections.asn1\n")
    focused = probe(work / "focused", work / "modules.list", REPO / "tools/rrc-collection-elements",
                    [("CollectionCodec", n) for n in ROOTS])
    assert [r["status"] for r in focused["roots"]] == (["PASS"] * 9 +
        ["BLOCKED_RENDER", "PASS"] + ["BLOCKED_EXTRACTION"] * 6)
    assert focused["roots"][9]["error"] == "unsupported type constraints/IOC"
    include = work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    run(["g++", *CPP, "-I" + str(work / "include"),
         "-I" + str(work / "focused/headers"), REPO / "tools/rrc-collection-elements/check_generated.cpp",
         REPO / "libuper/runtime.cpp", "-o", work / "check"], "generated-build")
    run([work / "check"], "generated-test")
    run([REPO / "libasn1typed/check_asn1typed_extract"], "extract-test")
    run([REPO / "libasn1typed/check_asn1typed_collection_elements"], "elements-test")
    # Direct instrumented extraction source validates new heap ownership/rollback.
    run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DHAVE_CONFIG_H",
         '-DCOLLECTION_ELEMENT_FIXTURE="' + str(REPO / "tools/rrc-collection-elements/collections.asn1") + '"',
         *["-I" + str(REPO / d) for d in
           (".", "libasn1common", "libasn1parser", "libasn1fix", "libasn1typed")],
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
         REPO / "libasn1typed/check_asn1typed_collection_elements.c",
         REPO / "libasn1typed/asn1typed_extract.c", *[REPO / n for n in LIBS[2:]],
         "-Wl,--wrap=malloc", "-Wl,--wrap=calloc", "-Wl,--wrap=realloc", "-lm",
         "-o", work / "elements-sanitized"], "sanitized-build")
    run([work / "elements-sanitized"], "sanitized-test",
        {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
         "UBSAN_OPTIONS": "halt_on_error=1"})

    prior = json.loads((REPO / "tools/rrc-singleton/readiness.json").read_text())
    ledger = prior["matrix"]["roots"]
    assert len(ledger) == 85
    matrix = probe(work / "matrix", REPO / "tools/rrc-readiness/rrc-rel18.modules", schema,
                   [(r["module"], r["root"]) for r in ledger])
    transitions = []
    unchanged = 0
    affected = {"BCCH-DL-SCH-Message", "DL-CCCH-Message", "LoggedMeasurementConfiguration-r16",
                "RRCSetup", "SIB1", "SidelinkUEInformationNR-r16", "UEAssistanceInformation"}
    for i, (old, new) in enumerate(zip(ledger, matrix["roots"], strict=True)):
        new["roles"] = old["roles"]
        if old["status"] == "PASS":
            assert new["status"] == "PASS" and new["header_sha256"] == old["header_sha256"]
            unchanged += 1
        elif new["root"] in affected:
            assert old["status"] == "BLOCKED_EXTRACTION"
            assert new["error"] != old["error"]
            if new["root"] in {"SidelinkUEInformationNR-r16", "UEAssistanceInformation"}:
                assert new["status"] == "PASS"
                unit = work / (new["root"] + ".cpp")
                unit.write_text(f'#include "r{i:03}.hpp"\n')
                run(["g++", *CPP, "-I" + str(work / "include"),
                     "-I" + str(work / "matrix/headers"), "-fsyntax-only", unit], new["root"] + "-compile")
                new["strict_cpp20"] = "PASS"
                new["wire_qualification"] = "NOT_RUN"
            else:
                assert new["status"] == "BLOCKED_RENDER"
                assert new["error"] == "ordinary CHOICE root tags unavailable"
            transitions.append({"module": new["module"], "root": new["root"],
                                "before": old["status"], "after": new["status"],
                                "previous_error": old["error"], "current_error": new["error"]})
        else:
            assert (new["status"], new["error"]) == (old["status"], old["error"])
    counts = dict(Counter(r["status"] for r in matrix["roots"]))
    assert counts == {"PASS": 67, "BLOCKED_EXTRACTION": 8, "BLOCKED_RENDER": 10}
    assert unchanged == 65 and len(transitions) == 7
    run([sys.executable, REPO / "tools/rrc-readiness/check_aper_regression_p4.py",
         "--asn1-root", schema, "--probe", work / "aper-probe", "--work", work / "aper",
         "--output", work / "aper-raw.json"], "aper-regression")
    aper = json.loads((work / "aper-raw.json").read_text())
    assert sum(p["messages"] for p in aper["profiles"].values()) == 361
    # Historical helper labels its own batch P4; identify the current invocation explicitly.
    aper["helper_batch"] = aper.pop("batch")
    aper["helper_baseline"] = aper.pop("baseline")
    aper.update(batch="RRC-P12", baseline=BASE)
    authenticate(schema, authority)
    assert inputs == {n: sha(REPO / n) for n in inputs}
    report = {"batch": "RRC-P12", "baseline": BASE, "source_authority": authority,
              "producer_inputs_sha256": inputs, "strict_cpp_flags": CPP,
              "focused": focused, "matrix": matrix, "summary": counts,
              "unchanged_supported_headers": unchanged, "transitions": transitions,
              "aper_regression": aper,
              "binary_sha256": {n: sha(work / n) for n in ("probe", "aper-probe", "check", "elements-sanitized")},
              "log_sha256": {f.name: sha(f) for f in sorted(work.glob("*.log"))},
              "sdk_delivered_roots": 64, "benchmark_run": False,
              "sanitizer": {"address": "PASS", "undefined": "PASS",
                            "scope": "current extractor and element tests directly instrumented",
                            "leak_scan": "NOT_RUN: environment cannot enumerate /proc tasks"},
              "limitations": ["Two newly rendered roots have strict compilation only; no new real-message wire qualification",
                              "P8/P9 SDK and qualification receipts remain unchanged; expanded delivery is deferred",
                              "Constrained named uses, named bits and other primitive element families remain rejected",
                              "INTEGER union element ownership passes but the existing UPER renderer rejects it",
                              "Nested list coverage uses a constructed element wrapper; parser grammar is unchanged"]}
    a.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"focused_extract_pass": 11, "focused_render_pass": 10,
                      "focused_render_blocked": 1, "focused_rejected": 6, "matrix": counts,
                      "old_headers_unchanged": unchanged, "aper": aper["profiles"]}, sort_keys=True))


if __name__ == "__main__":
    main()
