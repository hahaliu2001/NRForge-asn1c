#!/usr/bin/env python3
"""RRC-P2: compare all accepted APER BODY/available envelope header hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--asn1-root", type=Path, required=True)
    p.add_argument("--probe", type=Path, required=True)
    p.add_argument("--work", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    profiles = [
        ("F1AP", "tools/f1ap-readiness/readiness-f1-p3.json", "tools/f1ap-readiness/f1ap-rel18.modules"),
        ("NGAP", "tools/f1ap-readiness/ngap-regression-f1-p3.json", "tools/qualification/ngap-rel18.modules"),
        ("E1AP", "tools/e1ap-readiness/readiness-e1-p2.json", "tools/e1ap-readiness/e1ap-rel18.modules"),
    ]
    a.work.mkdir(parents=True, exist_ok=False)
    inputs = {path: digest(Path(path)) for path in (
        "libasn1typed/asn1typed_extract.c", "libasn1typed/asn1typed_extract.h",
        "tools/asn1typed_codec_coverage.c", "tools/rrc-readiness/check_aper_regression.py")}
    results = {}
    for profile, evidence, modules in profiles:
        baseline = json.loads(Path(evidence).read_text())
        inputs[evidence] = digest(Path(evidence))
        inputs[modules] = digest(Path(modules))
        for source in baseline["source_authority"]["modules"]:
            data = (a.asn1_root / source["path"]).read_bytes()
            if (hashlib.sha256(data).hexdigest() != source["sha256"] or
                    hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest() != source["git_blob"]):
                raise ValueError("frozen APER source drift")
        work = a.work / profile
        work.mkdir()
        headers = work / "headers"
        headers.mkdir()
        messages = work / "messages.list"
        messages.write_text("".join(m["message"] + "\n" for m in baseline["messages"]))
        with (work / "probe.json").open("wb") as out, (work / "probe.stderr").open("wb") as err:
            subprocess.run([str(a.probe.resolve()), str(Path(modules).resolve()), str(a.asn1_root.resolve()),
                            str(messages.resolve()), str(headers.resolve()), profile + "-PDU-Contents",
                            profile + "-PDU-Descriptions", profile + "-PDU", "--envelopes"],
                           stdout=out, stderr=err, check=True)
        current = json.loads((work / "probe.json").read_text())
        if current["parse"] != "PASS" or current["fix"] != "PASS" or not current["parser_deleted_before_generation"]:
            raise ValueError("APER parse/lifetime gate failed")
        compared = 0
        for i, (old, new) in enumerate(zip(baseline["messages"], current["messages"], strict=True)):
            if old["message"] != new["message"] or new["physical_extraction_rc"] or new["envelope_extraction_rc"]:
                raise ValueError("APER extraction regression")
            for key in ("generation", "envelope_generation"):
                if len(new[key]) != 3 or any(g["rc"] for g in new[key]):
                    raise ValueError("APER generation regression")
            for key in ("compile", "envelope_compile"):
                for family, wanted in old.get(key, {}).get("header_sha256", {}).items():
                    if digest(headers / f"{i:03d}_{family}.hpp") != wanted:
                        raise ValueError(f"APER header drift: {profile} {old['message']} {family}")
                    compared += 1
        results[profile] = {"messages": len(current["messages"]), "extraction_and_generation": "PASS",
                            "historical_headers_byte_identical": compared}
        print(profile, results[profile], flush=True)
    for path, expected in inputs.items():
        if digest(Path(path)) != expected:
            raise ValueError("regression input changed during run")
    a.output.write_text(json.dumps({
        "batch": "RRC-P2", "baseline": "21eac8aaba515dce8e03aecb78de3799035dff3c",
        "scope": "All 361 APER message BODY and target-envelope extraction/generation; historical BODY hashes and F1AP/E1AP envelope hashes",
        "profiles": results, "input_sha256": inputs,
        "limitations": ["NGAP historical evidence has BODY hashes only; NGAP envelopes generated but no historical byte comparison",
                        "No new all-message C++ compile, wire qualification or SDK rebuild"]
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
