# M6.003 standalone Transfer APER qualification

This directory is preparation material only. The terminal qualification has
not run. `qualification-contract.json` deliberately marks the oracle pin,
AMF/Simulator receive policy, and native-error mapping as awaiting one Owner
freeze; its proposed values are not approvals.

## Oracle candidate and installation

Recommended reference: pycrate **0.7.11**, Git tag `0.7.11`, commit
`c97546fda8d585e28347fe21bf629d496b2e6c5c`. The official PyPI wheel is
`pycrate-0.7.11-py2.py3-none-any.whl`, SHA-256
`2f4169b132aad12693c628fe4a373d8ad35e52f9e8289edf8c7c30d1ed2386b3`.
The recommendation follows the existing `tools/full-pdu-qualification` and
`tools/n11-envelope-qualification` pin. The hash-locked requirement is for a
dedicated per-run venv and uses `--no-deps`; it does not change the project
environment. Pycrate is an independent ASN.1 APER implementation, not the
tested NRForge codec.

Installation shape for the future terminal entry point:

```sh
python3 -m venv <new-run-directory>/oracle-venv
<new-run-directory>/oracle-venv/bin/python -m pip install \
  --disable-pip-version-check --no-deps --require-hashes \
  -r tools/m6003-transfer-qualification/oracle-requirements.lock
```

## Schema equivalence proof

The authority is TS 38.413 V18.10.0 at NRForge-RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`. Existing
`tools/n11-envelope-qualification/source-manifest.json` pins all six ordered
module Git blobs and SHA-256 values. The current NRForge-RAN checkout was
checked read-only: its six module byte hashes match that manifest, and the
six Git blob IDs at the frozen authority commit also match. This is direct
byte identity, not a filename/release-label inference.

The intended oracle path re-verifies those six identities and passes all six
untouched texts to pycrate `compile_text`, then freshly generates its native
Python module. It must not import a precompiled installed NGAP schema. The
generated Transfer headers are separately fixed by the prior compile/link
evidence in `verification/m6003-003-transfer-cpp-compile-*`; the runtime used
there is `/home/jie/m6003-b1-rerun/tree/libaper`, with its source/archive hashes
captured alongside the four generated triples.

## Approved and pending policy

Owner M6.003 Gate A explicitly approved the 4096-octet per-root input/output
limits and inheritance of the SDK's strict zero terminal-padding behavior.
Both are now registered in NRForge-5GC Matrix §15. Valid messages over 4096
octets are policy `resource_limit`, never reported as ASN.1-invalid. The
zero-padding rule records the SDK's interoperability behavior; it is not a
claim that X.691 requires all decoders to reject nonzero padding.

The general codec's ability to preserve ASN.1 fields and raw bytes is a
separate question from AMF and Simulator acceptance. The full receive ledger
and native-to-application error mapping remain pending in one Owner decision.
Until frozen, the policy-gated rows in `qualification-contract.json` are not
executable acceptance expectations and no qualification PASS may be reported.

The terminal runner, C++ root consumers, oracle vectors and malformed-input
matrix are not yet complete. This intentional gate prevents recommendations
from being treated as approved R1 behavior.
