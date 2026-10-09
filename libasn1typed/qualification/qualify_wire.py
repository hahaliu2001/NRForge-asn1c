#!/usr/bin/env python3
"""Opt-in synthetic APER qualification. No production renderer/runtime imports.

Valid wire: untouched original schema compiled with asn1tools + a bit layout model.
Reversed schema: independent canonical tag ranks; external original wire comparator.
Invalid wire/budgets: NRForge's strict S3 policy, not external decoder acceptance.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

import asn1tools

DEFAULT = (1048576, 1048576, 8388608)
BOUNDARIES = (0, 1, 255, 256, 32767, 32768, 65535)
VERSIONS = {"asn1tools": "0.167.0", "bitstruct": "8.23.0", "pyparsing": "3.3.3"}


def require(condition, message):
    # Deliberately not Python assert: -O must not disable qualification.
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def layout(kind, count, presence, branch, payload):
    """X.691 11.1/11.5/12/19/23: bit spans, not generated mapping traits.

    A span is one atomic field operation (alignment is inside the INTEGER span).
    The canonical alternatives have context tags 0=flag, 1=count in both schemas.
    """
    bits, spans, padding = [], [], []

    def bit(value):
        start = len(bits)
        bits.append(int(bool(value)))
        spans.append((start, len(bits), False))

    def integer(value):
        start = len(bits)
        pad = (-start) % 8
        padding.extend(range(start, start + pad))
        bits.extend([0] * pad)
        bits.extend((value >> shift) & 1 for shift in range(15, -1, -1))
        spans.append((start, len(bits), value > 65535))

    def choice():
        canonical = sorted(((2, 0, "flag"), (2, 1, "count")))
        alternative = "count" if branch else "flag"
        bit([item[2] for item in canonical].index(alternative))
        if branch:
            integer(payload)
        else:
            bit(payload)

    if kind == "C":
        integer(count)
    elif kind == "S":
        choice()
    elif kind == "P":
        bit(presence != 0)  # OPTIONAL bitmap precedes all components.
        integer(count)
        if presence:
            bit(presence == 2)
        choice()
    else:
        raise ValueError(kind)
    field_end = len(bits)
    final_pad = (-field_end) % 8
    padding.extend(range(field_end, field_end + final_pad))
    bits.extend([0] * final_pad)
    spans.append((field_end, len(bits), False))
    wire = bytes(sum(bits[i + j] << (7 - j) for j in range(8))
                 for i in range(0, len(bits), 8))
    return wire, (field_end, final_pad, 0, len(bits), len(wire)), spans, padding


def value(kind, count, presence, branch, payload):
    choice = ("count", payload) if branch else ("flag", bool(payload))
    if kind == "C":
        return count
    if kind == "S":
        return choice
    result = {"count": count, "selection": choice}
    if presence:
        result["enabled"] = presence == 2
    return result


def semantic(ns, kind, count, presence, branch, payload):
    if kind == "C":
        return f"OK {count}"
    # Source-order storage is intentionally different from canonical tag order.
    ordinal = branch if ns == 0 else 1 - branch
    tail = f"{branch} {payload} {ordinal}"
    return f"OK {tail}" if kind == "S" else f"OK {count} {presence} {tail}"


def error_model(command, wire, limits, spans, padding):
    """Strict S3 preflight policy over independent field spans.

    Tests mutate only padding (never value/selector/presence), hence the known
    span layout remains valid. Truncation is detected before examining padding.
    """
    input_limit, output_limit, wire_limit = limits
    if command == "D" and len(wire) > input_limit:
        return "ERR resource_limit 0"
    for start, end, overflow in spans:
        if command == "E" and overflow:
            return f"ERR constraint_violation {start}"
        if command == "D" and end > len(wire) * 8:
            return f"ERR truncated_input {len(wire) * 8}"
        if end > wire_limit or (command == "E" and (end + 7) // 8 > output_limit):
            return f"ERR resource_limit {start}"
        if command == "D":
            for position in padding:
                if start <= position < end and wire[position // 8] & (0x80 >> (position % 8)):
                    return f"ERR nonzero_padding {position}"
    end = spans[-1][1]
    if command == "D" and len(wire) * 8 > end:
        return f"ERR trailing_data {end}"
    return None


def build(root, work, cxx):
    renderer = root / "libasn1typed/check_asn1typed_codec_render"
    require(renderer.exists(), "Build check_asn1typed_codec_render first (see qualification doc)")
    for directory, fixture, module in (
        ("main", "cpp-aper-s1.asn1", "CppAperSlice"),
        ("reversed", "cpp-aper-reversed-tags.asn1", "CppAperReversedTags"),
    ):
        target = work / directory
        target.mkdir(parents=True)
        subprocess.run([str(renderer), str(root / "libasn1typed/fixtures" / fixture),
                        module, "qualification::" + directory, str(target)], check=True,
                       stdout=subprocess.DEVNULL)
    executable = work / "wire_driver"
    subprocess.run(shlex.split(cxx) + ["-std=c++20", "-Wall", "-Wextra", "-Werror",
                   "-pedantic-errors", "-Wconversion", "-Wsign-conversion", "-O2",
                   "-I" + str(root / "libaper"), "-I" + str(work),
                   str(Path(__file__).with_name("wire_driver.cpp")),
                   str(root / "libaper/runtime.cpp"), "-o", str(executable)], check=True)
    return executable


def run(root, work, cxx, quick):
    for package, expected in VERSIONS.items():
        require(importlib.metadata.version(package) == expected, f"Unpinned {package} version")
    fixtures = root / "libasn1typed/fixtures"
    reference = asn1tools.compile_files(str(fixtures / "cpp-aper-s1.asn1"), "per")
    reversed_reference = asn1tools.compile_files(str(fixtures / "cpp-aper-reversed-tags.asn1"), "per")
    # An explicit limitation check, not an expected failure hidden in the corpus.
    discrepancy = {}
    for alternative, payload in (("flag", True), ("count", 255)):
        v = (alternative, payload)
        native = reversed_reference.encode("Selection", v)
        canonical = reference.encode("Selection", v)
        require(native != canonical, "Known external canonical-order limitation changed; re-review oracle")
        discrepancy[alternative] = {"native_reversed": native.hex(), "canonical": canonical.hex()}

    executable = build(root, work, cxx)
    requests, expected, cross_decodes = [], [], []
    counts = {"valid_values": 0, "strict_negative_requests": 0, "budget_exact_requests": 0}
    typename = {"C": "Count", "S": "Selection", "P": "Packet"}

    def add(command, ns, kind, count, presence, branch, payload, wire, limits, answers):
        requests.append(f"{command} {ns} {kind} {count} {presence} {branch} {payload} "
                        f"{wire.hex() or '-'} {' '.join(map(str, limits))}\n")
        expected.extend(answers)

    def encoding_answer(wire, metrics):
        return "ENC " + wire.hex() + " " + " ".join(map(str, metrics))

    def valid(ns, kind, count=0, presence=0, branch=0, payload=0, negatives=False):
        wire, metrics, spans, padding = layout(kind, count, presence, branch, payload)
        v = value(kind, count, presence, branch, payload)
        external = reference.encode(typename[kind], v)
        require(external == wire, f"Independent bit model disagrees with external oracle: {v}")
        cross_decodes.append((len(expected), typename[kind], v))
        add("V", ns, kind, count, presence, branch, payload, external, DEFAULT,
            [encoding_answer(external, metrics), semantic(ns, kind, count, presence, branch, payload)])
        counts["valid_values"] += 1
        if not negatives:
            return

        def negative(command, mutated, limits):
            answer = error_model(command, mutated, limits, spans, padding)
            require(answer is not None, "Negative corpus accidentally contains success")
            add(command, ns, kind, count, presence, branch, payload, mutated, limits, [answer])
            counts["strict_negative_requests"] += 1

        for length in range(len(wire)):
            negative("D", wire[:length], DEFAULT)
        for suffix in (b"\x00", b"\xff", b"\x00\xff"):
            negative("D", wire + suffix, DEFAULT)
        for position in padding:
            mutated = bytearray(wire)
            mutated[position // 8] |= 0x80 >> (position % 8)
            negative("D", bytes(mutated), DEFAULT)
        if padding:
            mutated = bytearray(wire)
            for position in padding:
                mutated[position // 8] |= 0x80 >> (position % 8)
            negative("D", bytes(mutated), DEFAULT)
        for command, budget in (("D", 0), ("E", 1), ("D", 2), ("E", 2)):
            exact = list(DEFAULT)
            exact[budget] = len(wire) if budget < 2 else len(wire) * 8
            answer = semantic(ns, kind, count, presence, branch, payload) if command == "D" else encoding_answer(wire, metrics)
            add(command, ns, kind, count, presence, branch, payload, wire, exact, [answer])
            counts["budget_exact_requests"] += 1
            exact[budget] -= 1
            negative(command, wire, exact)

    for ns in (0, 1):
        for count in BOUNDARIES if quick else range(65536):
            valid(ns, "C", count, negatives=count in BOUNDARIES)
        selections = [(0, 0), (0, 1)] + [(1, n) for n in BOUNDARIES]
        for branch, payload in selections:
            valid(ns, "S", branch=branch, payload=payload, negatives=True)
        for count in BOUNDARIES:
            for presence in range(3):
                for branch, payload in selections:
                    valid(ns, "P", count, presence, branch, payload, negatives=True)
        rng = random.Random(6912021)
        for _ in range(256):
            branch = rng.randrange(2)
            valid(ns, "P", rng.randrange(65536), rng.randrange(3), branch,
                  rng.randrange(65536) if branch else rng.randrange(2))
        for overflow in (65536, 1 << 32, (1 << 64) - 1):
            for kind, count, presence, branch, payload in (
                ("C", overflow, 0, 0, 0), ("S", 0, 0, 1, overflow),
                ("P", overflow, 0, 0, 1), ("P", 1, 0, 1, overflow),
                ("P", 1, 2, 1, overflow),
            ):
                wire, _, spans, padding = layout(kind, count, presence, branch, payload)
                answer = error_model("E", wire, DEFAULT, spans, padding)
                require(answer is not None, "Overflow oracle failed")
                add("E", ns, kind, count, presence, branch, payload, wire, DEFAULT, [answer])
                counts["strict_negative_requests"] += 1

    corpus = "".join(requests)
    result = subprocess.run([str(executable)], input=corpus, text=True, capture_output=True)
    require(result.returncode == 0, f"wire_driver exited {result.returncode}: {result.stderr[:10000]}")
    actual = result.stdout.splitlines()
    require(len(actual) == len(expected), f"Response count {len(actual)} != {len(expected)}")
    for index, (got, wanted) in enumerate(zip(actual, expected)):
        require(got == wanted, f"Response {index}: {got!r} != {wanted!r}")
    # Explicit response offsets identify V encodings, independent of the order
    # of subsequent budget/error requests. Feed actual C++ bytes to the oracle.
    for index, name, v in cross_decodes:
        require(reference.decode(name, bytes.fromhex(actual[index].split()[1])) == v,
                "External decoder disagrees with generated codec value")
    external_decodes = len(cross_decodes)
    require(external_decodes == counts["valid_values"], "External cross-decode coverage incomplete")
    return {
        "schema_version": 1, "result": "PASS", "mode": "quick" if quick else "full",
        "baseline_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "dependencies": VERSIONS, "compiler": subprocess.check_output(shlex.split(cxx) + ["--version"], text=True).splitlines()[0],
        "counts": dict(counts, external_cross_decodes=external_decodes, protocol_requests=len(requests), checked_response_lines=len(actual)),
        "corpus_sha256": hashlib.sha256(corpus.encode()).hexdigest(),
        "sources_sha256": {str(path.relative_to(root)): digest(path) for path in (
            fixtures / "cpp-aper-s1.asn1", fixtures / "cpp-aper-reversed-tags.asn1",
            Path(__file__).resolve(), Path(__file__).with_name("wire_driver.cpp").resolve())},
        "external_native_reversed_discrepancy": discrepancy,
        "scope": "synthetic Count/Selection/Packet only; reversed tag ranks checked by independent model; strict negatives are S3 policy",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--quick", action="store_true", help="boundary Count only; not full qualification")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = args.build_dir.resolve()
    # A failed rerun must never leave an earlier PASS report at the destination.
    args.report.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="nrforge-wire-") as directory:
        report = run(root, Path(directory), args.cxx, args.quick)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report["counts"], sort_keys=True))
    print("PASS synthetic wire qualification; external reversed-tag limitation recorded")


if __name__ == "__main__":
    main()
