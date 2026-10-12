#!/usr/bin/env python3
"""P16 finite expanded independent RRC UPER wire/semantic campaign; no benchmarks."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import contextlib
import copy
import hashlib
import gzip
from importlib.metadata import version
import importlib.util
import io
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate
from native_values import build, choice_variants, replace, pack, restore, carrier_projection
from witnesses import REQUESTS, selected
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
    "--param=ggc-min-expand=5",
    "--param=ggc-min-heapsize=16384",
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
    # Preserve the complete generated source before long compiler work. The
    # compressed snapshot, not a later mutable scratch raw copy, is authoritative.
    path.with_suffix(path.suffix + ".gz").write_bytes(gzip.compress(path.read_bytes(), mtime=0))
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
    normalized = carrier_projection(obj, copy.deepcopy(obj.get_val()))
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
        witness_records = []
        for module, name, feature, target in REQUESTS:
            if (module, name) == (row["module"], row["root"]):
                value, descriptor = selected(obj, feature, target)
                cases.append(("witness-" + feature, value)); witness_records.append(descriptor)
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
                witnesses=witness_records, cases=records,
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
    p.add_argument("--jobs", type=int, default=1)
    p.add_argument("--compile-cache", type=Path, help="optional SHA-validated compile cache; executables are always rerun")
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
    guarded = ["libasn1typed/asn1typed_extract.c", "libasn1typed/asn1typed_render_uper.c", "libuper/runtime.cpp", "libuper/runtime.hpp", "tools/rrc-uper/probe.c", "tools/rrc-null-open/readiness.json", "tools/rrc-wire-qualification/qualification.json", "tools/rrc-wire-qualification/qualification-sanitized.json", "tools/rrc-wire-qualification/reference-boundary.json", "tools/rrc-readiness/fixtures-manifest.json", "tools/rrc-readiness/source-manifest.json", "libasn1typed/.libs/libasn1typed_extract.a", "libasn1typed/.libs/libasn1typed.a", "libasn1fix/.libs/libasn1fix.a", "libasn1parser/.libs/libasn1parser.a", "libasn1common/.libs/libasn1common.a", "tools/developer_tree.o"]
    producer_guard = {n: sha(REPO / n) for n in guarded}
    a.work.mkdir(parents=True, exist_ok=False)
    a.work = a.work.resolve()
    manifest = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(a.asn1_root, manifest)
    matrix = json.loads((REPO / "tools/rrc-null-open/readiness.json").read_text())
    rows = matrix["matrix"]["roots"]
    for i, row in enumerate(rows):
        if (
            row["status"] == "PASS"
            and sha(a.generated / f"r{i:03}.hpp") != row["header_sha256"]
        ):
            raise ValueError(("generated current fingerprint", row["root"]))
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
    accepted = json.loads((REPO / "tools/rrc-wire-qualification/qualification.json").read_text())
    preserved_cases = 0
    new_by_id = {(r["module"], r["root"]): r for r in real}
    if not a.only_compound:
        for old in accepted["real_cases"]:
            candidates = {c["id"]: c for c in new_by_id[old["module"], old["root"]]["cases"]}
            for case in old["cases"]:
                assert candidates[case["id"]] == case
                preserved_cases += 1
        assert preserved_cases == 143 and len(real) == 85
    assert synthetic == accepted["compound_cases"]
    for row in real:
        root_for_emit[row["module"], row["root"]] = root(
            native, row["module"], row["root"]
        )
    root_for_emit["RrcWireFixture", "Root"] = fixture.RrcWireFixture.Root
    (a.work / "corpus.json").write_text(
        json.dumps(dict(real=real, compound=synthetic), indent=2) + "\n"
    )
    corpus_bytes = (a.work / "corpus.json").read_bytes()
    (a.work / "corpus.json.gz").write_bytes(gzip.compress(corpus_bytes, mtime=0))
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

    compiler = subprocess.check_output(["c++", "--version"], text=True).splitlines()[0]
    compiler_binary = sha(shutil.which("c++"))
    native_sources = {name: hashlib.sha256(gzip.decompress((a.work / name).read_bytes())).hexdigest()
                      for name in ("frozen.py.gz", "fixture.py.gz") if (a.work / name).exists()}
    oracle_sources = {}
    for package in ("pycrate_asn1rt", "pycrate_asn1c", "pycrate_core"):
        package_root = Path(importlib.import_module(package).__file__).parent
        for path in sorted(package_root.rglob("*.py")):
            oracle_sources[package + "/" + str(path.relative_to(package_root))] = sha(path)
    reused = []

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
        (folder / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        cache_key = dict(header_sha256=sha(header), source_sha256=sha(source), runtime_sha256=sha(runtime),
                         flags=flags, compiler=compiler, compiler_binary_sha256=compiler_binary,
                         native_sources_sha256=native_sources, oracle_sources_sha256=oracle_sources,
                         corpus_sha256=hashlib.sha256(corpus_bytes).hexdigest(),
                         qualification_sources_sha256=source_guard, producer_inputs_sha256=producer_guard)
        cached = a.compile_cache / folder.name if a.compile_cache else None
        if cached and (cached / "manifest.json").exists():
            manifest_cache = json.loads((cached / "manifest.json").read_text())
            if manifest_cache["compile_returncode"] != 0 or set(manifest_cache["artifacts_sha256"]) != {"check", "compile.log", "command.json"}:
                raise ValueError("incomplete compile cache receipt")
            original = manifest_cache["original_command"]
            if json.loads((cached / "command.json").read_text()) != original or len(original) != 1 + len(flags) + 6 or original[0] != "c++" or original[1:1 + len(flags)] != flags or original[-2] != "-o":
                raise ValueError("compile cache command/flags mismatch")
            if manifest_cache["key"] != cache_key:
                raise ValueError(("compile cache input mismatch", row["root"]))
            for name, digest in manifest_cache["artifacts_sha256"].items():
                if sha(cached / name) != digest: raise ValueError(("compile cache artifact mismatch", name))
            shutil.copy2(cached / "check", exe)
            shutil.copy2(cached / "compile.log", folder / "compile.log")
            shutil.copy2(cached / "command.json", folder / "command.json")
            (folder / "cache-receipt.json").write_text(json.dumps(manifest_cache, indent=2) + "\n")
            reused.append(row["module"] + "." + row["root"])
        else:
            done = subprocess.run(command, capture_output=True, text=True)
            (folder / "compile.log").write_text(done.stdout + done.stderr)
            if done.returncode:
                raise RuntimeError((row["root"], done.stderr[:1500]))
            if cached:
                cached.mkdir(parents=True, exist_ok=False)
                for name in ("check", "compile.log", "command.json"):
                    shutil.copy2(folder / name, cached / name)
                manifest_cache = dict(key=cache_key, original_command=command, compile_returncode=0,
                                      artifacts_sha256={name: sha(cached / name) for name in ("check", "compile.log", "command.json")})
                (cached / "manifest.json").write_text(json.dumps(manifest_cache, indent=2) + "\n")
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
        print("ROOT_PASS " + row["module"] + " " + row["root"] + " cases=" + str(len(receipts)), flush=True)
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
            value = carrier_projection(obj, value)
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
        batch="RRC-P16",
        baseline="fc12b37d8d7527c5f2591c16a9728ec367e02114",
        status="FINITE_PROFILE_PASS",
        full_rrc_coverage=False,
        source_authority=manifest,
        native_oracle=dict(
            pycrate_version="0.7.11",
            frozen_generated_sha256=hashlib.sha256(gzip.decompress((a.work / "frozen.py.gz").read_bytes())).hexdigest() if native else None,
            fixture_generated_sha256=hashlib.sha256(gzip.decompress((a.work / "fixture.py.gz").read_bytes())).hexdigest(),
            original_frozen_modules_unmodified=True,
        ),
        strict_flags=flags,
        compiler=compiler, compiler_binary_sha256=compiler_binary,
        reused_compile_roots=sorted(reused),
        oracle_sources_sha256=oracle_sources,
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
        corpus_sha256=hashlib.sha256(corpus_bytes).hexdigest(),
        source_sha256=sources, producer_inputs_sha256=producer_guard,
        preserved_p7_real_cases=preserved_cases, preserved_p7_compound_cases=59,
        sdk_delivered_roots=64, benchmark_run=False,
        real_cases=real,
        compound_cases=synthetic,
        limitations=[
            "Finite semantic profile only; no all-field/all-value claim.",
            "SIB1 frozen fixture is included after semantic repairs.",
            "All 85 generated roots qualify only this finite selected semantic profile.",
            "Reversed explicit-tag CHOICE is reference-limited in asn1tools0.167.0/pycrate0.7.11; separate normative audit required, not silently reordered.",
            "Future SEQUENCE values are projected by discarding validated unknown additions; future CHOICE payloads are retained.",
            "Real profile covers mandatory low/contrast and mandatory-baseline channel branches, not every OPTIONAL or SIZE boundary.",
            "pycrate group DEFAULT input requires explicit fallback=2; native encoding suppresses it and native decode restores it; no oracle patch.",
            "Native pycrate automatically decodes Contents; canonical native-produced payloads are compared as opaque bytes after descriptor-driven projection; no arbitrary malformed-inner oracle claim.",
            "Seven optional witnesses cover only recorded native descriptor paths, not every field.",
            "SDK remains 64 roots; no installed SDK expansion, benchmark or merge.",
        ],
    )
    assert producer_guard == {n: sha(REPO / n) for n in guarded}
    assert compiler_binary == sha(shutil.which("c++"))
    current_oracle_sources = {}
    for package in ("pycrate_asn1rt", "pycrate_asn1c", "pycrate_core"):
        package_root = Path(importlib.import_module(package).__file__).parent
        for path in sorted(package_root.rglob("*.py")):
            current_oracle_sources[package + "/" + str(path.relative_to(package_root))] = sha(path)
    assert oracle_sources == current_oracle_sources
    report["artifact_sha256"] = {str(f.relative_to(a.work)): sha(f) for f in sorted(a.work.rglob("*")) if f.is_file() and (f.suffix in (".cpp", ".log", ".gz") or f.name in ("inputs.json", "command.json", "cache-receipt.json", "runtime.o", "check"))}
    a.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["counts"]))


if __name__ == "__main__":
    main()
