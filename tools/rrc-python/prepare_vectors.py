#!/usr/bin/env python3
"""Project immutable P7 native semantics through public model metadata only."""
import argparse
import copy
import hashlib
import importlib.util
from importlib.metadata import version
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-wire-qualification"))
import qualify
from native_values import build, pack, restore
from scan import authenticate


def project(obj, value, typ, declarations):
    if typ.startswith("std::optional<"):
        inner = typ[14:-1]
        if value is None:
            return None
        null = inner == "std::monostate" or (
            inner in declarations
            and declarations[inner].get("type") == "std::monostate"
        )
        if null:
            return {"present": True}
        return project(obj, value, inner, declarations)
    if not typ.startswith("T"):
        if obj.TYPE == "NULL":
            return None
        if obj.TYPE == "BIT STRING":
            n, bits = value
            return dict(
                octets=(n << ((-bits) % 8)).to_bytes((bits + 7) // 8, "big"),
                bit_count=bits,
            )
        if typ.startswith("std::vector<") and typ != "std::vector<std::byte>":
            return [project(obj._cont, v, typ[12:-1], declarations) for v in value]
        return value
    model = declarations[typ]
    if model["kind"] == "scalar":
        return project(obj, value, model["type"], declarations)
    if model["kind"] == "enum":
        return (
            dict(extension_index=int(value[5:])) if value.startswith("_ext_") else value
        )
    if model["kind"] == "choice":
        tag, item = value
        if tag.startswith("_ext_"):
            return dict(type="@unknown", value=dict(index=int(tag[5:]), payload=item))
        alternative = next(a for a in model["alternatives"] if a["tag"] == tag)
        return dict(
            type=tag,
            value=project(obj._cont[tag], item, alternative["type"], declarations),
        )
    result = {}
    for field in model["fields"]:
        name = field["name"]
        if name.startswith("@group"):
            members = obj._ext_nest[int(name[6:])]
            group_id = next(
                i for i, names in obj._ext_group.items() if names == members
            )
            desc = obj._ext_group_obj[group_id]
            item = {n: value[n] for n in members if n in value}
            if not item and field["optional"]:
                item = None
        else:
            desc = obj._cont[name]
            item = value.get(name, desc._def)
            if name not in value and field["optional"]:
                item = None
            elif item is None and not field["optional"]:
                raise ValueError("missing mandatory native field: " + name)
        result[name] = project(desc, item, field["type"], declarations)
    return result


def present_optional(obj, value, target, depth=0):
    """Construct one native NULL/false presence case, including absent containers."""
    if depth > 16:
        return None
    if obj.TYPE == target and obj._ext is not None:
        if target == "ENUMERATED":
            return "_ext_1000"
        if target == "CHOICE":
            return "_ext_1000", b"\xa5\x5a"
    if obj.TYPE == "SEQUENCE":
        optional = set(obj._root_opt or []) | set(obj._ext or [])
        for name, child in obj._cont.items():
            if name in optional and child.TYPE == target:
                out = copy.deepcopy(value)
                out[name] = 0 if target == "NULL" else False
                return out
        for name, child in obj._cont.items():
            try:
                item = value[name] if name in value else build(child)
                changed = present_optional(child, item, target, depth + 1)
            except (ValueError, KeyError):
                continue
            if changed is not None:
                out = copy.deepcopy(value)
                out[name] = changed
                return out
    elif obj.TYPE == "CHOICE":
        for tag in obj._root:
            try:
                item = value[1] if value[0] == tag else build(obj._cont[tag])
                changed = present_optional(obj._cont[tag], item, target, depth + 1)
            except (ValueError, KeyError):
                continue
            if changed is not None:
                return tag, changed
    elif obj.TYPE == "SEQUENCE OF" and value:
        changed = present_optional(obj._cont, value[0], target, depth + 1)
        if changed is not None:
            return [changed, *copy.deepcopy(value[1:])]
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("asn1-root", "sdk-prefix", "work", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    a = p.parse_args()
    if version("pycrate") != "0.7.11":
        raise ValueError("pycrate 0.7.11 required")
    a.work.mkdir(parents=True, exist_ok=False)
    authority = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(a.asn1_root, authority)
    native = qualify.native_compile(
        [(a.asn1_root / m["path"]).read_text() for m in authority["modules"]],
        a.work.resolve() / "native.py",
    )
    path = REPO / "python/rrc/bindings/generate_bindings.py"
    spec = importlib.util.spec_from_file_location("rrc_public_models", path)
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    manifest, provenance, headers = generator.installed(a.sdk_prefix)
    entries = {r["index"]: r for r in manifest["roots"]}
    qpath = REPO / "tools/rrc-wire-qualification/qualification.json"
    q = json.loads(qpath.read_text())
    if (
        hashlib.sha256(qpath.read_bytes()).hexdigest()
        != manifest["qualification_sha256"]
    ):
        raise ValueError("immutable qualification identity mismatch")
    cases = []
    supplemental = {}
    for row in q["real_cases"]:
        entry = entries[row["index"]]
        desc = qualify.root(native, row["module"], row["root"])
        models = {
            d["name"]: d
            for d in generator.models(
                (a.sdk_prefix / "include" / entry["header"]).read_text()
            )
        }
        for c in row["cases"]:
            desc.from_uper(bytes.fromhex(c["wire_hex"]))
            if pack(desc.get_val()) != c["normalized"]:
                raise ValueError("native semantic receipt mismatch")
            cases.append(
                dict(
                    module=row["module"],
                    root=row["root"],
                    id=c["id"],
                    wire_hex=c["wire_hex"],
                    semantic_bits=c["semantic_bits"],
                    value=pack(project(desc, restore(c["value"]), "T0", models)),
                    expected=pack(project(desc, desc.get_val(), "T0", models)),
                )
            )
        for target in ("BOOLEAN", "ENUMERATED", "CHOICE"):
            if target in supplemental:
                continue
            variant = present_optional(desc, restore(row["cases"][0]["value"]), target)
            if variant is None:
                continue
            try:
                wire, bits, normalized = qualify.oracle(desc, variant)
            except Exception:
                continue
            supplemental[target] = dict(
                module=row["module"],
                root=row["root"],
                id="optional-" + target.lower(),
                wire_hex=wire.hex(),
                semantic_bits=bits,
                value=pack(project(desc, variant, "T0", models)),
                expected=pack(project(desc, normalized, "T0", models)),
            )
    if len(cases) != 143 or len({(c["module"], c["root"]) for c in cases}) != 64:
        raise ValueError("finite case closure")
    result = dict(
        qualification_sha256=manifest["qualification_sha256"],
        source_authority=authority,
        public_header_sha256=headers,
        sdk_identity=provenance["identity"],
        cases=cases,
        supplemental=list(supplemental.values()),
        scope="P7 integration projection; native semantic receipts verified; metadata only",
    )
    a.output.write_text(json.dumps(result, indent=2) + "\n")
    if set(supplemental) != {"BOOLEAN", "ENUMERATED", "CHOICE"}:
        raise ValueError("native optional presence coverage missing")
    print("PASS 143 independently projected real cases, 64 roots")


if __name__ == "__main__":
    main()
