#!/usr/bin/env python3
"""Authenticate, generate and seal the finite installed RRC C++ SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def digest(v):
    return hashlib.sha256(
        json.dumps(v, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def run(command, log):
    with Path(log).open("w") as stream:
        subprocess.run(
            list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT, check=True
        )


def identity_name(s):
    result = s.replace("-", "_")
    if not re.fullmatch("[A-Za-z_][A-Za-z0-9_]*", result):
        raise ValueError("unsafe ASN.1 public identity")
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("asn1-root", "work", "prefix"):
        p.add_argument("--" + key, type=Path, required=True)
    p.add_argument("--jobs", type=int, default=1)
    p.add_argument("--generate-only", action="store_true")
    a = p.parse_args()
    if not 1 <= a.jobs <= 4:
        raise ValueError("jobs must be 1..4")
    a.work.mkdir(parents=True, exist_ok=False)
    work = a.work.resolve()
    prefix = a.prefix.resolve()
    if prefix.exists() and any(prefix.iterdir()):
        raise ValueError("prefix must be new or empty")
    authority = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(a.asn1_root, authority)
    sealed = json.loads((REPO / "tools/rrc-null-open/readiness.json").read_text())["matrix"]
    qpath = REPO / "tools/rrc-wire-expanded/qualification.json"
    source_paths = [
        REPO / "libuper/runtime.hpp",
        REPO / "libuper/runtime.cpp",
        *sorted((REPO / "libasn1typed").glob("asn1typed*.[ch]")),
        REPO / "tools/developer_tree.c",
        REPO / "tools/developer_tree.h",
        REPO / "tools/rrc-uper/probe.c",
        REPO / "tools/rrc-uper/generate.py",
        REPO / "tools/rrc-readiness/scan.py",
        REPO / "tools/rrc-readiness/rrc-rel18.modules",
        REPO / "tools/rrc-readiness/source-manifest.json",
        REPO / "tools/rrc-null-open/readiness.json",
        qpath,
        Path(__file__).resolve(),
        REPO / "tools/rrc-uper/check.py",
        REPO / "cmake/NrforgeRrcSdkExpanded.cmake",
        REPO / "cmake/VerifyNgapSdk.cmake",
        REPO / "cmake/WriteNgapSdkReceipt.cmake",
        REPO / "cmake/NRForgeProtocolConfig.cmake.in",
    ]
    for directory in ("libasn1common", "libasn1parser", "libasn1fix"):
        source_paths.extend(
            p
            for p in sorted((REPO / directory).glob("*"))
            if p.is_file() and p.suffix in (".c", ".h", ".l", ".y")
        )
    source_paths.extend(
        REPO / name
        for name in (
            "tools/developer_tree.o",
            "libasn1typed/.libs/libasn1typed_extract.a",
            "libasn1typed/.libs/libasn1typed.a",
            "libasn1fix/.libs/libasn1fix.a",
            "libasn1parser/.libs/libasn1parser.a",
            "libasn1common/.libs/libasn1common.a",
        )
    )
    sources = {str(p.relative_to(REPO)): sha(p) for p in source_paths}
    qualification = json.loads(qpath.read_text())
    if (
        qualification["status"] != "FINITE_PROFILE_PASS"
        or qualification["counts"]["qualified_real_roots"] != 85
    ):
        raise ValueError("P16 finite qualification gate")
    if qualification["source_authority"] != authority:
        raise ValueError("P16 source authority drift")
    if {(r["module"], r["root"]) for r in qualification["real_cases"]} != {
        (r["module"], r["root"])
        for r in sealed["roots"]
        if r["status"] == "PASS"
    }:
        raise ValueError("P16/current qualified identity closure drift")
    # Fresh controller build and authenticated full matrix preserve the accepted
    # codec bytes; installation changes only namespace/includes/API linkage.
    # Regenerate all headers from the current linked producer, then require
    # exact P16-qualified bytes; qualification itself is immutable.
    rawdir = work / "raw"
    rawdir.mkdir()
    probe = work / "probe"
    command = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-DHAVE_CONFIG_H"]
    command += ["-I" + str(REPO / d) for d in (".", "libasn1common", "libasn1parser", "libasn1fix", "libasn1typed", "tools")]
    command += [str(REPO / "tools/rrc-uper/probe.c"), str(REPO / "tools/developer_tree.o")]
    command += [str(REPO / n) for n in ("libasn1typed/.libs/libasn1typed_extract.a", "libasn1typed/.libs/libasn1typed.a", "libasn1fix/.libs/libasn1fix.a", "libasn1parser/.libs/libasn1parser.a", "libasn1common/.libs/libasn1common.a")]
    command += ["-lm", "-o", str(probe)]
    run(command, work / "probe-build.log")
    rows = sealed["roots"]
    roots = work / "roots.list"
    roots.write_text("".join(r["module"] + "\t" + r["root"] + "\n" for r in rows))
    raw = subprocess.check_output([str(probe), str(REPO / "tools/rrc-readiness/rrc-rel18.modules"), str(a.asn1_root.resolve()), str(roots), str(rawdir)])
    result = json.loads(raw)
    if not result["owned_after_tree_destruction"] or len(result["roots"]) != 85:
        raise ValueError("owned current matrix closure")
    for i, (r, expected) in enumerate(zip(result["roots"], rows)):
        if (r["module"], r["root"]) != (expected["module"], expected["root"]) or r["extract"] or r["render"] or sha(rawdir / f"r{i:03}.hpp") != expected["header_sha256"]:
            raise ValueError("P16 qualified generation drift")
    (work / "matrix.json").write_text(json.dumps(result, indent=2) + "\n")
    g = work / "generated"
    inc = g / "include/nrforge/rrc"
    inc.mkdir(parents=True)
    entries = []
    namespaces = set()
    for i, r in enumerate(rows):
        entry = dict(
            index=i,
            module=r["module"],
            root=r["root"],
            roles=r["roles"],
            status=r["status"],
        )
        if r["status"] != "PASS":
            entry["error"] = r["error"]
            entries.append(entry)
            continue
        ns = (
            "nrforge::rrc::"
            + identity_name(r["module"])
            + "::"
            + identity_name(r["root"])
        )
        if ns in namespaces:
            raise ValueError("public namespace collision")
        namespaces.add(ns)
        rel = (
            "roots/"
            + identity_name(r["module"])
            + "/"
            + identity_name(r["root"])
            + ".hpp"
        )
        raw = (work / "raw" / f"r{i:03}.hpp").read_text()
        assert sha(work / "raw" / f"r{i:03}.hpp") == r["header_sha256"]
        text = raw.replace(
            "#include <uper/runtime.hpp>", "#include <nrforge/rrc/runtime.hpp>"
        ).replace(
            "namespace nrforge_generated::r" + f"{i:03}" + " {",
            "namespace " + ns + " {",
        )
        # Qualified algorithm bodies are kept verbatim behind the wrappers.
        text = text.replace(
            " encode(const Root&", " encode_unchecked(const Root&"
        ).replace(" decode(std::span", " decode_unchecked(std::span")
        text = (
            text.rsplit("} // namespace", 1)[0]
            + """::nrforge::uper::Result<::nrforge::uper::CompleteEncoding> encode(const Root&,::nrforge::uper::Limits={},::nrforge::uper::Completion=::nrforge::uper::Completion::octets);
::nrforge::uper::Result<Decoded> decode(std::span<const std::byte>,::nrforge::uper::Limits={},std::optional<std::size_t>={},::nrforge::uper::Completion=::nrforge::uper::Completion::octets);
} // namespace
""".replace(
                "size_t>={}", "size_t> = {}"
            )
        )
        target = inc / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
        (g / f"r{i:03}.cpp").write_text(
            "#include <nrforge/rrc/"
            + rel
            + ">\nnamespace "
            + ns
            + """ {
::nrforge::uper::Result<::nrforge::uper::CompleteEncoding> encode(const Root& v,::nrforge::uper::Limits l,::nrforge::uper::Completion c){return encode_unchecked(v,l,c);}
::nrforge::uper::Result<Decoded> decode(std::span<const std::byte>b,::nrforge::uper::Limits l,std::optional<std::size_t>n,::nrforge::uper::Completion c){return decode_unchecked(b,l,n,c);}
}
namespace nrforge::rrc::detail {void root_anchor_"""
            + str(i)
            + "() noexcept {auto volatile encode_ref=&"
            + ns
            + "::encode;auto volatile decode_ref=&"
            + ns
            + "::decode;if(!encode_ref||!decode_ref)std::terminate();}}\n"
        )
        entry.update(
            header="nrforge/rrc/" + rel,
            namespace=ns,
            qualified_header_sha256=r["header_sha256"],
            installed_header_sha256=sha(target),
        )
        entries.append(entry)
    manifest = dict(
        version="0.2.0",
        protocol="RRC",
        requested_roots=85,
        supported_roots=85,
        selected_supported_roots=80,
        blocked_roots=0,
        channel_roots=12,
        full_rrc_coverage=False,
        qualification_sha256=sha(qpath),
        roots=entries,
    )
    (g / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    runtime = (REPO / "libuper/runtime.hpp").read_text()
    (inc / "runtime.hpp").write_text(
        "#include <nrforge/rrc/sdk_version.hpp>\n" + runtime
    )
    (g / "runtime.cpp").write_text(
        (REPO / "libuper/runtime.cpp")
        .read_text()
        .replace('#include "runtime.hpp"', "#include <nrforge/rrc/runtime.hpp>")
    )
    supported = [e for e in entries if e["status"] == "PASS"]
    catalog = (
        """#pragma once
#include <nrforge/rrc/runtime.hpp>
#include <array>
#include <string_view>
namespace nrforge::rrc {
struct RootInfo {std::string_view module,name,header;};
inline constexpr std::array<RootInfo,85> supported_roots{{
"""
        + "".join(
            "RootInfo{"
            + ",".join(json.dumps(e[k]) for k in ("module", "root", "header"))
            + "},\n"
            for e in supported
        )
        + """}};
}
"""
    )
    (inc / "rrc.hpp").write_text(catalog)
    cmake = (
        """cmake_minimum_required(VERSION 3.20)
project(nrforge_rrc LANGUAGES CXX)
set(NRFORGE_SOURCE_ROOT "" CACHE PATH "Source SDK root")
include("${NRFORGE_SOURCE_ROOT}/cmake/NrforgeRrcSdkExpanded.cmake")
nrforge_verify_sdk_lock()
add_library(nrforge_rrc STATIC runtime.cpp sdk_identity.cpp
"""
        + "".join(f' r{e["index"]:03}.cpp\n' for e in supported)
        + """)
nrforge_package_rrc_sdk(nrforge_rrc)
"""
    )
    (g / "CMakeLists.txt").write_text(cmake)
    if {str(p.relative_to(REPO)): sha(p) for p in source_paths} != sources:
        raise ValueError("producer inputs changed during generation")
    inputs = {
        str(p.relative_to(g)): sha(p) for p in sorted(g.rglob("*")) if p.is_file()
    }
    fingerprint = digest(
        dict(version="0.2.0", sources=sources, generated=inputs, schema=authority)
    )
    identity = dict(
        version="0.2.0",
        fingerprint=fingerprint,
        schema_sha256=authority["combined_sha256"],
        runtime_sha256=digest(
            {
                str(p.relative_to(REPO)): sha(p)
                for p in (REPO / "libuper/runtime.hpp", REPO / "libuper/runtime.cpp")
            }
        ),
        source_revision=subprocess.check_output(
            ["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True
        ).strip(),
    )
    symbol = "sdk_require_" + fingerprint
    (inc / "sdk_version.hpp").write_text(
        """#pragma once
#include <string_view>
namespace nrforge::rrc {
struct SdkIdentity {std::string_view version,fingerprint,schema_sha256,runtime_sha256,source_revision;};
inline constexpr SdkIdentity header_sdk_identity{"""
        + ",".join(json.dumps(v) for v in identity.values())
        + """};
const SdkIdentity& sdk_identity() noexcept;
namespace detail {void """
        + symbol
        + """() noexcept;
inline const bool sdk_header_library_guard=[](){"""
        + symbol
        + """();return true;}();}
}
"""
    )
    (g / "sdk_identity.cpp").write_text(
        "#include <nrforge/rrc/sdk_version.hpp>\nnamespace nrforge::rrc {const SdkIdentity& sdk_identity() noexcept{return header_sdk_identity;}namespace detail {void "
        + symbol
        + "() noexcept{}}}\n"
    )
    (g / "sdk_link_check.cpp").write_text(
        "#include <nrforge/rrc/rrc.hpp>\nnamespace nrforge::rrc::detail {\n"
        + "".join(
            "void root_anchor_" + str(e["index"]) + "() noexcept;\n" for e in supported
        )
        + "}\nint main(){\n"
        + "".join(
            "nrforge::rrc::detail::root_anchor_" + str(e["index"]) + "();\n"
            for e in supported
        )
        + "return nrforge::rrc::supported_roots.size()!=85 || nrforge::rrc::sdk_identity().fingerprint!=nrforge::rrc::header_sdk_identity.fingerprint;}\n"
    )
    provenance = dict(
        identity=identity,
        source_inputs=sources,
        generated_inputs=inputs,
        source_authority=authority,
        qualification_sha256=sha(qpath),
        full_rrc_coverage=False,
        scope="P16 finite real profile; SDK consistency, not authenticity or stable binary ABI",
    )
    (g / "sdk-provenance.json").write_text(
        json.dumps(provenance, indent=2, sort_keys=True) + "\n"
    )
    hashes = {
        str(p.relative_to(g)): sha(p) for p in sorted(g.rglob("*")) if p.is_file()
    }
    lock = []
    for key, val in [("VERSION", "0.2.0"), ("FINGERPRINT", fingerprint)]:
        lock.append(f'set(NRFORGE_SDK_{key} "{val}")')
    for key in ("schema_sha256", "runtime_sha256"):
        lock.append(f'set(NRFORGE_{key.upper()} "{identity[key]}")')
    for scope, mapping in [("SOURCE", sources), ("GENERATED", hashes)]:
        lock += (
            ["set(NRFORGE_" + scope + "_HASHES"]
            + [' "' + k + "|" + v + '"' for k, v in sorted(mapping.items())]
            + [")"]
        )
    (g / "sdk-lock.cmake").write_text("\n".join(lock) + "\n")
    authenticate(a.asn1_root, authority)
    if a.generate_only:
        print("PASS sealed generation " + fingerprint)
        return
    run(
        [
            "cmake",
            "-S",
            g,
            "-B",
            work / "build",
            "-DNRFORGE_SOURCE_ROOT=" + str(REPO),
            "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DCMAKE_CXX_FLAGS=-O0 -Wall -Wextra -Werror -Wconversion -Wsign-conversion -pedantic-errors --param=ggc-min-expand=5 --param=ggc-min-heapsize=16384",
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--threads=1",
        ],
        work / "configure.log",
    )
    run(["cmake", "--build", work / "build", "--parallel", a.jobs], work / "build.log")
    run(["cmake", "--install", work / "build"], work / "install.log")
    authenticate(a.asn1_root, authority)
    if {str(p.relative_to(REPO)): sha(p) for p in source_paths} != sources:
        raise ValueError("producer inputs changed during build/install")
    print("PASS sealed SDK build/install " + fingerprint)


if __name__ == "__main__":
    main()
