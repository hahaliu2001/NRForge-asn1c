#!/usr/bin/env python3
"""Prepare public installed consumers from accepted independent P7 semantics."""
import argparse
import json
from importlib.metadata import version
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-wire-qualification"))
import qualify
from native_values import pack
from scan import authenticate


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("asn1-root", "generated", "work"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    if version("pycrate") != "0.7.11":
        raise ValueError("pycrate0.7.11 required")
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
        (REPO / "tools/rrc-wire-qualification/qualification.json").read_text()
    )
    manifest = json.loads((a.generated / "manifest.json").read_text())
    entries = {r["index"]: r for r in manifest["roots"]}
    source = work / "consumer"
    source.mkdir()
    cmake = [
        "cmake_minimum_required(VERSION 3.20)",
        "project(RRCInstalledConsumer LANGUAGES CXX)",
        "find_package(NRForgeRRC 0.1.0 EXACT CONFIG REQUIRED)",
        "enable_testing()",
    ]
    for row in q["real_cases"]:
        e = entries[row["index"]]
        obj = qualify.root(native, row["module"], row["root"])
        # Recheck stored native semantic receipts against original descriptors.
        for c in row["cases"]:
            obj.from_uper(bytes.fromhex(c["wire_hex"]))
            if pack(obj.get_val()) != c["normalized"]:
                raise ValueError("native semantic receipt mismatch")
        qualify.root_for_emit[row["module"], row["root"]] = obj
        header = a.generated / "include" / e["header"]
        text = qualify.emit(row, header).replace(
            '#include "' + header.name + '"', "#include <" + e["header"] + ">"
        )
        text = text.replace(
            "Writer writer;",
            """auto public_decoded=checked(g::decode(native_wire));
CHECK(public_decoded.value==expected&&public_decoded.semantic_bits==r.cursor_bit());
auto public_encoded=checked(g::encode(expected));
CHECK(public_encoded.octets==native_wire&&public_encoded.semantic_bits==r.cursor_bit()&&public_encoded.encoded_bits==native_wire.size()*8);
for(auto completion:{Completion::contained_bits,Completion::exact}){
 auto contained=checked(g::encode(expected,{},completion));
 CHECK(contained.octets==native_wire&&contained.semantic_bits==r.cursor_bit()&&contained.encoded_bits==r.cursor_bit());
 auto contained_decoded=checked(g::decode(contained.octets,{},contained.encoded_bits,completion));
 CHECK(contained_decoded.value==expected&&contained_decoded.semantic_bits==r.cursor_bit());
}
CHECK(!g::decode(native_wire,{},r.cursor_bit()-1,Completion::contained_bits));
Limits zero_depth;zero_depth.max_depth=0;auto bounded=g::decode(native_wire,zero_depth);
CHECK(!bounded&&bounded.error().code==ErrorCode::resource_limit);
Writer writer;""",
        )
        name = f'root{row["index"]:03}'
        (source / (name + ".cpp")).write_text(text)
        cmake += [
            "add_executable(" + name + " " + name + ".cpp)",
            "target_link_libraries(" + name + " PRIVATE NRForge::rrc)",
            "target_compile_options("
            + name
            + " PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic-errors)",
            "set_target_properties(" + name + " PROPERTIES CXX_EXTENSIONS OFF)",
            "add_test(NAME " + name + " COMMAND " + name + ")",
        ]
    # Real installed SDKs; all four APIs and includes share this translation unit.
    mixed = [
        "#include <nrforge/rrc/roots/NR_RRC_Definitions/MIB.hpp>",
        "#include <nrforge/rrc/rrc.hpp>",
        "#include <nrforge/ngap/pdu.hpp>",
        "#include <nrforge/f1ap/f1ap_pdu.hpp>",
        "#include <nrforge/e1ap/e1ap_pdu.hpp>",
        "#include <iostream>",
        "int main(){",
    ]
    for proto in ("ngap", "f1ap", "e1ap"):
        fixtures = json.loads(
            (
                REPO / ("tools/" + proto + "-sdk-consumer/native-fixtures.json")
            ).read_text()
        )
        # Committed setup examples supply the independent full-PDU wire.
        example = (
            REPO
            / (
                "tools/"
                + proto
                + "-sdk-consumer/"
                + {
                    "ngap": "ng_setup.cpp",
                    "f1ap": "f1_setup.cpp",
                    "e1ap": "e1_setup.cpp",
                }[proto]
            )
        ).read_text()
        import re

        wire = re.search(r"const auto native_wire = (.*);", example)[1]
        mixed += [
            "{ auto wire="
            + wire
            + "; auto d=nrforge::"
            + proto
            + "::decode_"
            + proto
            + "_pdu(wire);if(!d||d.value().kind()!=nrforge::"
            + proto
            + "::PduKind::typed)return 1;",
            "auto e=nrforge::"
            + proto
            + "::encode_"
            + proto
            + "_pdu(d.value());if(!e||e.value().octets!=wire)return 2;",
            "auto registry=nrforge::"
            + proto
            + "::"
            + proto
            + "_registry_state();if(!registry||registry.value().message_count()!="
            + str({"ngap": 131, "f1ap": 158, "e1ap": 72}[proto])
            + ')return 3; if(!d.value().message_info()||d.value().message_info()->message!="'
            + {
                "ngap": "NGSetupRequest",
                "f1ap": "F1SetupRequest",
                "e1ap": "GNB-CU-UP-E1SetupRequest",
            }[proto]
            + '")return 7;}',
        ]
    mixed += [
        "{namespace g=nrforge::rrc::NR_RRC_Definitions::MIB;std::vector<std::byte>b={std::byte{2},std::byte{0},std::byte{8}};auto d=g::decode(b);if(!d||d.value().semantic_bits!=23)return 4;auto e=g::encode(d.value().value);if(!e||e.value().octets!=b)return 5;}",
        'if(nrforge::rrc::supported_roots.size()!=64){return 6;}\nstd::cout<<"FOUR_PROTOCOLS_PASS\\n";}',
    ]
    (source / "mixed.cpp").write_text("\n".join(mixed) + "\n")
    for proto in ("NGAP", "F1AP", "E1AP"):
        cmake += ["find_package(NRForge" + proto + " 0.1.0 EXACT CONFIG REQUIRED)"]
    cmake += [
        "add_executable(mixed mixed.cpp)",
        "target_link_libraries(mixed PRIVATE NRForge::rrc NRForge::ngap NRForge::f1ap NRForge::e1ap)",
        "target_compile_options(mixed PRIVATE -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic-errors)",
        "find_program(NRFORGE_LLD ld.lld REQUIRED)",
        "target_link_options(mixed PRIVATE -fuse-ld=lld -Wl,--threads=1)",
        "set_target_properties(mixed PROPERTIES CXX_EXTENSIONS OFF)",
        "add_test(NAME four_protocols COMMAND mixed)",
    ]
    (source / "CMakeLists.txt").write_text("\n".join(cmake) + "\n")
    (work / "preparation.json").write_text(
        json.dumps(
            dict(
                real_roots=64,
                real_cases=143,
                consumer_files={p.name: qualify.sha(p) for p in source.iterdir()},
                qualification_sha256=qualify.sha(
                    REPO / "tools/rrc-wire-qualification/qualification.json"
                ),
            ),
            indent=2,
        )
        + "\n"
    )
    authenticate(a.asn1_root, authority)
    print("PASS prepared 64 public root consumers and four-protocol TU")


if __name__ == "__main__":
    main()
