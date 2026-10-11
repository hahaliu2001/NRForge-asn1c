#!/usr/bin/env python3
"""Resolve the P6 explicit-tag gate without altering frozen schemas/oracles."""
import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--generated", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    assert version("asn1tools") == "0.167.0" and version("pycrate") == "0.7.11"
    a.work.mkdir(parents=True, exist_ok=False)
    a.work = a.work.resolve()
    observation = a.work / "observation.json"
    subprocess.run(
        [
            sys.executable,
            str(REPO / "tools/rrc-uper/observe_oracles.py"),
            "--work",
            str(a.work / "oracles"),
            "--output",
            str(observation),
        ],
        check=True,
    )
    obs = json.loads(observation.read_text())
    assert [(r["case"], r["byte_match"]) for r in obs["cases"]] == [
        ("minimal", True),
        ("group", True),
        ("choice_root", False),
        ("choice_extension", True),
        ("root_extension", True),
    ]
    header = a.generated.resolve() / "r000.hpp"
    source = a.work / "check.cpp"
    source.write_text(
        """#include "r000.hpp"
#include <cstdio>
namespace g=nrforge_generated::r000;
int main(){
 g::Root expected{};g::T3 pick{};pick.value=g::T3::A1{};
 expected.value=g::T0::A2{pick};auto encoded=g::encode(expected);
 if(!encoded||encoded.value().semantic_bits!=5||encoded.value().octets!=std::vector<std::byte>{std::byte{0x40}})return 1;
 auto decoded=g::decode(encoded.value().octets);
 if(!decoded||decoded.value().value!=expected||decoded.value().semantic_bits!=5)return 2;
 std::puts("40 5");
}
"""
    )
    include = a.work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    command = [
        "c++",
        "-std=c++20",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wconversion",
        "-Wsign-conversion",
        "-pedantic-errors",
        "-I" + str(header.parent),
        "-I" + str(include.parent),
        str(source),
        str(REPO / "libuper/runtime.cpp"),
        "-o",
        str(a.work / "check"),
    ]
    subprocess.run(command, check=True)
    actual = subprocess.check_output([str(a.work / "check")], text=True).strip()
    assert actual == "40 5"
    import asn1tools.codecs.per
    import pycrate_asn1rt.asnobj_construct

    implementations = [
        Path(asn1tools.codecs.per.__file__),
        Path(pycrate_asn1rt.asnobj_construct.__file__),
    ]
    report = dict(
        batch="RRC-P7",
        status="NORMATIVE_MAPPING_PASS_REFERENCE_LIMITATION_RESOLVED",
        native_independent_pass_for_reversed_tags=False,
        normative_expected_hex="40",
        semantic_bits=5,
        actual_cpp_receipt=actual,
        normative_rules=[
            dict(
                clause="X.680 (02/2021) 29.2",
                rule="Any textual tag on a CHOICE alternative suppresses automatic tagging for the AlternativeTypeLists.",
            ),
            dict(
                clause="X.680 (02/2021) 31.2.4",
                rule="Omitted tag class in [0] / [2] is context-specific.",
            ),
            dict(
                clause="X.691 (02/2021) 10.2 and 23.2",
                rule="CHOICE root alternatives use canonical outermost tag order: nothing [0] precedes first [2].",
            ),
        ],
        primary_sources=[
            "https://www.itu.int/rec/T-REC-X.680-202102-I/en",
            "https://www.itu.int/rec/T-REC-X.691-202102-I/en",
        ],
        bit_derivation="Root extension=0, canonical root pick index=2 => 10, Pick extension=0, canonical nothing index=0 => 0: 01000 => 40 with zero padding.",
        oracle_observation=obs,
        implementation_audit=[
            dict(
                version="asn1tools0.167.0",
                sha256=sha(implementations[0]),
                finding="PER CHOICE compile_members(flat_additions=True) omits sort_by_tag; unlike SET compilation, declaration order remains.",
            ),
            dict(
                version="pycrate0.7.11",
                sha256=sha(implementations[1]),
                finding="CHOICE._to_per uses self._root.index(ident); the original tagged fixture keeps declaration order.",
            ),
        ],
        original_fixture_sha256=sha(REPO / "tools/rrc-uper/compound.asn1"),
        generated_header_sha256=sha(header),
        audit_source_sha256=sha(Path(__file__)),
        policy="No codec mapping change, no oracle patch, no frozen source reorder. This tagged case is normatively verified and explicitly excluded from native wire PASS. Actual frozen profile closures separately audited for compatible tag order.",
    )
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"])


if __name__ == "__main__":
    main()
