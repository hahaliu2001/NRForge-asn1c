#!/usr/bin/env python3
"""Prepare public installed consumers from accepted independent P16 semantics."""
import argparse
import json
from importlib.metadata import version
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-wire-expanded"))
import qualify
from native_values import pack, restore, carrier_projection
from scan import authenticate


def public_emit(row, header, entry):
    from semantic_bridge import Bridge, literal
    obj = qualify.root_for_emit[row["module"], row["root"]]
    lines = ['#include <' + entry["header"] + '>', '#include <iostream>', '#include <algorithm>', '#include <cstdlib>',
             '#define CHECK(...) do {if(!(__VA_ARGS__)){std::cerr<<"public qualification failure "<<__LINE__<<"\\n";std::abort();}}while(false)',
             'using namespace nrforge::uper;', 'namespace g=' + entry["namespace"] + ';',
             'template<class T>T checked(Result<T> r){CHECK(r);return std::move(r).value();}']
    for i,c in enumerate(row["cases"]):
        assert c["offset"] == 0
        b=Bridge(header.read_text());b.walk(obj,restore(c["value"]),"T0","expected")
        bits=c["semantic_bits"]
        lines += [f'void case{i}(){{g::Root expected{{}};',*b.lines,
                  'const auto native_wire='+literal(bytes.fromhex(c["wire_hex"]))+';',
                  f'auto decoded=checked(g::decode(native_wire));CHECK(decoded.value==expected&&decoded.semantic_bits=={bits});',
                  f'auto encoded=checked(g::encode(expected));CHECK(encoded.octets==native_wire&&encoded.semantic_bits=={bits}&&encoded.encoded_bits==native_wire.size()*8);',
                  'for(auto mode:{Completion::contained_bits,Completion::exact}){auto e=checked(g::encode(expected,{},mode));',
                  f'CHECK(e.octets==native_wire&&e.semantic_bits=={bits}&&e.encoded_bits=={bits});auto d=checked(g::decode(e.octets,{{}},e.encoded_bits,mode));CHECK(d.value==expected&&d.semantic_bits=={bits});}}',
                  'auto storage=native_wire;auto owned=checked(g::decode(storage));std::fill(storage.begin(),storage.end(),std::byte{0xa5});CHECK(owned.value==expected);',
                  'std::size_t negatives=0;',
                  f'std::vector<std::size_t> points;if({bits}<=256){{for(std::size_t n=0;n<{bits};++n)points.push_back(n);}}else points={{0,1,{bits}//2,{bits}-1}};'.replace('//2','/2'),
                  'for(auto n:points){CHECK(!g::decode(native_wire,{},n,Completion::contained_bits));++negatives;}',
                  'auto extra=native_wire;extra.push_back(std::byte{0});CHECK(!g::decode(extra));++negatives;',
                  f'if({bits}%8){{auto pad=native_wire;pad.back()|=std::byte{{1}};auto d=g::decode(pad);CHECK(!d&&d.error().code==ErrorCode::nonzero_padding);++negatives;}}',
                  'Limits depth;depth.max_depth=0;auto failed=g::decode(native_wire,depth);CHECK(!failed&&failed.error().code==ErrorCode::resource_limit);++negatives;',
                  f'Limits work;work.max_work_bits={bits}-1;CHECK(!g::decode(native_wire,work));++negatives;',
                  'Limits input;input.max_input_octets=native_wire.size()-1;CHECK(!g::decode(native_wire,input));++negatives;',
                  f'std::cout<<"CASE {i} ";for(auto byte:encoded.octets){{auto n=std::to_integer<unsigned>(byte);std::cout<<"0123456789abcdef"[n>>4]<<"0123456789abcdef"[n&15];}}std::cout<<" "<<encoded.semantic_bits<<"\\nNEG "<<negatives<<"\\n";', '}']
    return '\n'.join(lines)+ '\nint main(){'+''.join(f'case{i}();' for i in range(len(row["cases"])))+'}\n'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("asn1-root", "generated", "work"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    if version("pycrate") != "0.7.11":
        raise ValueError("pycrate0.7.11 required")
    bound_paths = [Path(__file__).resolve(), REPO / "tools/rrc-wire-expanded/qualify.py",
                   REPO / "tools/rrc-wire-expanded/native_values.py",
                   REPO / "tools/rrc-wire-expanded/semantic_bridge.py",
                   REPO / "tools/rrc-wire-expanded/qualification.json",
                   a.generated / "manifest.json"]
    bound_paths += [REPO / "tools/rrc-readiness" / n for n in ["scan.py", "source-manifest.json", "rrc-rel18.modules"]]
    bound_before = {str(p.resolve()): qualify.sha(p) for p in bound_paths}
    a.work.mkdir(parents=True, exist_ok=False)
    work = a.work.resolve()
    authority = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(a.asn1_root, authority)
    native = qualify.native_compile(
        [(a.asn1_root / m["path"]).read_text() for m in authority["modules"]],
        work / "native.py",
    )
    q = json.loads(
        (REPO / "tools/rrc-wire-expanded/qualification.json").read_text()
    )
    manifest = json.loads((a.generated / "manifest.json").read_text())
    entries = {r["index"]: r for r in manifest["roots"]}
    assert len(entries) == len(q["real_cases"]) == 85
    assert manifest["qualification_sha256"] == qualify.sha(REPO / "tools/rrc-wire-expanded/qualification.json")
    expected_cases = {}
    source = work / "consumer"
    source.mkdir()
    cmake = [
        "cmake_minimum_required(VERSION 3.20)",
        "project(RRCInstalledConsumer LANGUAGES CXX)",
        "find_package(NRForgeRRC 0.2.0 EXACT CONFIG REQUIRED)",
        "enable_testing()",
    ]
    for row in q["real_cases"]:
        e = entries[row["index"]]
        assert (e["module"], e["root"]) == (row["module"], row["root"])
        sealed_header = a.generated / "include" / e["header"]
        assert qualify.sha(sealed_header) == e["installed_header_sha256"]
        bound_before[str(sealed_header.resolve())] = qualify.sha(sealed_header)
        obj = qualify.root(native, row["module"], row["root"])
        # Recheck stored native semantic receipts against original descriptors.
        for c in row["cases"]:
            obj.from_uper(bytes.fromhex(c["wire_hex"]))
            if pack(carrier_projection(obj, obj.get_val())) != c["normalized"]:
                raise ValueError("native semantic receipt mismatch")
        qualify.root_for_emit[row["module"], row["root"]] = obj
        header = a.generated / "include" / e["header"]
        text = public_emit(row, header, e)
        name = f'root{row["index"]:03}'
        expected_cases[name] = [{"case": i, "wire_hex": c["wire_hex"], "semantic_bits": c["semantic_bits"]} for i,c in enumerate(row["cases"])]
        (source / (name + ".cpp")).write_text(text)
        cmake += [
            "add_executable(" + name + " " + name + ".cpp)",
            "target_link_libraries(" + name + " PRIVATE NRForge::rrc)",
            "target_compile_options("
            + name
            + " PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic-errors --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384)",
            "set_target_properties(" + name + " PROPERTIES CXX_EXTENSIONS OFF)",
            "add_test(NAME " + name + " COMMAND " + name + ")",
        ]
    (source / "CMakeLists.txt").write_text("\n".join(cmake) + "\n")
    (work / "preparation.json").write_text(
        json.dumps(
            dict(
                bound_inputs_sha256=bound_before,
                native_module_sha256=qualify.sha(work / "native.py"),
                expected_cases=expected_cases,
                public_quota_scope=["depth", "work_bits", "input_octets"],
                real_roots=85,
                real_cases=248,
                consumer_files={p.name: qualify.sha(p) for p in source.iterdir()},
                qualification_sha256=qualify.sha(
                    REPO / "tools/rrc-wire-expanded/qualification.json"
                ),
            ),
            indent=2,
        )
        + "\n"
    )
    authenticate(a.asn1_root, authority)
    assert all(qualify.sha(Path(p)) == h for p,h in bound_before.items()), "preparation input drift"
    print("PASS prepared 85 public RRC root consumers")


if __name__ == "__main__":
    main()
