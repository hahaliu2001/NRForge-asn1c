#!/usr/bin/env python3
"""P7 finite independent RRC UPER wire/semantic campaign; no benchmarks."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import contextlib
import copy
import hashlib
from importlib.metadata import version
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate
from native_values import build, choice_variants, replace, pack, restore
from semantic_bridge import Bridge, literal
from pycrate_asn1c.asnproc import compile_text
from pycrate_asn1c.generator import PycrateGenerator

FLAGS = [
    "-std=c++20",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wconversion",
    "-Wsign-conversion",
    "-pedantic-errors",
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load(path):
    spec = importlib.util.spec_from_file_location("native_" + path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def native_compile(texts, path):
    # Each compiler invocation is a fresh process: pycrate GLOBAL is not shared
    # between fixture and frozen module families.
    script = path.with_suffix(".compile.py")
    inputs = path.with_suffix(".inputs.json")
    inputs.write_text(json.dumps(texts))
    script.write_text(
        "from pycrate_asn1c.asnproc import compile_text\nfrom pycrate_asn1c.generator import PycrateGenerator\nimport json\ncompile_text(json.load(open("
        + repr(str(inputs))
        + ")))\nPycrateGenerator("
        + repr(str(path))
        + ")\n"
    )
    with path.with_suffix(".compile.log").open("w") as log:
        subprocess.run(
            [sys.executable, str(script)],
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
        )
    return load(path)


def root(native, module, name):
    cls = getattr(native, module.replace("-", "_"))
    # Generated Python identifiers preserve original names through _name.
    candidates = [o for o in cls._all_ if o._name == name]
    if len(candidates) != 1:
        raise ValueError(("native identity", module, name))
    return candidates[0]


def audit_choices(obj):
    seen = set()
    rows = []

    def visit(o, path):
        if id(o) in seen:
            return
        seen.add(id(o))
        if o.TYPE == "CHOICE":
            tags = []
            for name in o._root:
                child = o._cont[name]
                chain = getattr(child, "_tagc", None)
                if not chain:
                    raise ValueError(
                        ("reference untagged root alternative", path, name)
                    )
                tags.append((name, tuple(chain[0])))
            if len({t for _, t in tags}) != len(tags) or [t for _, t in tags] != sorted(
                t for _, t in tags
            ):
                raise ValueError(
                    ("reference CHOICE canonical order differs", path, tags)
                )
            rows.append(
                dict(
                    path=path,
                    roots=[n for n, _ in tags],
                    tags=[list(t) for _, t in tags],
                )
            )
        if o.TYPE in ("SEQUENCE", "CHOICE"):
            for n, c in o._cont.items():
                visit(c, path + "." + n)
        elif o.TYPE == "SEQUENCE OF":
            visit(o._cont, path + "[]")

    visit(obj, obj._name)
    return rows


def oracle(obj, value):
    obj.set_val(copy.deepcopy(value))
    wire = obj.to_uper_ws()
    bits = sum(c.get_bl() for c in obj._struct if c._name != "P")
    obj.from_uper(wire)
    normalized = copy.deepcopy(obj.get_val())
    obj.set_val(copy.deepcopy(normalized))
    if obj.to_uper() != wire:
        raise ValueError("native decode/reencode changed wire")
    return wire, bits, normalized


def corpus_real(native, rows):
    result = []
    audits = []
    fixtures = json.loads(
        (REPO / "tools/rrc-readiness/fixtures-manifest.json").read_text()
    )["fixtures"]
    for i, row in enumerate(rows):
        if row["status"] != "PASS":
            continue
        obj = root(native, row["module"], row["root"])
        audit = audit_choices(obj)
        audits.append(dict(module=row["module"], root=row["root"], choices=audit))
        cases = [
            ("mandatory-low", build(obj)),
            ("mandatory-contrast", build(obj, True)),
        ]
        if "channel" in row["roles"]:
            base = cases[0][1]
            cases += [
                ("channel-choice-" + str(j), replace(base, path, value))
                for j, (path, value, _) in enumerate(choice_variants(obj, base))
            ]
        for fixture in fixtures:
            if (fixture["module"], fixture["asn1_type"]) == (
                row["module"],
                row["root"],
            ):
                wire = bytes.fromhex(fixture["hex"])
                obj.from_uper(wire)
                value = obj.get_val()
                actual, bits, _ = oracle(obj, value)
                if actual != wire or bits != fixture["semantic_bits"]:
                    raise ValueError(("frozen seed native", fixture["asn1_type"]))
                cases.append(("frozen-seed", value))
        seen = set()
        records = []
        for name, value in cases:
            wire, bits, normalized = oracle(obj, value)
            key = json.dumps(pack(value), sort_keys=True)
            if key in seen and name != "frozen-seed":
                continue
            seen.add(key)
            records.append(
                dict(
                    id=name,
                    value=pack(value),
                    normalized=pack(normalized),
                    wire_hex=wire.hex(),
                    semantic_bits=bits,
                    offset=0,
                )
            )
        result.append(
            dict(
                index=i,
                module=row["module"],
                root=row["root"],
                roles=row["roles"],
                cases=records,
            )
        )
    return result, audits


def fixture_schema():
    schema = (HERE / "compound.asn1").read_text()
    return schema.replace(
        "\nEND",
        "\n"
        + "".join(
            f"W{n} ::= SEQUENCE {{ p BIT STRING (SIZE ({n})), v Root }}\n"
            for n in range(1, 8)
        )
        + "FutureW3 ::= SEQUENCE { p BIT STRING (SIZE (3)), v FutureRoot }\nEND",
    )


def corpus_compound(native):
    cls = native.RrcWireFixture
    obj = cls.Root
    audit_choices(obj)
    plain = dict(small=[1, 2], extList=[3], octets=b"\x12\x34", bits=(1, 1))
    values = [
        ("defaults-absent", ("body", plain)),
        ("defaults-explicit", ("body", dict(plain, counter=3, mode="off"))),
        (
            "defaults-nondefault",
            ("body", dict(plain, counter=31, mode="on", flag=False)),
        ),
        ("group-default", ("body", dict(plain, required=0, fallback=2))),
        (
            "group-full",
            ("body", dict(plain, added=31, required=11, maybe=False, fallback=7)),
        ),
        ("list-extension", ("body", dict(plain, extList=[1, 2, 3, 4]))),
        ("bit-extension", ("body", dict(plain, bits=(1023, 10)))),
        ("enum-extension", ("body", dict(plain, mode="extra"))),
        ("enum-future", ("body", dict(plain, mode="_ext_64"))),
        ("choice-null", ("pick", ("nothing", 0))),
        ("choice-counter", ("pick", ("first", 31))),
        ("choice-extension", ("pick", ("later", [3, 31]))),
        ("choice-bool-extension", ("pick", ("future", False))),
        ("root-extension", ("later", plain)),
    ]
    for n in (0, 1, 16383, 16384, 16385, 65536):
        values.append(("collection-" + str(n), ("chunk", [0] * n)))
    for n in (127, 128, 16384, 16385):
        values.append(
            ("octets-" + str(n), ("body", dict(plain, octets=bytes([0xA5]) * n)))
        )
    cases = []
    for name, value in values:
        offsets = (
            range(8)
            if name
            in ("defaults-absent", "group-full", "choice-extension", "root-extension")
            else [0]
        )
        for offset in offsets:
            wrapped = (
                {"p": ((1 << offset) - 1, offset), "v": value} if offset else value
            )
            desc = getattr(cls, "W" + str(offset)) if offset else obj
            wire, bits, normalized = oracle(desc, wrapped)
            norm = normalized["v"] if offset else normalized
            cases.append(
                dict(
                    id=name + "-offset" + str(offset),
                    value=pack(value),
                    normalized=pack(norm),
                    wire_hex=wire.hex(),
                    semantic_bits=bits,
                    offset=offset,
                )
            )
    # Independently encode a future sender; receiver projection for unknown
    # SEQUENCE fields is an explicit policy, not byte-preserving roundtrip.
    future = ("body", dict(plain, newValue=b"future"))
    wire, bits, _ = oracle(cls.FutureRoot, future)
    projected, projected_bits, norm = oracle(obj, ("body", plain))
    cases.append(
        dict(
            id="future-sequence-discard",
            value=pack(("body", plain)),
            normalized=pack(norm),
            wire_hex=wire.hex(),
            semantic_bits=bits,
            offset=0,
            encoded_hex=projected.hex(),
            encoded_bits=projected_bits,
            policy="discard_future_sequence",
        )
    )
    for n in (1, 128, 16384):
        for offset in (0, 3):
            sender = ("newer", bytes([0x5A]) * n)
            if offset:
                wire, bits, _ = oracle(cls.FutureW3, dict(p=(7, 3), v=sender))
                cls.W3.from_uper(wire)
                unknown = cls.W3.get_val()["v"]
            else:
                wire, bits, _ = oracle(cls.FutureRoot, sender)
                obj.from_uper(wire)
                unknown = obj.get_val()
            if unknown[0] != "_ext_1":
                raise ValueError("native unknown CHOICE index")
            cases.append(
                dict(
                    id="future-choice-retain-" + str(n) + "-offset" + str(offset),
                    value=pack(unknown),
                    normalized=pack(unknown),
                    wire_hex=wire.hex(),
                    semantic_bits=bits,
                    offset=offset,
                    policy="retain_future_choice",
                )
            )
    return dict(
        index=0, module="RrcWireFixture", root="Root", roles=["compound"], cases=cases
    )


def emit(row, header):
    pre = [
        '#include "' + header.name + '"',
        "#include <cstdio>",
        "#include <cstdlib>",
        "#include <iostream>",
        '#define CHECK(...) do {if(!(__VA_ARGS__)){std::cerr<<"qualification failure "<<__LINE__<<"\\n";std::abort();}}while(false)',
        "using namespace nrforge::uper;",
        "namespace g=" + Bridge(header.read_text()).ns + ";",
        "template<class T>T checked(Result<T> r){CHECK(r);return std::move(r).value();}",
        "void checked(Result<void> r){CHECK(r);}",
        "void negative(std::span<const std::byte> bytes,std::size_t bits,unsigned offset,bool unknown,const Usage& usage){",
        "std::vector<std::size_t> points;if(bits<=256){for(std::size_t n=0;n<bits;++n)points.push_back(n);}else points={0,offset,std::size_t{offset}+1,bits/2,bits-1};",
        "for(auto n:points){auto r=checked(Reader::make(bytes,{},n));if(n<offset){CHECK(!r.read_bits(offset));CHECK(r.cursor_bit()==0);continue;}checked(r.read_bits(offset));auto s=g::get0(r);CHECK(!s);CHECK(r.cursor_bit()==offset&&r.usage().work_bits==offset&&r.usage().value_octets==0&&r.usage().collection_elements==0);}",
        "auto extra=std::vector<std::byte>(bytes.begin(),bytes.end());extra.push_back(std::byte{0});auto rr=checked(Reader::make(extra));checked(rr.read_bits(offset));auto v=g::get0(rr);CHECK(v&&!rr.finish());",
        "if(bits%8){auto pad=std::vector<std::byte>(bytes.begin(),bytes.end());pad.back()|=std::byte{1};auto pr=checked(Reader::make(pad));checked(pr.read_bits(offset));auto pv=g::get0(pr);CHECK(pv&&!pr.finish()&&pr.first_error()->code==ErrorCode::nonzero_padding);}",
        "Limits depth;depth.max_depth=0;auto dr=checked(Reader::make(bytes,depth));checked(dr.read_bits(offset));CHECK(!g::get0(dr)&&dr.cursor_bit()==offset);",
        "Limits work;work.max_work_bits=offset;auto wr=checked(Reader::make(bytes,work));checked(wr.read_bits(offset));CHECK(!g::get0(wr)&&wr.cursor_bit()==offset&&wr.usage().work_bits==offset);",
        "if(unknown){Limits limits;limits.max_unknown_records=0;auto ur=checked(Reader::make(bytes,limits));checked(ur.read_bits(offset));auto us=g::get0(ur);CHECK(!us&&us.error().code==ErrorCode::resource_limit&&ur.cursor_bit()==offset&&ur.usage().unknown_records==0);}",
        "std::size_t budgets=0;for(unsigned k=0;k<3;++k){auto quota=k==0?usage.value_octets:k==1?usage.collection_elements:usage.bitmap_bits;if(!quota)continue;Limits l;if(k==0)l.max_value_octets=quota-1;else if(k==1)l.max_collection_elements=quota-1;else l.max_bitmap_bits=quota-1;auto br=checked(Reader::make(bytes,l));checked(br.read_bits(offset));auto bs=g::get0(br);CHECK(!bs&&bs.error().code==ErrorCode::resource_limit&&br.cursor_bit()==offset&&br.usage().value_octets==0&&br.usage().collection_elements==0&&br.usage().bitmap_bits==0);++budgets;}",
        "Limits input;input.max_input_octets=bytes.size()-1;CHECK(!Reader::make(bytes,input));",
        'std::cout<<"NEG "<<points.size()+4+budgets+static_cast<unsigned>(bits%8!=0)+static_cast<unsigned>(unknown)<<"\\n";}',
    ]
    functions = []
    for i, case in enumerate(row["cases"]):
        bridge = Bridge(header.read_text())
        bridge.walk(
            root_for_emit[(row["module"], row["root"])],
            restore(case["value"]),
            "T0",
            "expected",
        )
        wire = bytes.fromhex(case["wire_hex"])
        encoded = bytes.fromhex(case.get("encoded_hex", case["wire_hex"]))
        bits = case["semantic_bits"]
        encbits = case.get("encoded_bits", bits)
        offset = case["offset"]
        functions += [
            f"void case{i}(){{g::Root expected{{}};",
            *bridge.lines,
            f"const auto native_wire={literal(wire)};",
            "auto r=checked(Reader::make(native_wire));",
            f"CHECK(checked(r.read_bits({offset}))=={(1<<offset)-1});",
            "auto actual=checked(g::get0(r));CHECK(actual==expected);",
            f"CHECK(r.cursor_bit()=={bits});checked(r.finish());",
            "Writer writer;",
            f"checked(writer.write_bits({(1<<offset)-1},{offset}));checked(g::put0(writer,expected));auto encoded=checked(writer.finish());",
            f"CHECK(encoded.octets=={literal(encoded)}&&encoded.semantic_bits=={encbits});",
            f'negative(native_wire,{bits},{offset},{str(case.get("policy")=="retain_future_choice").lower()},r.usage());',
            f'std::cout<<"CASE {i} ";for(auto b:encoded.octets){{auto n=std::to_integer<unsigned>(b);std::cout<<"0123456789abcdef"[n>>4]<<"0123456789abcdef"[n&15];}}std::cout<<" "<<encoded.semantic_bits<<"\\n";',
            "}",
        ]
    return (
        "\n".join(
            pre
            + functions
            + [
                "int main(){"
                + "".join(f"case{i}();" for i in range(len(row["cases"])))
                + "}"
            ]
        )
        + "\n"
    )


root_for_emit = {}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--generated", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--only-compound", action="store_true")
    a = p.parse_args()
    if not 1 <= a.jobs <= 4:
        raise ValueError("jobs must be 1..4")
    if version("pycrate") != "0.7.11":
        raise ValueError("pycrate0.7.11 required")
    source_guard = {
        str(p.relative_to(REPO)): sha(p)
        for p in sorted(HERE.glob("*"))
        if p.is_file() and p.suffix in (".py", ".asn1")
    }
    a.work.mkdir(parents=True, exist_ok=False)
    a.work = a.work.resolve()
    manifest = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(a.asn1_root, manifest)
    matrix = json.loads((REPO / "tools/rrc-uper/readiness-rrc-p6.json").read_text())
    rows = matrix["roots"] + matrix["default_seed_roots"]
    for i, row in enumerate(rows):
        if (
            row["status"] == "PASS"
            and sha(a.generated / f"r{i:03}.hpp") != row["header_sha256"]
        ):
            raise ValueError(("generated P6 fingerprint", row["root"]))
    if a.only_compound:
        native = None
        real, audits = [], []
    else:
        native = native_compile(
            [(a.asn1_root / m["path"]).read_text() for m in manifest["modules"]],
            a.work / "frozen.py",
        )
        real, audits = corpus_real(native, rows)
    fixture = native_compile([fixture_schema()], a.work / "fixture.py")
    synthetic = corpus_compound(fixture)
    for row in real:
        root_for_emit[row["module"], row["root"]] = root(
            native, row["module"], row["root"]
        )
    root_for_emit["RrcWireFixture", "Root"] = fixture.RrcWireFixture.Root
    (a.work / "corpus.json").write_text(
        json.dumps(dict(real=real, compound=synthetic), indent=2) + "\n"
    )
    compound = a.work / "compound"
    compound.mkdir()
    (compound / "modules.list").write_text("compound.asn1\n")
    (compound / "roots.list").write_text("RrcWireFixture Root\n")
    raw = subprocess.check_output(
        [
            str(a.probe.resolve()),
            str(compound / "modules.list"),
            str(HERE),
            str(compound / "roots.list"),
            str(compound),
        ]
    )
    graph = json.loads(raw)
    assert graph["roots"][0]["render"] == 0 and graph["owned_after_tree_destruction"]
    include = a.work / "include/uper"
    include.mkdir(parents=True)
    (include / "runtime.hpp").symlink_to(REPO / "libuper/runtime.hpp")
    runtime = a.work / "runtime.o"
    flags = FLAGS + (
        ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
        if a.sanitize
        else []
    )
    subprocess.run(
        ["c++", *flags, "-c", str(REPO / "libuper/runtime.cpp"), "-o", str(runtime)],
        check=True,
    )

    def run(row, header, folder):
        folder.mkdir()
        source = folder / "check.cpp"
        source.write_text(emit(row, header))
        exe = folder / "check"
        command = [
            "c++",
            *flags,
            "-I" + str(header.parent),
            "-I" + str(include.parent),
            str(source),
            str(runtime),
            "-o",
            str(exe),
        ]
        done = subprocess.run(command, capture_output=True, text=True)
        (folder / "compile.log").write_text(done.stdout + done.stderr)
        if done.returncode:
            raise RuntimeError((row["root"], done.stderr[:1500]))
        env = dict(
            os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"
        )
        done = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
        (folder / "run.log").write_text(done.stdout + done.stderr)
        if done.returncode:
            raise RuntimeError((row["root"], done.stdout[-500:], done.stderr[-1000:]))
        receipts = [
            line.split()
            for line in done.stdout.splitlines()
            if line.startswith("CASE ")
        ]
        if len(receipts) != len(row["cases"]):
            raise ValueError("missing case receipt")
        return (
            row,
            receipts,
            sum(
                int(line.split()[1])
                for line in done.stdout.splitlines()
                if line.startswith("NEG ")
            ),
        )

    jobs = [
        (
            row,
            a.generated / f'r{row["index"]:03}.hpp',
            a.work / f'real-{row["index"]:03}',
        )
        for row in real
    ]
    jobs.append((synthetic, compound / "r000.hpp", a.work / "synthetic"))
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        results = list(pool.map(lambda j: run(*j), jobs))
    negative_count = 0
    case_count = 0
    for row, receipts, negatives in results:
        negative_count += negatives
        for i, receipt in enumerate(receipts):
            if receipt[1] != str(i):
                raise ValueError("receipt order")
            case = row["cases"][i]
            wire = bytes.fromhex(receipt[2])
            bits = int(receipt[3])
            offset = case["offset"]
            if (wire.hex(), bits) != (
                case.get("encoded_hex", case["wire_hex"]),
                case.get("encoded_bits", case["semantic_bits"]),
            ):
                raise ValueError("C++ encode receipt mismatch")
            obj = root_for_emit[row["module"], row["root"]]
            desc = getattr(fixture.RrcWireFixture, "W" + str(offset)) if offset else obj
            desc.from_uper(wire)
            value = desc.get_val()
            value = value["v"] if offset else value
            if pack(value) != case["normalized"]:
                raise ValueError(
                    ("native decode C++ semantic mismatch", row["root"], case["id"])
                )
            case_count += 1
    authenticate(a.asn1_root, manifest)
    sources = {
        str(p.relative_to(REPO)): sha(p)
        for p in sorted(HERE.glob("*"))
        if p.is_file() and p.suffix in (".py", ".asn1")
    }
    if sources != source_guard:
        raise ValueError("qualification sources changed while campaign was running")
    report = dict(
        batch="RRC-P7",
        baseline="e3d7f359f7dac56bfe6d4f461b1c6991665dc912",
        status="FINITE_PROFILE_PASS",
        full_rrc_coverage=False,
        source_authority=manifest,
        native_oracle=dict(
            pycrate_version="0.7.11",
            frozen_generated_sha256=sha(a.work / "frozen.py") if native else None,
            fixture_generated_sha256=sha(a.work / "fixture.py"),
            original_frozen_modules_unmodified=True,
        ),
        strict_flags=flags,
        sanitized=a.sanitize,
        counts=dict(
            qualified_real_roots=len(real),
            selected_real_roots=len(
                [r for r in real if "default_seed" not in r["roles"]]
            ),
            default_seed_roots=sum("default_seed" in r["roles"] for r in real),
            qualified_channel_roots=sum("channel" in r["roles"] for r in real),
            independent_cases=case_count,
            negative_checks=negative_count,
        ),
        blocked_roots=[
            dict(
                module=r["module"],
                root=r["root"],
                roles=r["roles"],
                status=r["status"],
                error=r["error"],
            )
            for r in rows
            if r["status"] != "PASS"
        ],
        choice_reference_audits=audits,
        corpus_sha256=sha(a.work / "corpus.json"),
        source_sha256=sources,
        real_cases=real,
        compound_cases=synthetic,
        limitations=[
            "Finite semantic profile only; no all-field/all-value claim.",
            "SIB1 fixture is extraction-blocked and not wire-qualified.",
            "16 extraction gaps and five CHOICE evidence gaps retained.",
            "Reversed explicit-tag CHOICE is reference-limited in asn1tools0.167.0/pycrate0.7.11; separate normative audit required, not silently reordered.",
            "Future SEQUENCE values are projected by discarding validated unknown additions; future CHOICE payloads are retained.",
            "Real profile covers mandatory low/contrast and mandatory-baseline channel branches, not every OPTIONAL or SIZE boundary.",
            "pycrate group DEFAULT input requires explicit fallback=2; native encoding suppresses it and native decode restores it; no oracle patch.",
            "No installed SDK, benchmark or merge.",
        ],
    )
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["counts"]))


if __name__ == "__main__":
    main()
