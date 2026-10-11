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
    lexical_groups = sum(m["source_extension_group_pairs"] for m in p1["modules"])
    # Six NR CHOICE bracket lists use AlternativeTypeLists (individual choices),
    # not ComponentTypeLists (one SEQUENCE extension bitmap slot per group).
    if lexical_groups != 640 or result["source_group_count"] != 634:
        raise ValueError("frozen lexical/SEQUENCE group inventory changed")
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
                structure = t["structure"]
                if structure["owned"]:
                    components = t["fields"] or t["alternatives"]
                    if structure["roots"] + structure["additions"] != len(components):
                        raise ValueError("ordinary component count mismatch")
                    policy = 0 if not t["extensible"] else 1 if t["kind"] == 1 else 2
                    if structure["policy"] != policy:
                        raise ValueError("unknown extension policy mismatch")
                    if bool(structure["valid"]) != structure["rechecked"]:
                        raise ValueError("owned structure failed post-deletion validation")
                    if t["kind"] == 1 and not structure["rechecked"]:
                        raise ValueError("SEQUENCE structure unavailable")
                    for index, f in enumerate(t["fields"]):
                        is_root = index < structure["roots"]
                        if (f["role"] == 0) != is_root or f["role"] not in (0, 1, 2):
                            raise ValueError("field role mismatch")
                        if not f["has_index"] or f["index"] != (index if is_root else index - structure["roots"]):
                            raise ValueError("field outer index mismatch")
                        if f["role"] == 2:
                            group = next(x for x in row["types"] if (x["module"],x["name"]) == (f["ref"]["module"],f["ref"]["name"]))
                            if group["kind"] != 1 or f["presence"] != 1 or not group["structure"]["rechecked"]:
                                raise ValueError("addition group lost its owned SEQUENCE")
                    for index, alt in enumerate(t["alternatives"]):
                        is_root = index < structure["roots"]
                        if bool(alt["addition"]) == is_root:
                            raise ValueError("CHOICE root/addition classification mismatch")
                        if structure["rechecked"]:
                            if is_root:
                                rank = sum((a["tag_class"],a["tag_number"]) < (alt["tag_class"],alt["tag_number"]) for a in t["alternatives"][:structure["roots"]])
                                if not alt["has_root_index"] or alt["root_index"] != rank or alt["has_extension_index"]:
                                    raise ValueError("CHOICE canonical root index mismatch")
                            elif not alt["has_extension_index"] or alt["extension_index"] != index - structure["roots"] or alt["has_root_index"]:
                                raise ValueError("CHOICE declaration-order extension index mismatch")
                        elif alt["has_root_index"] or alt["has_extension_index"]:
                            raise ValueError("unavailable CHOICE evidence published indexes")
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
    previous = json.loads((HERE / "readiness-rrc-p3.json").read_text())
    prior = {(r["module"],r["root"]):r["status"] for r in previous["roots"]}
    if any(r["status"] != "PASS" and prior[(r["module"],r["root"])] == "PASS" for r in matrix):
        raise ValueError("P3 successful closure regressed")
    unique = {}
    for row in result["roots"]:
        for t in row["types"]:
            identity = t["module"], t["name"]
            if identity in unique and unique[identity] != t:
                raise ValueError("same owned identity has inconsistent structure across closures")
            unique[identity] = t
    # Store every body once. Per-root identity lists retain closure membership;
    # the raw probe SHA pins the full post-deletion graphs before normalization.
    for row in result["roots"]:
        row["types"] = [{"module":t["module"], "name":t["name"]} for t in row["types"]]
    report = {
        "batch": "RRC-P4", "baseline": "97d099f4edfb2d463bd564dc5cec8e586a81c8e4",
        "source_commit": manifest["commit"], "combined_sha256": manifest["combined_sha256"],
        "limits": {"max_types": 4096, "max_references": 65536, "max_ast_nodes": 262144, "ast_depth": 128},
        "parse_fix": result["parse_fix"], "owned_after_tree_destruction": True,
        "raw_owned_graphs_sha256": hashlib.sha256(raw).hexdigest(),
        "summary": dict(Counter(r["status"] for r in matrix)),
        "role_summary": {role: dict(Counter(r["status"] for r in matrix if role in r["roles"]))
                         for role in ("channel", "payload", "bare_seed")},
        "selected_root_count": 80, "roots": matrix, "default_seed_roots": seeds,
        "owned_type_inventory": [unique[k] for k in sorted(unique)],
        "default_seed_summary": {"roots": 5, "unique_default_fields": len(defaults), "status": "PASS"},
        "new_pass_roots": [f'{r["module"]}.{r["root"]}' for r in matrix if r["status"] == "PASS" and prior[(r["module"],r["root"])] == "FAIL"],
        "extension_evidence": {
            "source_sequence_groups": result["source_group_count"],
            "source_lexical_bracket_pairs": lexical_groups,
            "source_choice_bracket_lists": 6,
            "unique_extensible_sequences": sum(t["structure"]["owned"] and t["kind"] == 1 and t["extensible"] for t in unique.values()),
            "unique_addition_groups": len({(t["module"],t["name"],f["name"]) for t in unique.values() for f in t["fields"] if f["role"] == 2}),
            "unique_extensible_choices": sum(t["structure"]["owned"] and t["alternatives"] != [] and t["extensible"] for t in unique.values()),
            "unavailable_choice_mappings": [f'{t["module"]}.{t["name"]}' for t in unique.values() if t["alternatives"] and not t["structure"]["rechecked"]],
            "unknown_sequence_policy": "skip unknown open additions after framing validation (runtime NOT_RUN)",
            "unknown_choice_policy": "preserve unknown extension index and opaque payload (runtime NOT_RUN)"
        },
        "cpp_generation": "NOT_RUN", "cpp_compile": "NOT_RUN", "uper_wire_qualification": "NOT_RUN"
    }
    output = HERE / "readiness-rrc-p4.json"
    content = json.dumps(report, indent=2) + "\n"
    if a.check:
        if output.read_text() != content:
            raise ValueError("P4 evidence drift")
    else:
        output.write_text(content)
    print(json.dumps({"summary": report["summary"], "roles": report["role_summary"]}, sort_keys=True))


if __name__ == "__main__":
    main()
