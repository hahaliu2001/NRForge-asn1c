#!/usr/bin/env python3
"""Record exploratory compound oracle differences; never qualifies full wire."""
import argparse
import contextlib
import hashlib
from importlib.metadata import version
import importlib.util
import io
import json
from pathlib import Path
import asn1tools
from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    assert version("asn1tools") == "0.167.0" and version("pycrate") == "0.7.11"
    a.work.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).with_name("compound.asn1").read_bytes()
    schema = source.decode()
    first = asn1tools.compile_string(schema, "uper")
    # pycrate cannot compile the fixture's type formal T; explicitly disclose
    # expansion of this single specialization, without modifying oracle code.
    expanded = schema.replace(
        "Box { T } ::= SEQUENCE { value T }",
        "BoxCounter ::= SEQUENCE { value Counter }",
    ).replace("Box { Counter }", "BoxCounter")
    (a.work / "expanded.asn1").write_text(expanded)
    native = a.work / "native.py"
    with contextlib.redirect_stdout(io.StringIO()):
        compile_text(expanded)
        PycrateGenerator(str(native))
    spec = importlib.util.spec_from_file_location("p6_native", native)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    second = module.UperFixture.Root
    body = {"small": [1, 2], "box": {"value": 4}, "extList": [3]}
    cases = [
        ("minimal", ("body", body), "01044403", 32),
        (
            "group",
            (
                "body",
                dict(
                    body,
                    counter=31,
                    mode="on",
                    flag=True,
                    extList=[1, 2, 3, 4],
                    added=9,
                    required=11,
                    maybe=False,
                    fallback=7,
                ),
            ),
            "1ff30444820443201c05200b58e0",
            110,
        ),
        ("choice_root", ("pick", ("nothing", None)), "40", 5),
        ("choice_extension", ("pick", ("later", [3])), "500028c0", 27),
        ("root_extension", ("later", body), "800408222018", 48),
    ]
    rows = []
    for name, value, wire, bits in cases:
        left = first.encode("Root", value).hex()
        second.set_val(("pick", ("nothing", 0)) if name == "choice_root" else value)
        right = second.to_uper().hex()
        assert left == right
        rows.append(
            dict(
                case=name,
                generated_focused_hex=wire,
                generated_semantic_bits=bits,
                asn1tools_hex=left,
                pycrate_hex=right,
                byte_match=left == wire,
            )
        )
    assert sum(r["byte_match"] for r in rows) == 4
    report = dict(
        status="EXPLORATORY_ONLY",
        full_wire_qualification=False,
        fixture_sha256=hashlib.sha256(source).hexdigest(),
        asn1tools_version="0.167.0",
        pycrate_version="0.7.11",
        pycrate_limitation="Original parameter formal T triggers compiler assertion; only Box { Counter } explicitly expanded to BoxCounter; oracle implementation unmodified.",
        expanded_schema_sha256=hashlib.sha256(expanded.encode()).hexdigest(),
        cases=rows,
        discrepancy="AUTOMATIC TAGS Pick first [2] Counter / nothing [0] NULL: inherited owned canonical root mapping selects nothing index0; both reference codecs emit declaration-order-compatible index1. Generated Root.pick/nothing is 40 (5 semantic bits); references emit 48.",
        next_gate="RRC-P7: independently audit X.691 10.2/23.2 and X.680 automatic/explicit tag rules and each oracle implementation before claiming this case independently qualified. Do not replace validated mapping with declaration order.",
    )
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(
        "Exploratory compound: 4 matching byte cases, 1 explicit-tag discrepancy recorded; no wire qualification PASS"
    )


if __name__ == "__main__":
    main()
