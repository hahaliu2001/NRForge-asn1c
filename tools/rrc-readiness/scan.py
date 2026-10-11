#!/usr/bin/env python3
"""RRC-P1 inventory; source authentication precedes the read-only probe.

Feature counts are source declaration AST counts, including type actuals but
excluding fixer specialization clones. They do not prove per-root extraction.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
CHANNELS = [
    ("NR-RRC-Definitions", name) for name in (
        "BCCH-BCH-Message", "BCCH-DL-SCH-Message", "DL-CCCH-Message",
        "DL-DCCH-Message", "MCCH-Message-r17", "MulticastMCCH-Message-r18",
        "PCCH-Message", "UL-CCCH-Message", "UL-CCCH1-Message", "UL-DCCH-Message")
] + [("PC5-RRC-Definitions", "SBCCH-SL-BCH-Message"),
     ("PC5-RRC-Definitions", "SCCH-Message")]


def authenticate(root, manifest):
    combined = bytearray()
    expected_list = [m["path"] for m in manifest["modules"]]
    actual_list = (HERE / "rrc-rel18.modules").read_text().splitlines()
    if actual_list != expected_list:
        raise ValueError("ordered module list drift")
    for module in manifest["modules"]:
        data = (root / module["path"]).read_bytes()
        git_blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if (len(data) != module["bytes"] or
                hashlib.sha256(data).hexdigest() != module["sha256"] or
                git_blob != module["git_blob"]):
            raise ValueError("frozen source drift: " + module["path"])
        combined.extend(data)
    if (len(combined) != manifest["combined_bytes"] or
            hashlib.sha256(combined).hexdigest() != manifest["combined_sha256"]):
        raise ValueError("combined source drift")


def walk(node):
    yield node
    for child in node["members"]:
        yield from walk(child)
    if node["actuals"]:
        yield from walk(node["actuals"])


def constraints(node):
    if node:
        yield node["kind"]
        for child in node["children"]:
            yield from constraints(child)


def features(declaration):
    nodes = list(walk(declaration))
    return {
        "nodes": len(nodes),
        "default_fields": sum(n["default"] for n in nodes),
        "optional_fields": sum(n["optional"] for n in nodes),
        "parameter_actual_sites": sum(n["actuals"] is not None for n in nodes),
        "extension_markers": sum(n["kind"] == "EXTENSION_MARKER" for n in nodes),
        "kinds": dict(sorted(Counter(n["kind"] for n in nodes).items())),
        "constraints": dict(sorted(Counter(k for n in nodes for k in constraints(n["constraint"])).items())),
    }


def build_report(ast, manifest, root, raw_sha):
    if ast["parse_fix"] != "PASS":
        raise ValueError("parse/fix failed")
    expected = [m["module"] for m in manifest["modules"]]
    if [m["name"] for m in ast["modules"]] != expected:
        raise ValueError("fixed module identity/order drift")
    modules = {m["name"]: m for m in ast["modules"]}
    index = {(m["name"], d["name"]): d for m in ast["modules"]
             for d in m["declarations"] if d["meta"] in (1, 2)}
    if len(index) != sum(d["meta"] in (1, 2) for m in ast["modules"] for d in m["declarations"]):
        raise ValueError("duplicate type identity")

    def resolve(module, name):
        if (module, name) in index:
            return module, name
        candidates = [i["module"] for i in modules[module]["imports"] if i["name"] == name]
        if len(candidates) != 1 or (candidates[0], name) not in index:
            raise ValueError(f"unresolved channel type: {module}.{name}")
        return candidates[0], name

    channels = []
    for module, name in CHANNELS:
        declaration = index[module, name]
        if declaration["kind"] != "SEQUENCE" or len(declaration["members"]) != 1:
            raise ValueError("channel envelope changed: " + name)
        message = declaration["members"][0]
        if message["name"] != "message" or message["kind"] != "REFERENCE":
            raise ValueError("channel message field changed: " + name)
        identity = resolve(module, message["reference"])
        selector = index[identity]
        if selector["kind"] != "CHOICE":
            raise ValueError("channel selector changed: " + name)
        branches = []

        def choice(node, path):
            if node["kind"] == "CHOICE":
                for child in node["members"]:
                    choice(child, path + [child["name"]])
            else:
                row = {"path": ".".join(path), "kind": node["kind"], "line": node["line"]}
                if node["kind"] == "REFERENCE":
                    target = resolve(identity[0], node["reference"])
                    row["payload"] = ".".join(target)
                branches.append(row)

        choice(selector, [])
        channels.append({"module": module, "root": name, "line": declaration["line"],
                         "selector": ".".join(identity), "branches": branches})
    discovered = {".".join(key) for key, d in index.items()
                  if d["kind"] == "CHOICE" and re.fullmatch(r".*-MessageType(?:-r\d+)?", key[1])}
    if discovered != {c["selector"] for c in channels}:
        raise ValueError("channel selector inventory does not reconcile")
    rows = []
    inventory = []
    for source in manifest["modules"]:
        m = modules[source["module"]]
        types = [d for d in m["declarations"] if d["meta"] in (1, 2)]
        values = [d for d in m["declarations"] if d["meta"] == 3]
        aggregate = features({"members": types, "actuals": None, "kind": "MODULE", "constraint": None,
                              "default": False, "optional": False})
        aggregate["nodes"] -= 1
        aggregate["kinds"].pop("MODULE")
        text = (root / source["path"]).read_text()
        # Only lexical evidence: strip line comments, count paired group tokens.
        text = re.sub(r"--[^\n]*", "", text)
        opens, closes = text.count("[["), text.count("]]")
        if opens != closes:
            raise ValueError("extension group token mismatch")
        rows.append({"module": m["name"], "declarations": len(m["declarations"]),
                     "named_types": len(types), "named_values": len(values),
                     "imports": m["imports"], "features": aggregate,
                     "source_extension_group_pairs": opens,
                     "templates": [{"name": d["name"], "line": d["line"],
                                    "fixed_specializations": d["specializations"]}
                                   for d in types if d["template"]]})
        for d in types:
            f = features(d)
            compact = {k: f[k] for k in ("default_fields", "parameter_actual_sites", "extension_markers") if f[k]}
            if f["constraints"].get("ContentsConstraint"):
                compact["contents_constraints"] = f["constraints"]["ContentsConstraint"]
            inventory.append({"module": m["name"], "name": d["name"], "kind": d["kind"],
                              "line": d["line"], "features": compact})
    payloads = sorted({b["payload"] for c in channels for b in c["branches"] if "payload" in b})
    gates = ast["gates"]
    if len(gates) != 22:
        raise ValueError("gate count drift")
    return {
        "batch": "RRC-P1", "baseline": manifest["asn1c_baseline"],
        "source_commit": manifest["commit"], "combined_sha256": manifest["combined_sha256"],
        "fixed_ast_sha256": raw_sha, "source_authentication": "PASS", "parse_fix": "PASS",
        "method": "All source named types, fixed AST including type actuals; no specialization clones, no instantiated root closure, no generation or wire operation",
        "modules": rows, "channel_roots": channels, "distinct_channel_payloads": payloads,
        "summary": {"named_types": len(index), "channel_roots": len(channels),
                    "payload_branch_occurrences": sum("payload" in b for c in channels for b in c["branches"]),
                    "distinct_channel_payloads": len(payloads),
                    "whole_module_pass": sum(g["rc"] == 0 for g in gates if g["api"] == "whole_module"),
                    "physical_IOC_root_pass": sum(g["rc"] == 0 for g in gates if g["api"] != "whole_module")},
        "gates": gates, "cpp_generation": "NOT_RUN", "cpp_compile": "NOT_RUN",
        "uper_wire_qualification": "NOT_RUN", "type_inventory": inventory,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--ast", type=Path, required=True, help="scratch raw fixed AST output")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--check", action="store_true", help="compare committed evidence without writing it")
    args = p.parse_args()
    manifest = json.loads((HERE / "source-manifest.json").read_text())
    authenticate(args.asn1_root, manifest)
    with args.ast.open("wb") as stream:
        subprocess.run([str(args.probe.resolve()), str(HERE / "rrc-rel18.modules"),
                        str(args.asn1_root.resolve())], stdout=stream, check=True)
    raw = args.ast.read_bytes()
    report = build_report(json.loads(raw), manifest, args.asn1_root, hashlib.sha256(raw).hexdigest())
    content = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if args.output.read_text() != content:
            raise ValueError("readiness evidence drift")
    else:
        args.output.write_text(content)
    print(json.dumps(report["summary"], sort_keys=True))


if __name__ == "__main__":
    main()
