#!/usr/bin/env python3
"""Authenticate frozen inputs and record all 80 selected ordinary root gates."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
from scan import HERE, authenticate


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    manifest = json.loads((HERE / "source-manifest.json").read_text())
    authenticate(a.asn1_root, manifest)
    p1 = json.loads((HERE / "readiness.json").read_text())
    channels = {(c["module"], c["root"]) for c in p1["channel_roots"]}
    payloads = {tuple(x.split(".", 1)) for x in p1["distinct_channel_payloads"]}
    bare = {("NR-RRC-Definitions", x) for x in ("MIB", "SIB1", "SIB2")}
    bare.add(("NR-InterNodeDefinitions", "MeasurementTimingConfiguration"))
    selected = sorted(channels | payloads | bare)
    if len(selected) != 80:
        raise ValueError("selected root reconciliation failed")
    a.work.mkdir(parents=True, exist_ok=True)
    root_list = a.work / "roots.list"
    root_list.write_text("".join(f"{m}\t{n}\n" for m, n in selected))
    raw_path = a.work / "ordinary-graphs.json"
    with raw_path.open("wb") as out:
        subprocess.run([str(a.probe.resolve()), str(HERE / "rrc-rel18.modules"),
                        str(a.asn1_root.resolve()), str(root_list)], stdout=out, check=True)
    raw = raw_path.read_bytes()
    result = json.loads(raw)
    if result["parse_fix"] != "PASS" or not result["owned_after_tree_destruction"]:
        raise ValueError("probe lifetime/parse gate failed")
    if [(r["module"], r["root"]) for r in result["roots"]] != selected:
        raise ValueError("probe root list drift")
    for row in result["roots"]:
        key = row["module"], row["root"]
        row["roles"] = [role for role, keys in (("channel", channels), ("payload", payloads), ("bare_seed", bare)) if key in keys]
        row["status"] = "PASS" if row["rc"] == 0 else "FAIL"
        if row["rc"] != 0 and (row["types"] or row["type_count"] or not row["error"]):
            raise ValueError("failure published partial evidence")
        identities = {(t["module"], t["name"]) for t in row["types"]}
        if len(identities) != row["type_count"]:
            raise ValueError("duplicate graph identity")
        if row["rc"] == 0:
            if not row["types"] or (row["types"][0]["module"], row["types"][0]["name"]) != key:
                raise ValueError("root is not types[0]")
            for t in row["types"]:
                refs = [f["ref"] for f in t["fields"] if not f["inline_enum"]]
                refs += [x["ref"] for x in t["alternatives"] if not x["inline_enum"]]
                if t["element"]["name"]:
                    refs.append(t["element"])
                for ref in refs:
                    if ref["actual_count"] or (ref["name"] and (ref["module"], ref["name"]) not in identities):
                        raise ValueError("ordinary graph closure incomplete")
    report = {
        "batch": "RRC-P2", "baseline": "21eac8aaba515dce8e03aecb78de3799035dff3c",
        "source_commit": manifest["commit"], "combined_sha256": manifest["combined_sha256"],
        "limits": {"max_types": 4096, "max_references": 65536, "max_ast_nodes": 262144, "ast_depth": 128},
        "parse_fix": result["parse_fix"], "owned_after_tree_destruction": True,
        "raw_owned_graphs_sha256": hashlib.sha256(raw).hexdigest(),
        "summary": dict(Counter(r["status"] for r in result["roots"])),
        "role_summary": {role: dict(Counter(r["status"] for r in result["roots"] if role in r["roles"]))
                         for role in ("channel", "payload", "bare_seed")},
        "roots": result["roots"],
        "cpp_generation": "NOT_RUN", "cpp_compile": "NOT_RUN", "uper_wire_qualification": "NOT_RUN"
    }
    output = HERE / "readiness-rrc-p2.json"
    content = json.dumps(report, indent=2) + "\n"
    if a.check:
        if output.read_text() != content:
            raise ValueError("P2 evidence drift")
    else:
        output.write_text(content)
    print(json.dumps({"summary": report["summary"], "roles": report["role_summary"]}, sort_keys=True))


if __name__ == "__main__":
    main()
