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
    default_seeds = sorted(("NR-RRC-Definitions", x) for x in
        ("FilterConfig", "FilterConfigCLI-r16", "QuantityConfigUTRA-FDD-r16", "QuantityConfigNR", "Q-OffsetRangeList"))
    requested = selected + default_seeds
    a.work.mkdir(parents=True, exist_ok=True)
    root_list = a.work / "roots.list"
    root_list.write_text("".join(f"{m}\t{n}\n" for m, n in requested))
    raw_path = a.work / "ordinary-graphs.json"
    with raw_path.open("wb") as out:
        subprocess.run([str(a.probe.resolve()), str(HERE / "rrc-rel18.modules"),
                        str(a.asn1_root.resolve()), str(root_list)], stdout=out, check=True)
    raw = raw_path.read_bytes()
    result = json.loads(raw)
    if result["parse_fix"] != "PASS" or not result["owned_after_tree_destruction"]:
        raise ValueError("probe lifetime/parse gate failed")
    if [(r["module"], r["root"]) for r in result["roots"]] != requested:
        raise ValueError("probe root list drift")
    for row in result["roots"]:
        key = row["module"], row["root"]
        row["roles"] = [role for role, keys in (("channel", channels), ("payload", payloads), ("bare_seed", bare), ("default_seed", default_seeds)) if key in keys]
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
                for actual in t["instance"]["actuals"]:
                    if (actual["module"], actual["name"]) not in identities:
                        raise ValueError("ordinary actual type closure incomplete")
                for field in t["fields"]:
                    if (field["presence"] == 3) != (field["default"]["kind"] != 0):
                        raise ValueError("DEFAULT evidence inconsistent")
                refs = [f["ref"] for f in t["fields"] if not f["inline_enum"]]
                refs += [x["ref"] for x in t["alternatives"] if not x["inline_enum"]]
                if t["element"]["name"]:
                    refs.append(t["element"])
                for ref in refs:
                    if (ref["name"] and (ref["module"], ref["name"]) not in identities):
                        raise ValueError("ordinary graph closure incomplete")
                    if ref["actual_count"]:
                        target = next(t for t in row["types"] if (t["module"], t["name"]) == (ref["module"], ref["name"]))
                        if target["instance"] != ref or any(a["kind"] != 1 for a in ref["actuals"]):
                            raise ValueError("ordinary specialization identity mismatch")
    matrix = [r for r in result["roots"] if (r["module"], r["root"]) in selected]
    seeds = [r for r in result["roots"] if "default_seed" in r["roles"]]
    if any(r["rc"] for r in seeds):
        raise ValueError("frozen DEFAULT seed closure failed")
    defaults = {(t["module"],t["name"],f["name"]): f["default"]
                for r in seeds for t in r["types"] for f in t["fields"] if f["presence"] == 3}
    if len(defaults) != 13 or any(d["kind"] != 3 or (d["enum_symbol"],d["number"]) not in (("dB0",15),("fc4",4)) for d in defaults.values()):
        raise ValueError("frozen DEFAULT seed semantics failed")
    report = {
        "batch": "RRC-P3", "baseline": "e7b5b95ee70200cf110923c9262cd27cc1c01d08",
        "source_commit": manifest["commit"], "combined_sha256": manifest["combined_sha256"],
        "limits": {"max_types": 4096, "max_references": 65536, "max_ast_nodes": 262144, "ast_depth": 128},
        "parse_fix": result["parse_fix"], "owned_after_tree_destruction": True,
        "raw_owned_graphs_sha256": hashlib.sha256(raw).hexdigest(),
        "summary": dict(Counter(r["status"] for r in matrix)),
        "role_summary": {role: dict(Counter(r["status"] for r in matrix if role in r["roles"]))
                         for role in ("channel", "payload", "bare_seed")},
        "selected_root_count": 80, "roots": matrix, "default_seed_roots": seeds,
        "default_seed_summary": {"roots": 5, "unique_default_fields": len(defaults), "status": "PASS"},
        "cpp_generation": "NOT_RUN", "cpp_compile": "NOT_RUN", "uper_wire_qualification": "NOT_RUN"
    }
    output = HERE / "readiness-rrc-p3.json"
    content = json.dumps(report, indent=2) + "\n"
    if a.check:
        if output.read_text() != content:
            raise ValueError("P3 evidence drift")
    else:
        output.write_text(content)
    print(json.dumps({"summary": report["summary"], "roles": report["role_summary"]}, sort_keys=True))


if __name__ == "__main__":
    main()
