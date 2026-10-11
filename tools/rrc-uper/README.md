# RRC-P6 compound generation

Build/configure the repository first. This is a developer generation/check path,
not an installed SDK. Frozen ASN.1 inputs are supplied externally and authenticated
against `tools/rrc-readiness/source-manifest.json`.

```sh
python3 tools/rrc-uper/check.py --work /tmp/p6-focused
ASAN_OPTIONS=detect_leaks=0 python3 tools/rrc-uper/check.py --work /tmp/p6-asan --sanitize
python3 tools/rrc-uper/generate.py --asn1-root /path/to/schema \
  --probe /tmp/p6-focused/probe --work /tmp/p6-matrix \
  --output /tmp/readiness-rrc-p6.json
```

Work directories must be fresh. The generator accounts for 80 selected roots and
five DEFAULT seeds; each successful header is compiled with strict C++20 flags.
The four inherited bare fixtures are either decoded/reencoded or explicitly
blocked. Frozen source authentication is repeated after generation.

The checked-in matrix is 59/80 selected PASS, with 16 extraction gaps and five
CHOICE evidence gaps. Five extra DEFAULT roots pass. Full independent wire
qualification remains P7 and installed SDK sealing remains P8. See the P6 contract
and verification/review records. No benchmark is run by these tools.

Optional exploratory reference check (pinned asn1tools 0.167.0, pycrate 0.7.11):

```sh
python3 tools/rrc-uper/observe_oracles.py --work /tmp/p6-oracles \
  --output /tmp/oracle-observation.json
```

This records four byte matches and one explicit-tag disagreement. It does not
mark independent wire qualification PASS; P7 must resolve the documented gate.
