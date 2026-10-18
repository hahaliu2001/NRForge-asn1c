# M6.003 standalone Transfer APER qualification

The oracle, receive policy and error mapping were frozen by Owner on
2026-10-17 America/Los_Angeles. The tool is implemented, but its actual native
build and qualification run remain **NOT_RUN** until executed in terminal.
No accepted SDK/wheel, Python binding or AMF implementation is changed.

## Run on native Linux

Prerequisites: Python >=3.11 with venv, pip, Git, Autoconf/Automake/Libtool,
Bison/Flex, make and a C++20 compiler. Use the committed feature branch and
preserve any unrelated tracked edits before running. No root privilege needed.
The RAN repository only supplies `git show` of the fixed schema commit; its
worktree and branch are neither read nor modified. Oracle wheel download
requires PyPI access; no sibling SDK prefix or installed nrforge_ngap is used.

```bash
cd /home/jie/jie_github/NRForge-asn1c
git pull --ff-only origin feature/ngap-transfer-owned-roots
bash tools/m6003-transfer-qualification/run.sh
```

Optional explicit inputs:

```bash
bash tools/m6003-transfer-qualification/run.sh \
  --ran-repo /home/jie/jie_github/NRForge-RAN \
  --output "$HOME/m6003-transfer-wire-run-1" --cxx g++-13 --jobs 2
```

The output directory must not exist. Default is a unique timestamped directory
under HOME. Each subprocess records argv/cwd/timeout, stdin where applicable,
stdout/stderr and status. Failure/interruption stops the campaign and preserves
all evidence. No retry, deletion or expected-result rewrite occurs. A new run
uses a new output directory. Long qualification has a two-hour outer deadline;
individual builds default to 15 minutes. Progress includes the active command.

The bootstrap exports tracked HEAD (excluding prior verification binaries and
an unrelated CodeQL workspace link), preserves safe internal source symlinks,
records source inventory, exports and hashes the six schema modules, builds
common/parser/fix before typed to avoid the known build-order issue, and builds
the unchanged root probe. A fresh venv installs only the approved hash-locked
pycrate wheel. It never installs anything into the project venv.

## Independent checks

Oracle: pycrate 0.7.11, commit c97546fda8d585e28347fe21bf629d496b2e6c5c.
Wheel SHA-256:
2f4169b132aad12693c628fe4a373d8ad35e52f9e8289edf8c7c30d1ed2386b3.
All six unchanged module texts are independently compiled by pycrate after
module order, Git blob and SHA-256 verification against the existing manifest.
No bundled precompiled NGAP schema fallback is allowed.

The four root triples are freshly generated with determinism checks from the
same verified schema. The existing semantic bridge constructs C++ objects and
asserts every oracle-defined field, optional presence, choice, list entry,
criticality and unknown payload. It does not encode/decode PER. Each root has
one strictly compiled standalone consumer linked directly with exported runtime
source, not the old installed SDK. Oracle encode -> native semantic assertions
and native encode -> oracle semantic assertions are separate operations;
canonical byte equality is an additional check, not the only proof.

Coverage includes minimum values, independent tunnel/TEID variations, all D2
ARP label combinations and rate edges, non-R1 optional/CHOICE values, reordered,
missing and duplicate IEs, known criticality mismatches, known and unknown
extension containers, raw unknown retention/refused encoding, owned input
lifetime, all short prefixes of the first oracle seed, trailing bytes, and an
explicit terminal-padding mutation. Oracle-rejected single-bit mutations are
recorded independently, with native syntax/resource classifications preserved.
They are finite malformed examples, not exhaustive constraint fuzzing.

Each root has oracle-generated exact 4096/4097 input boundary vectors. Native
output caps are exercised on every encodable root case, and a known TLContainer
provides exact 4096/4097 output vectors for SetupRequest. These large vectors are
valid ASN.1; rejecting the oversized ones is policy resource_limit. Collection
and retained-unknown byte budgets have independent refusal checks.

A documented mutation of oracle-produced empty ReleaseResponse tests unknown
SEQUENCE addition index/bitmap/payload retention and encode refusal, with an
independent oracle decode and unsupported_profile projection. This is a receiver
mutation, not an oracle-encoded golden vector. The known upstream pycrate suffix
API issue is not repaired or normalized: inability to confirm the mutation's
payload stops the run. Other roots' unknown SEQUENCE suffixes are not claimed
as separately qualified by that one example.

The receive ledger is in RECEIVE_LEDGER.md and policy.py; the run exports a
reachable schema-field/registry inventory. Policy is checked only after native
semantic assertions passed for the same oracle value. This harness does not
provide a production adapter or prove UE/run/session correlation.

## Results and review boundary

Inspect `summary.json`, `SUMMARY.txt`, `campaign/report.json`, `vectors.json`,
`inputs.json`, `oracle-schema.json`, `receive-ledger.json`, per-root consumers,
generated headers, command logs and mutation manifests. Values and hashes are
actual outputs; no PASS is checked into this directory in advance. A PASS means
this finite native campaign passed, pending independent review and Owner
acceptance. It does not qualify Python binding, installed SDK relocation, outer
131-message regression, live RAN/SCTP, M6.003 completion or a new release.
Retain failed evidence; do not commit binaries/venvs or bulk run directories.
