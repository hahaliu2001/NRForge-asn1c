#!/usr/bin/env python3
"""Read-only P10 blocker study: authenticated fresh probes and conservative shapes."""
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/rrc-readiness"))
from scan import authenticate

BASE = "6cf38c3e6d8ab09d8298214b182eb8757c7aab80"
FAMILIES = {
    "singleton_integer": [
        "DL-CCCH-Message",
        "RRCReconfiguration",
        "RRCSetup",
        "SystemInformation",
    ],
    "constrained_collection_element": [
        "BCCH-DL-SCH-Message",
        "LoggedMeasurementConfiguration-r16",
        "SIB1",
        "SidelinkUEInformationNR-r16",
        "UEAssistanceInformation",
    ],
    "opaque_containing_context": [
        "DL-DCCH-Message",
        "RRCReconfigurationComplete",
        "RRCRelease",
        "RRCResume",
        "RRCResumeComplete",
        "UL-DCCH-Message",
    ],
    "constructed_alias": ["UEInformationResponse-r16"],
    "parameterized_choice_tags": [
        "LocationMeasurementIndication",
        "RRCReestablishment",
        "RRCReconfigurationSidelink",
        "RemoteUEInformationSidelink-r17",
        "SCCH-Message",
    ],
}
SEEDS = [
    "SRB-Identity-v1700",
    "MCCH-RepetitionPeriodAndOffset-r17",
    "SL-InterestedFreqList-r16",
    "PropagationDelayDifference-r17",
    "PCCH-Config",
    "CAG-Config-r18",
    "RRCReconfigurationComplete-v1560-IEs",
    "RRCResume-v1610-IEs",
    "RRCResumeComplete-v1610-IEs",
    "SRS-PosRRC-Inactive-r17",
    "MobilityHistoryReport-r16",
    "VisitedCellInfoList-r16",
    "RadioBearerConfig",
    "MeasConfig",
    "CellGroupConfig",
]
TIER_A = {
    "SIB1",
    "BCCH-DL-SCH-Message",
    "RRCSetup",
    "DL-CCCH-Message",
    "RRCReconfiguration",
    "RRCReconfigurationComplete",
    "RRCRelease",
    "DL-DCCH-Message",
    "UL-DCCH-Message",
}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def digest(v):
    return hashlib.sha256(
        json.dumps(v, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def nodes(d, path=(), parent=None):
    yield d, path, parent
    for i, child in enumerate(d["members"]):
        yield from nodes(
            child, path + (child["name"] or "@element" + str(i),), d["kind"]
        )
    if d["actuals"]:
        yield from nodes(d["actuals"], path + ("@actuals",), d["kind"])


def constraint_kinds(c):
    if c:
        yield c["kind"]
        for child in c["children"]:
            yield from constraint_kinds(child)


def status(r):
    return (
        "BLOCKED_EXTRACTION"
        if r["extract"]
        else "BLOCKED_RENDER" if r["render"] else "PASS"
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    schema = a.asn1_root.resolve()
    w = a.work.resolve()
    w.mkdir(parents=True, exist_ok=False)
    authority = json.loads(
        (REPO / "tools/rrc-readiness/source-manifest.json").read_text()
    )
    authenticate(schema, authority)
    accepted = json.loads(
        (REPO / "tools/rrc-python/verification-summary.json").read_text()
    )
    for n, h in accepted["sdk_source_inputs"].items():
        if sha(REPO / n) != h:
            raise ValueError("producer drift " + n)
    sources = {
        "matrix": "tools/rrc-uper/probe.c",
        "ast": "tools/rrc-readiness/probe.c",
        "graph": "tools/rrc-readiness/root_probe_p4.c",
        "inspect": "tools/rrc-blockers/inspect.c",
    }
    libraries = [
        "tools/developer_tree.o",
        "libasn1typed/.libs/libasn1typed_extract.a",
        "libasn1typed/.libs/libasn1typed.a",
        "libasn1fix/.libs/libasn1fix.a",
        "libasn1parser/.libs/libasn1parser.a",
        "libasn1common/.libs/libasn1common.a",
    ]
    inputs = dict(accepted["sdk_source_inputs"])
    inputs.update({n: sha(REPO / n) for n in sources.values()})
    for name, src in sources.items():
        command = [
            "gcc",
            "-std=gnu11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DHAVE_CONFIG_H",
            *[
                "-I" + str(REPO / d)
                for d in (
                    ".",
                    "libasn1common",
                    "libasn1parser",
                    "libasn1fix",
                    "libasn1typed",
                    "tools",
                )
            ],
            str(REPO / src),
            *[str(REPO / d) for d in libraries],
            "-lm",
            "-o",
            str(w / name),
        ]
        subprocess.run(command, check=True, capture_output=True)
    modules = str(REPO / "tools/rrc-readiness/rrc-rel18.modules")

    def probe(name, args, dest):
        with (w / dest).open("w") as out, (w / (dest + ".stderr")).open("w") as err:
            subprocess.run(
                [str(w / name), modules, str(schema), *map(str, args)],
                stdout=out,
                stderr=err,
                check=True,
            )
        return json.loads((w / dest).read_text())

    ast = probe("ast", [], "ast.json")
    assert ast["parse_fix"] == "PASS"
    fixed = probe("inspect", [], "fixed.json")
    assert len(fixed["declarations"]) == 6 and fixed["read_only"]
    prior = json.loads((REPO / "tools/rrc-uper/readiness-rrc-p6.json").read_text())
    ledger = prior["roots"] + prior["default_seed_roots"]
    assert len(ledger) == 85 and len({(r["module"], r["root"]) for r in ledger}) == 85
    (w / "roots.list").write_text(
        "".join(r["module"] + " " + r["root"] + "\n" for r in ledger)
    )
    (w / "headers").mkdir()
    matrix = probe("matrix", [w / "roots.list", w / "headers"], "matrix.json")
    assert matrix["owned_after_tree_destruction"]
    for i, (r, old) in enumerate(zip(matrix["roots"], ledger, strict=True)):
        assert (r["module"], r["root"], status(r), r["error"]) == (
            old["module"],
            old["root"],
            old["status"],
            old["error"],
        )
        h = w / "headers" / f"r{i:03}.hpp"
        if status(r) == "PASS":
            assert sha(h) == old["header_sha256"]
        else:
            assert not h.exists()
    rendered = [r for r in matrix["roots"] if status(r) == "BLOCKED_RENDER"]
    (w / "graph-roots.list").write_text(
        "".join(r["module"] + " " + r["root"] + "\n" for r in rendered)
    )
    graphs = probe("graph", [w / "graph-roots.list"], "graphs.json")
    assert graphs["owned_after_tree_destruction"]
    (w / "seeds.list").write_text(
        "".join("NR-RRC-Definitions " + n + "\n" for n in SEEDS)
    )
    (w / "seed-headers").mkdir()
    seeds = probe("matrix", [w / "seeds.list", w / "seed-headers"], "seeds.json")
    index = {
        (m["name"], d["name"]): d
        for m in ast["modules"]
        for d in m["declarations"]
        if d["meta"] in (1, 2)
    }
    assert len(index) == 2630
    imports = {
        m["name"]: {i["name"]: i["module"] for i in m["imports"]}
        for m in ast["modules"]
    }
    files = {m["module"]: m["path"] for m in authority["modules"]}

    def locate(module, name):
        path = files[module]
        lines = (schema / path).read_text().splitlines()
        matches = [
            i
            for i, line in enumerate(lines)
            if re.match(r"^" + re.escape(name) + r"\s*(?:\{[^}]*\}\s*)?::=", line)
        ]
        assert len(matches) == 1, (module, name)
        i = matches[0]
        end = i + 1
        while end < len(lines) and end < i + 12 and lines[end].strip():
            end += 1
        return dict(path=path, line=i + 1, excerpt="\n".join(lines[i:end]))

    def resolve(module, name):
        if (module, name) in index:
            return module, name
        imported = imports[module].get(name)
        if imported and (imported, name) in index:
            return imported, name
        return None

    edges = {}
    candidates = []
    unresolved = []
    for (module, name), decl in index.items():
        deps = set()
        for node, path, parent in nodes(decl):
            if node["kind"] == "REFERENCE":
                target = resolve(module, node["reference"])
                if target:
                    deps.add(target)
                elif node["reference"] != "ElementTypeParam":
                    unresolved.append(
                        dict(
                            module=module,
                            owner=name,
                            path=list(path),
                            reference=node["reference"],
                        )
                    )
            kinds = set(constraint_kinds(node["constraint"]))
            family = None
            if node["kind"] == "INTEGER" and "SingleValue" in kinds:
                family = "singleton_integer"
            elif (
                node["kind"] == "SEQUENCE OF"
                and len(node["members"]) == 1
                and node["members"][0]["constraint"]
                and node["members"][0]["kind"]
                not in ("SEQUENCE", "CHOICE", "SEQUENCE OF")
            ):
                family = "constrained_collection_element"
            elif (
                node["kind"] == "OCTET STRING"
                and "ContentsConstraint" in kinds
                and (not path or parent == "CHOICE")
            ):
                family = "opaque_containing_context"
            if family:
                candidates.append(
                    dict(
                        module=module,
                        owner=name,
                        path=list(path),
                        parser_line=node["line"],
                        family=family,
                        kind=node["kind"],
                        constraint_kinds=sorted(kinds),
                    )
                )
        edges[module, name] = deps

    def reachable(root):
        seen = {root}
        todo = deque([root])
        parents = {}
        while todo:
            n = todo.popleft()
            for child in sorted(edges[n]):
                if child not in seen:
                    seen.add(child)
                    parents[child] = n
                    todo.append(child)
        return seen, parents

    candidate_by_owner = {}
    for c in candidates:
        candidate_by_owner.setdefault((c["module"], c["owner"]), []).append(c)
    rows = []
    missing = {}
    for r, g in zip(rendered, graphs["roots"], strict=True):
        assert (r["module"], r["root"]) == (g["module"], g["root"]) and g["rc"] == 0
        types = []
        for t in g["types"]:
            if t["kind"] == 4 and not t["structure"]["valid"]:
                assert t["structure"]["owned"] and t["name"].startswith(
                    "$type$SetupRelease$"
                )
                assert all(
                    a["tag_class"] == 0 and not a["has_root_index"]
                    for a in t["alternatives"]
                )
                item = dict(
                    module=t["module"],
                    instance=t["name"],
                    actuals=t["instance"]["actuals"],
                    alternatives=t["alternatives"],
                    structure=t["structure"],
                )
                types.append(item)
                missing[t["module"], t["name"]] = item
        assert types
        rows.append(
            dict(
                module=r["module"],
                root=r["root"],
                type_count=g["type_count"],
                missing_instances=types,
            )
        )
    setup = next(
        d for d in fixed["declarations"] if d["declaration"]["name"] == "SetupRelease"
    )
    clone_tag_counts = Counter(
        tuple(c["tag_class"] for c in r["clone"]["children"])
        for r in setup["specializations"]
    )
    relevant = []
    for item in sorted(missing.values(), key=lambda i: i["instance"]):
        actual = item["actuals"][0]
        matches = [
            r
            for r in setup["specializations"]
            if len(r["actuals"]["children"]) == 1
            and resolve(
                r["actuals"]["children"][0]["module"],
                r["actuals"]["children"][0]["reference"],
            )
            == (actual["module"], actual["name"])
        ]
        assert len(matches) == 1, actual
        relevant.append(
            dict(
                instance=item["instance"],
                actual=actual,
                fixed_specialization=matches[0],
                fixed_root_tags_available=all(
                    c["tag_class"] != 0 for c in matches[0]["clone"]["children"]
                ),
            )
        )
    assert (
        len(relevant) == 10
        and sum(r["fixed_root_tags_available"] for r in relevant) == 2
    )
    fixed["setup_release_specialization_summary"] = dict(
        total=len(setup["specializations"]),
        tag_classes={str(k): v for k, v in sorted(clone_tag_counts.items())},
        relevant_missing_instances=relevant,
    )
    # Full raw fixed evidence is hash-bound; retain only relevant clones in the report.
    setup["specializations"] = []
    blocked = []
    for r in matrix["roots"]:
        if status(r) == "PASS":
            continue
        family = next(k for k, v in FAMILIES.items() if r["root"] in v)
        seen, parents = reachable((r["module"], r["root"]))
        potential = []
        for owner in sorted(seen):
            for c in candidate_by_owner.get(owner, []):
                chain = [owner]
                while chain[-1] != (r["module"], r["root"]):
                    chain.append(parents[chain[-1]])
                potential.append(
                    dict(
                        c,
                        reference_path=[
                            {"module": m, "name": n} for m, n in reversed(chain)
                        ],
                    )
                )
        blocked.append(
            dict(
                module=r["module"],
                root=r["root"],
                stage=status(r),
                first_error=r["error"],
                first_family=family,
                tier_a_priority=r["root"] in TIER_A,
                source=locate(r["module"], r["root"]),
                static_reachable_declarations=len(seen),
                potential_shape_sites=potential,
            )
        )
    assert Counter(r["first_family"] for r in blocked) == Counter(
        {k: len(v) for k, v in FAMILIES.items()}
    )
    source_seeds = [
        dict(
            module="NR-RRC-Definitions",
            root=n,
            source=locate("NR-RRC-Definitions", n),
            measured_status=status(r),
            first_error=r["error"],
        )
        for n, r in zip(SEEDS, seeds["roots"], strict=True)
    ]
    # Catch input mutation during the run, including generated producer archives.
    authenticate(schema, authority)
    for n, h in inputs.items():
        assert sha(REPO / n) == h, n
    report = dict(
        batch="RRC-P10",
        baseline_commit=BASE,
        status="STUDY_COMPLETE",
        source_authority=authority,
        producer_inputs_sha256=inputs,
        probe_binary_sha256={name: sha(w / name) for name in sources},
        raw_sha256={
            n: sha(w / n)
            for n in (
                "ast.json",
                "matrix.json",
                "graphs.json",
                "seeds.json",
                "fixed.json",
            )
        },
        accepted_p6_matrix_sha256=sha(REPO / "tools/rrc-uper/readiness-rrc-p6.json"),
        accepted_p9_receipt_sha256=sha(
            REPO / "tools/rrc-python/verification-summary.json"
        ),
        matrix=[
            dict(
                module=r["module"],
                root=r["root"],
                status=status(r),
                error=r["error"],
                header_sha256=(
                    sha(w / "headers" / f"r{i:03}.hpp") if status(r) == "PASS" else None
                ),
            )
            for i, r in enumerate(matrix["roots"])
        ],
        summary=dict(
            requested=85,
            supported=64,
            blocked_extraction=16,
            blocked_render=5,
            first_failure_families={k: len(v) for k, v in FAMILIES.items()},
            unchanged_supported_header_hashes=64,
            source_type_declarations=2630,
        ),
        blocked_roots=blocked,
        render_blocked_owned_graphs=rows,
        unique_missing_choice_instances=len(missing),
        named_seed_inspection=source_seeds,
        fixed_constraint_tag_evidence=fixed,
        static_shape_candidates=candidates,
        static_unresolved_references=unresolved,
        limitations=[
            "Each root diagnostic is only its first observed failure.",
            "Static source reference reachability includes all optional/addition/actual types and is conservative, not instantiated typed closure or a predicted unlock count.",
            "Shape candidates are inspection leads; accepted primitive forms can share a shape, and no exhaustive latent blocker inventory is claimed.",
            "Constraint-contained payload references are not traversed; opaque containers remain opaque.",
            "No C++ compile, wire qualification, SDK rebuild, installed consumer test or benchmark is run in this read-only study.",
        ],
        full_rrc_coverage=False,
        codec_changes=False,
        qualification_changes=False,
    )
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if a.check:
        if a.output.read_text() != encoded:
            raise ValueError("study report drift")
    else:
        a.output.write_text(encoded)
    print(
        "PASS read-only RRC-P10 study",
        json.dumps(report["summary"], sort_keys=True),
        "missing choice instances",
        len(missing),
        "static candidates",
        len(candidates),
        "unresolved refs",
        len(unresolved),
    )


if __name__ == "__main__":
    main()
