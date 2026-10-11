#!/usr/bin/env python3
"""NULL-OPEN: direct NULL known-open lambda strict compilation, frozen RRC matrix and shared APER regression."""
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

BASE = "28ec9073b93605f3053510f4c64d6a2b65f07dc9"
CPP = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wconversion",
       "-Wsign-conversion", "-pedantic-errors"]
LIBS = ["tools/developer_tree.o", "libasn1typed/.libs/libasn1typed_extract.a",
        "libasn1typed/.libs/libasn1typed.a", "libasn1fix/.libs/libasn1fix.a",
        "libasn1parser/.libs/libasn1parser.a", "libasn1common/.libs/libasn1common.a"]


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
        ".gitignore", "Makefile.am", "libasn1typed/Makefile.am",
        "libasn1fix/asn1fix.c", "libasn1fix/asn1fix_constr.c", "libasn1fix/asn1fix_tags.c", "libasn1fix/asn1fix_misc.c",
        "libasn1typed/check_asn1typed_parameters_default.c",
        "libasn1typed/check_asn1typed_parameters_default",
        "libasn1typed/check_asn1typed_null_open",
        "libasn1typed/fixtures/ordinary-parameters-default.asn1",
        "tools/rrc-null-open/contract.md",
        "tools/rrc-inherited-alias/readiness.json",
        "tools/rrc-uper/probe.c", "tools/rrc-uper/readiness-rrc-p6.json",
        "tools/rrc-readiness/scan.py", "tools/rrc-readiness/source-manifest.json",
        "tools/rrc-readiness/rrc-rel18.modules", "tools/asn1typed_codec_coverage.c",
        "tools/rrc-readiness/check_aper_regression_p4.py"}
    paths |= {str(p.relative_to(REPO)) for p in (REPO / "tools/rrc-null-open").iterdir() if p.is_file() and p.name not in {"readiness.json", "verification.json", "review.md"}}
    inputs = {n: sha(REPO / n) for n in sorted(paths)}

    def run(command, name, env=None):
        with (work / (name + ".log")).open("wb") as log:
            subprocess.run(list(map(str, command)), cwd=REPO, stdout=log,
                           stderr=subprocess.STDOUT, check=True, env=env)

    def build(src, dest, extra=()):
        run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DHAVE_CONFIG_H",
             *["-I" + str(REPO / d) for d in
               (".", "libasn1common", "libasn1parser", "libasn1fix", "libasn1typed", "tools")],
             REPO / src, *extra, *[REPO / n for n in LIBS], "-lm", "-o", work / dest], dest + "-build")

    baseline_source = work / "baseline-renderer.c"
    baseline_source.write_bytes(subprocess.check_output(["git", "show", BASE + ":libasn1typed/asn1typed_render_uper.c"], cwd=REPO))
    build("tools/rrc-uper/probe.c", "baseline-probe", [baseline_source])
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

    build("tools/rrc-null-open/check_extract.c", "check-extract")
    run([work / "check-extract"], "focused-extract")
    (work / "modules.list").write_text("null.asn1\n")
    focused = probe(work / "focused", work / "modules.list", REPO / "tools/rrc-null-open",
                    [("NullOpen", n) for n in ("SeqNull", "ChoiceNull", "SeqBool", "ChoiceBool", "SeqNamed", "ChoiceNamed")])
    assert all(r["status"] == "PASS" for r in focused["roots"])
    include = work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    run(["g++", *CPP, "-I" + str(work / "include"), "-I" + str(work / "focused/headers"),
         REPO / "tools/rrc-null-open/check_generated.cpp", REPO / "libuper/runtime.cpp",
         "-o", work / "check"], "generated-build")
    run([work / "check"], "generated-test")
    run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DHAVE_CONFIG_H",
         *["-I" + str(REPO / d) for d in (".", "libasn1common", "libasn1parser", "libasn1fix", "libasn1typed")],
         "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
         REPO / "tools/rrc-null-open/check_extract.c",
         REPO / "libasn1typed/asn1typed_extract.c", REPO / "libasn1typed/asn1typed_render_uper.c",
         *[REPO / n for n in LIBS[2:]], "-lm", "-o", work / "extract-sanitized"], "sanitized-build")
    run([work / "extract-sanitized"], "sanitized-test",
        {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1", "UBSAN_OPTIONS": "halt_on_error=1"})
    prior = json.loads((REPO / "tools/rrc-inherited-alias/readiness.json").read_text())
    ledger = prior["matrix"]["roots"]
    assert len(ledger) == 85
    matrix = probe(work / "matrix", REPO / "tools/rrc-readiness/rrc-rel18.modules", schema,
                   [(r["module"], r["root"]) for r in ledger])
    baseline_dir = work / "baseline-matrix"
    baseline_dir.mkdir(); (baseline_dir / "headers").mkdir()
    with (baseline_dir / "raw.json").open("wb") as out, (baseline_dir / "stderr").open("wb") as err:
        subprocess.run([str(work / "baseline-probe"), str(REPO / "tools/rrc-readiness/rrc-rel18.modules"), str(schema),
                        str(work / "matrix/roots.list"), str(baseline_dir / "headers")], stdout=out, stderr=err, check=True)
    baseline = json.loads((baseline_dir / "raw.json").read_text())
    assert all(not r["extract"] and not r["render"] for r in baseline["roots"])
    transitions = []; unchanged = 0
    for i, (old, new) in enumerate(zip(ledger, matrix["roots"], strict=True)):
        new["roles"] = old["roles"]
        old_file = baseline_dir / "headers" / f"r{i:03}.hpp"
        new_file = work / "matrix/headers" / f"r{i:03}.hpp"
        assert sha(old_file) == old["header_sha256"]
        old_text = old_file.read_text(); new_text = new_file.read_text()
        restored = new_text.replace("Writer& io){\n(void)io;\n", "Writer& io){\n").replace("Reader& io){(void)io;return", "Reader& io){return")
        assert restored == old_text
        for key in ("strict_cpp20", "wire_qualification", "compile_diagnostic"):
            if key in old: new[key] = old[key]
        if new_text == old_text: unchanged += 1
        else:
            assert new["root"] in {"LocationMeasurementIndication", "UL-DCCH-Message"}
            unit = work / (new["root"] + ".cpp"); unit.write_text(f'#include "r{i:03}.hpp"\n')
            run(["g++", *CPP, "-I" + str(work / "include"), "-I" + str(work / "matrix/headers"), "-fsyntax-only", unit], new["root"] + "-compile")
            new["strict_cpp20"] = "PASS"; new.pop("compile_diagnostic", None)
            new["wire_qualification"] = "NOT_RUN"
            transitions.append({"module": new["module"], "root": new["root"], "before": old["strict_cpp20"], "after": "PASS"})
    counts = dict(Counter(r["status"] for r in matrix["roots"]))
    assert unchanged == 83
    assert len(transitions) == 2
    assert counts == {"PASS": 85}
    print(json.dumps({"matrix": counts, "transitions": transitions}), flush=True)
    run([sys.executable, REPO / "tools/rrc-readiness/check_aper_regression_p4.py",
         "--asn1-root", schema, "--probe", work / "aper-probe", "--work", work / "aper",
         "--output", work / "aper-raw.json"], "aper-regression")
    aper = json.loads((work / "aper-raw.json").read_text())
    assert sum(p["messages"] for p in aper["profiles"].values()) == 361
    # Historical helper labels its own batch P4; identify the current invocation explicitly.
    aper["helper_batch"] = aper.pop("batch")
    aper["helper_baseline"] = aper.pop("baseline")
    aper.update(batch="RRC-NULL-OPEN", baseline=BASE)
    authenticate(schema, authority)
    assert inputs == {n: sha(REPO / n) for n in inputs}
    report = {"batch": "RRC-NULL-OPEN", "baseline": BASE, "source_authority": authority,
              "producer_inputs_sha256": inputs, "strict_cpp_flags": CPP,
              "focused": focused, "matrix": matrix, "summary": counts,
              "unchanged_supported_headers": unchanged,
              "new_strict_compile": dict(Counter(r["strict_cpp20"] for r in matrix["roots"] if r["status"] == "PASS" and r["root"] in {t["root"] for t in transitions})), "transitions": transitions,
              "aper_regression": aper,
              "baseline_renderer_sha256": sha(baseline_source), "generated_changes_only_void_io": True,
              "binary_sha256": {n: sha(work / n) for n in ("probe", "aper-probe", "check-extract", "check", "extract-sanitized", "baseline-probe")},
              "log_sha256": {f.name: sha(f) for f in sorted(work.glob("*.log"))},
              "sdk_delivered_roots": 64, "benchmark_run": False,
              "sanitizer": {"address": "PASS", "undefined": "PASS", "scope": "current renderer and extractor directly instrumented", "leak_scan": "NOT_RUN: environment cannot enumerate /proc tasks"},
              "limitations": ["Newly rendered roots have strict compilation only; no new real-message wire qualification",
                              "P8/P9 SDK and qualification receipts remain unchanged; expanded delivery is deferred",
                              "Known explicit SIZE-addition metadata keeps its separate existing renderer block"]}
    a.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"matrix": counts, "old_headers_unchanged": unchanged,
                      "aper": aper["profiles"]}, sort_keys=True))


if __name__ == "__main__":
    main()
