#!/usr/bin/env python3
"""Bind the finite sealed installed RRC public model grammar, without ASN.1 input."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1048576), b""):
            h.update(block)
    return h.hexdigest()


def unique(pairs):
    out = {}
    for key, value in pairs:
        if key in out:
            raise ValueError("duplicate JSON key: " + key)
        out[key] = value
    return out


def read_json(path):
    return json.loads(path.read_text(), object_pairs_hook=unique)


def installed(prefix, expected=None):
    include = prefix / "include/nrforge/rrc"
    metadata = prefix / "share/nrforge-rrc"
    provenance = read_json(metadata / "sdk-provenance.json")
    manifest = read_json(metadata / "manifest.json")
    identity = provenance["identity"]
    if identity["version"] != "0.1.0" or manifest["protocol"] != "RRC":
        raise ValueError("unsupported RRC SDK")
    for key in ("fingerprint", "schema_sha256", "runtime_sha256"):
        if not re.fullmatch(r"[a-f0-9]{64}", identity[key]):
            raise ValueError("invalid SDK identity")
    if expected is not None and expected != identity["fingerprint"]:
        raise ValueError("SDK fingerprint differs from CMake package")
    rows = manifest["roots"]
    supported = [r for r in rows if r["status"] == "PASS"]
    if (len(rows), len(supported)) != (85, 64):
        raise ValueError("finite root closure")
    if any(
        manifest[k] != v
        for k, v in {
            "requested_roots": 85,
            "supported_roots": 64,
            "blocked_roots": 21,
            "channel_roots": 7,
            "selected_supported_roots": 59,
            "full_rrc_coverage": False,
        }.items()
    ):
        raise ValueError("unsupported RRC profile")
    keys = [(r["module"], r["root"]) for r in rows]
    if len(set(keys)) != 85 or len({r["index"] for r in rows}) != 85:
        raise ValueError("duplicate root identity")
    if manifest["qualification_sha256"] != provenance["qualification_sha256"]:
        raise ValueError("qualification identity mismatch")
    generated = provenance["generated_inputs"]
    manifest_name = "manifest.json"
    if sha(metadata / manifest_name) != generated[manifest_name]:
        raise ValueError("manifest hash mismatch")
    header_names = {"runtime.hpp", "rrc.hpp", "sdk_version.hpp"}
    for row in supported:
        module = row["module"].replace("-", "_")
        root = row["root"].replace("-", "_")
        if not re.fullmatch(r"[A-Za-z_]\w*", module + root):
            raise ValueError("unsafe root identity")
        rel = f"roots/{module}/{root}.hpp"
        if row["header"] != "nrforge/rrc/" + rel:
            raise ValueError("unexpected root include")
        if row["namespace"] != "nrforge::rrc::" + module + "::" + root:
            raise ValueError("unexpected root namespace")
        header_names.add(rel)
        if row["installed_header_sha256"] != generated["include/nrforge/rrc/" + rel]:
            raise ValueError("root/header receipt mismatch")
    actual = {str(p.relative_to(include)) for p in include.rglob("*") if p.is_file()}
    if actual != header_names:
        raise ValueError("public header inventory mismatch")
    hashes = {}
    for name in sorted(header_names):
        path = include / name
        if path.is_symlink() or not path.resolve().is_relative_to(include.resolve()):
            raise ValueError("escaped public header")
        if (
            name != "sdk_version.hpp"
            and sha(path) != generated["include/nrforge/rrc/" + name]
        ):
            raise ValueError("public header hash mismatch: " + name)
        hashes[name] = sha(path)
    version = (include / "sdk_version.hpp").read_text()
    values = ",".join(
        json.dumps(identity[k])
        for k in (
            "version",
            "fingerprint",
            "schema_sha256",
            "runtime_sha256",
            "source_revision",
        )
    )
    expected_version = (
        "#pragma once\n#include <string_view>\nnamespace nrforge::rrc {\n"
        "struct SdkIdentity {std::string_view version,fingerprint,schema_sha256,runtime_sha256,source_revision;};\n"
        "inline constexpr SdkIdentity header_sdk_identity{" + values + "};\n"
        "const SdkIdentity& sdk_identity() noexcept;\nnamespace detail {void sdk_require_"
        + identity["fingerprint"]
        + "() noexcept;\ninline const bool sdk_header_library_guard=[](){sdk_require_"
        + identity["fingerprint"]
        + "();return true;}();}\n}\n"
    )
    if version != expected_version:
        raise ValueError("SDK version header mismatch")
    return manifest, provenance, hashes


def blocks(text):
    out = []
    for match in re.finditer(r"\bstruct (T\d+) \{", text):
        start = pos = match.end()
        depth = 1
        while depth and pos < len(text):
            depth += (text[pos] == "{") - (text[pos] == "}")
            pos += 1
        if depth or text[pos : pos + 1] != ";":
            raise ValueError("unterminated public model")
        out.append((match[1], text[start : pos - 1]))
    if not out or len({n for n, _ in out}) != len(out):
        raise ValueError("missing/duplicate public model")
    return out


def validate_type(typ, known):
    if typ in known or typ in {
        "bool",
        "std::uint64_t",
        "std::int64_t",
        "std::monostate",
        "::nrforge::uper::BitString",
        "::nrforge::uper::Index",
    }:
        return
    m = re.fullmatch(r"std::(optional|vector)<(.+)>", typ)
    if m:
        if m[1] == "vector" and m[2] == "std::byte":
            return
        validate_type(m[2], known)
        return
    raise ValueError("unsupported public type: " + typ)


def models(text):
    raw = blocks(text)
    known = {name for name, _ in raw}
    out = []
    for name, body in raw:
        ending = "bool operator==(const " + name + "&) const = default;"
        if body.count(ending) != 1:
            raise ValueError("public comparison shape")
        body = body.replace(ending, "").strip()
        sources = dict(
            (int(i), json.loads(v))
            for i, v in re.findall(
                r'static constexpr std::string_view field_source(\d+)=("[^"\n]*");',
                body,
            )
        )
        alternatives = dict(
            (int(i), json.loads(v))
            for i, v in re.findall(
                r'static constexpr std::string_view alternative(\d+)=("[^"\n]*");', body
            )
        )
        symbols = dict(
            (int(i), json.loads(v))
            for i, v in re.findall(
                r'static constexpr std::string_view symbol(\d+)=("[^"\n]*");', body
            )
        )
        indexes = dict(
            (int(i), {"extension": ext == "true", "index": int(n)})
            for i, ext, n in re.findall(
                r"static constexpr ::nrforge::uper::Index e(\d+)\{(true|false),(\d+)\};",
                body,
            )
        )
        # Every supported metadata declaration is consumed explicitly.
        metadata = r'static constexpr (?:std::string_view (?:field_source|alternative|symbol)\d+="[^"\n]*"|::nrforge::uper::Index (?:e|wire)\d+\{(?:true|false),\d+\}|std::int64_t number\d+=(?:INT64_C\(-?\d+\)|INT64_MIN));'
        body = re.sub(metadata, "", body).strip()
        if alternatives:
            elements = re.findall(
                r"struct A(\d+) \{ ([^;]+) value\{\}; bool operator==\(const A\d+&\) const = default; \};",
                body,
            )
            for i, typ in elements:
                validate_type(typ, known)
            body = re.sub(
                r"struct A\d+ \{ [^;]+ value\{\}; bool operator==\(const A\d+&\) const = default; \};",
                "",
                body,
            ).strip()
            variant = re.fullmatch(
                r"std::variant<std::monostate((?:,A\d+)+)(,Unknown)?> value;", body
            )
            if (
                not variant
                or len(elements) != len(alternatives)
                or list(alternatives) != list(range(len(elements)))
            ):
                raise ValueError("unsupported CHOICE model")
            if variant[1] != "".join(",A" + i for i, _ in elements):
                raise ValueError("CHOICE storage/index mismatch")
            out.append(
                dict(
                    name=name,
                    kind="choice",
                    alternatives=[
                        dict(tag=alternatives[int(i)], wrapper="A" + i, type=t)
                        for i, t in elements
                    ],
                    extensible=bool(variant[2]),
                )
            )
        elif symbols:
            if body != "::nrforge::uper::Index value{};" or set(symbols) != set(
                indexes
            ):
                raise ValueError("unsupported ENUMERATED model")
            out.append(
                dict(
                    name=name,
                    kind="enum",
                    items=[dict(label=symbols[i], **indexes[i]) for i in symbols],
                )
            )
        elif sources or not body:
            fields = re.findall(r"([^\s]+) (f_\w+)(\{\}| = ([^;]+));", body)
            remainder = re.sub(r"[^\s]+ f_\w+(?:\{\}| = [^;]+);", "", body).strip()
            if (
                remainder
                or len(fields) != len(sources)
                or list(sources) != list(range(len(fields)))
            ):
                raise ValueError("unsupported SEQUENCE model")
            projected = []
            for i, (typ, member, _, default) in enumerate(fields):
                validate_type(typ, known)
                projected.append(
                    dict(
                        name=sources[i],
                        member=member,
                        type=typ,
                        optional=typ.startswith("std::optional<"),
                        default=default or None,
                    )
                )
            if len({f["name"] for f in projected}) != len(projected):
                raise ValueError("duplicate source field")
            out.append(dict(name=name, kind="sequence", fields=projected))
        else:
            m = re.fullmatch(r"([^\s]+) value(?:\{\})?;", body)
            if not m:
                raise ValueError("unsupported scalar/list model")
            validate_type(m[1], known)
            out.append(dict(name=name, kind="scalar", type=m[1]))
    if "T0" not in known or "using Root=T0;" not in text:
        raise ValueError("ordinary root alias mismatch")
    nulls = {d["name"] for d in out if d.get("type") == "std::monostate"}
    for declaration in out:
        for field in declaration.get("fields", []):
            if field["optional"] and field["type"][14:-1] in nulls | {"std::monostate"}:
                raise ValueError("optional NULL presence is not qualified")
    return out


def render(row, declarations):
    namespace = row["namespace"]
    q = lambda name: "::" + namespace + "::" + name

    def typ(t):
        return re.sub(r"\bT\d+\b", lambda m: q(m[0]), t)

    out = [
        '#include "bindings.hpp"',
        "#include <" + row["header"] + ">",
        "namespace nrforge::rrc_python_sdk {",
    ]
    for d in declarations:
        out.append(
            f'template<> struct Convert<{q(d["name"])}> {{ static py::object to(const {q(d["name"])}&,ConversionBudget&); static {q(d["name"])} from(py::handle,ConversionBudget&); }};'
        )
    for d in declarations:
        cpp = q(d["name"])
        out += [
            f"py::object Convert<{cpp}>::to(const {cpp}& v,ConversionBudget& b) {{",
            "auto scope=b.enter();",
            "(void)v;",
        ]
        if d["kind"] == "scalar":
            out.append(f'return Convert<{typ(d["type"])}>::to(v.value,b);')
        elif d["kind"] == "sequence":
            out.append("py::dict out;")
            for f in d["fields"]:
                out.append(
                    f'out[{json.dumps(f["name"])}]=Convert<{typ(f["type"])}>::to(v.{f["member"]},b);'
                )
            out.append("return out;")
        elif d["kind"] == "enum":
            for item in d["items"]:
                out.append(
                    f'if(v.value==::nrforge::uper::Index{{{str(item["extension"]).lower()},{item["index"]}}})return named_label({json.dumps(item["label"])},b);'
                )
            out += [
                'if(!v.value.extension)throw py::value_error("invalid enum root index");',
                'py::dict out;out["extension_index"]=Convert<::std::uint64_t>::to(v.value.index,b);return out;',
            ]
        else:
            for item in d["alternatives"]:
                out.append(
                    f'if(auto p=std::get_if<{cpp}::{item["wrapper"]}>(&v.value)){{py::dict out;out["type"]=named_label({json.dumps(item["tag"])},b);out["value"]=Convert<{typ(item["type"])}>::to(p->value,b);return out;}}'
                )
            if d["extensible"]:
                out.append(
                    f'if(auto p=std::get_if<{q("Unknown")}>(&v.value)){{py::dict out,data;out["type"]=named_label("@unknown",b);data["index"]=Convert<::std::uint64_t>::to(p->index,b);data["payload"]=Convert<std::vector<std::byte>>::to(p->payload,b);out["value"]=data;return out;}}'
                )
            out.append('throw py::value_error("unselected or invalid CHOICE");')
        out += [
            "}",
            f"{cpp} Convert<{cpp}>::from(py::handle value,ConversionBudget& b) {{",
            "auto scope=b.enter();",
            f"{cpp} out{{}};",
        ]
        if d["kind"] == "scalar":
            out.append(f'out.value=Convert<{typ(d["type"])}>::from(value,b);')
        elif d["kind"] == "sequence":
            out += [
                "auto dict=require_dict(value);",
                "check_keys(dict,{"
                + ",".join(json.dumps(f["name"]) for f in d["fields"])
                + "});",
            ]
            for f in d["fields"]:
                key = json.dumps(f["name"])
                expression = f'out.{f["member"]}=Convert<{typ(f["type"])}>::from(required(dict,{key}),b);'
                if f["optional"] or f["default"] is not None:
                    expression = f"if(dict.contains({key})){{{expression}}}"
                out.append(expression)
        elif d["kind"] == "enum":
            out.append(
                "if(PyUnicode_Check(value.ptr())){auto label=read_label(value,b);"
            )
            for item in d["items"]:
                out.append(
                    f'if(label=={json.dumps(item["label"])}){{out.value={{{str(item["extension"]).lower()},{item["index"]}}};return out;}}'
                )
            out += [
                'throw py::value_error("unknown enum label");}',
                'auto dict=require_dict(value);check_keys(dict,{"extension_index"});out.value={true,Convert<::std::uint64_t>::from(required(dict,"extension_index"),b)};',
            ]
        else:
            out += [
                'auto dict=require_dict(value);check_keys(dict,{"type","value"});auto tag=read_label(required(dict,"type"),b);auto item=required(dict,"value");'
            ]
            for a in d["alternatives"]:
                out.append(
                    f'if(tag=={json.dumps(a["tag"])}){{out.value={cpp}::{a["wrapper"]}{{Convert<{typ(a["type"])}>::from(item,b)}};return out;}}'
                )
            if d["extensible"]:
                out.append(
                    f'if(tag=="@unknown"){{auto data=require_dict(item);check_keys(data,{{"index","payload"}});out.value={q("Unknown")}{{Convert<std::uint64_t>::from(required(data,"index"),b),Convert<std::vector<std::byte>>::from(required(data,"payload"),b)}};return out;}}'
                )
            out.append('throw py::value_error("unknown CHOICE alternative");')
        out += ["return out;", "}"]
    index = row["index"]
    out += [
        f"py::dict encode_{index}(py::handle value,::nrforge::uper::Limits l,::nrforge::uper::Completion c,ConversionBudget& b) {{",
        f'auto owned=Convert<{q("Root")}>::from(value,b);',
        f"auto result=[&](){{py::gil_scoped_release release;return ::{namespace}::encode(owned,l,c);}}();",
        "return encoded_result(unwrap(std::move(result)),b);}",
        f"py::dict decode_{index}(std::span<const std::byte> bytes,::nrforge::uper::Limits l,std::optional<std::size_t> n,::nrforge::uper::Completion c,ConversionBudget& b) {{",
        f"auto result=[&](){{py::gil_scoped_release release;return ::{namespace}::decode(bytes,l,n,c);}}();",
        "auto owned=unwrap(std::move(result));py::dict out;",
        f'out["value"]=Convert<{q("Root")}>::to(owned.value,b);',
        'out["semantic_bits"]=py::int_(owned.semantic_bits);return out;}',
        "}",
    ]
    return "\n".join(out) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sdk-prefix", type=Path, required=True)
    p.add_argument("--expected-fingerprint")
    p.add_argument("--output", type=Path)
    p.add_argument("--verify-only", action="store_true")
    a = p.parse_args()
    manifest, provenance, hashes = installed(
        a.sdk_prefix.resolve(), a.expected_fingerprint
    )
    source = Path(__file__).resolve().parent
    producer = {
        str(p.relative_to(source.parent)): sha(p)
        for p in (
            source / "generate_bindings.py",
            source / "conversion.hpp",
            source / "module.cpp",
            source.parent / "CMakeLists.txt",
        )
    }
    seal = dict(
        identity=provenance["identity"],
        public_header_sha256=hashes,
        metadata_sha256={
            n: sha(a.sdk_prefix / "share/nrforge-rrc" / n)
            for n in ("manifest.json", "sdk-provenance.json")
        },
        archive_sha256=sha(a.sdk_prefix / "lib/libnrforge_rrc.a"),
        producer_sha256=producer,
    )
    if a.verify_only:
        if a.output is not None:
            receipt = read_json(a.output / "bindings-receipt.json")
            if receipt["seal"] != seal:
                raise ValueError("binding producer/SDK seal changed")
            files = {
                str(p.relative_to(a.output)): sha(p)
                for p in a.output.rglob("*")
                if p.is_file() and p.name != "bindings-receipt.json"
            }
            if files != receipt["generated_sha256"]:
                raise ValueError("generated binding closure changed")
        print("PASS installed RRC public seal")
        return
    if a.output is None:
        p.error("--output required")
    include = a.sdk_prefix / "include"
    supported = [r for r in manifest["roots"] if r["status"] == "PASS"]
    output = {}
    schemas = {}
    for row in supported:
        text = (include / row["header"]).read_text()
        declarations = models(text)
        schemas[row["module"] + "." + row["root"]] = {
            "root": "T0",
            "types": declarations,
        }
        output[f'root{row["index"]:03}.cpp'] = render(row, declarations)
    header = '#pragma once\n#include "conversion.hpp"\n#include <array>\nnamespace nrforge::rrc_python_sdk {\nstruct Binding {const char* module;const char* root;py::dict(*encode)(py::handle,::nrforge::uper::Limits,::nrforge::uper::Completion,ConversionBudget&);py::dict(*decode)(std::span<const std::byte>,::nrforge::uper::Limits,std::optional<std::size_t>,::nrforge::uper::Completion,ConversionBudget&);};\nconst std::array<Binding,64>& bindings();\n'
    for row in supported:
        i = row["index"]
        header += f"py::dict encode_{i}(py::handle,::nrforge::uper::Limits,::nrforge::uper::Completion,ConversionBudget&);\npy::dict decode_{i}(std::span<const std::byte>,::nrforge::uper::Limits,std::optional<std::size_t>,::nrforge::uper::Completion,ConversionBudget&);\n"
    output["bindings.hpp"] = header + "}\n"
    output["registry.cpp"] = (
        '#include "bindings.hpp"\nnamespace nrforge::rrc_python_sdk {const std::array<Binding,64>& bindings(){static const std::array<Binding,64> all{{\n'
        + "".join(
            f'Binding{{{json.dumps(r["module"])},{json.dumps(r["root"])},&encode_{r["index"]},&decode_{r["index"]}}},\n'
            for r in supported
        )
        + "}};return all;}}\n"
    )
    output["conversion-schema.json"] = (
        json.dumps(
            dict(
                manifest=manifest,
                schemas=schemas,
                identity=provenance["identity"],
                public_header_sha256=hashes,
            ),
            indent=2,
        )
        + "\n"
    )
    output["sources.cmake"] = (
        "set(NRFORGE_RRC_PYTHON_SOURCES\n"
        + "".join(
            ' "${CMAKE_CURRENT_BINARY_DIR}/generated/' + n + '"\n'
            for n in output
            if n.endswith(".cpp")
        )
        + ")\n"
    )
    a.output.mkdir(parents=True, exist_ok=True)
    for name, content in output.items():
        (a.output / name).write_text(content)
    actual = {p.name for p in a.output.iterdir()}
    if actual - set(output) - {"bindings-receipt.json"}:
        raise ValueError("unexpected generated binding files")
    receipt = dict(
        seal=seal, generated_sha256={name: sha(a.output / name) for name in output}
    )
    (a.output / "bindings-receipt.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    )
    print("PASS 64 complete ordinary RRC bindings")


if __name__ == "__main__":
    main()
