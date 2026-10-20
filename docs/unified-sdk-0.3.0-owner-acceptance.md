# Unified ASN1 SDK 0.3.0 — Owner Acceptance

- Owner decision: **ACCEPTED**, 2026-10-20 (America/Los_Angeles).
- Independent review: **PASS**; R1–R3 closed, no remaining blockers.
- Execution/source revision: `79b062990beb04ea14face76a7ae1438e717f3e0`.
- Candidate directory: `/home/jie/asn1-unified-wsl-nxFcGZdf/work/local-candidate-10gib-20261020T182317Z`.
- Candidate SHA256: `2468a666ed5f670914328736bad68e3965e1cc68479c9e8b3521c3a87b84c93e`.
- Acceptance JSON: `/home/jie/asn1-unified-wsl-nxFcGZdf/work/local-candidate-10gib-20261020T182317Z-acceptance.json`.

## Accepted scope

Acceptance applies to the finite qualification documented in this candidate:
NGAP outer regression (131 messages, 12 tests, three NG Setup cases),
143 Transfer vectors, the recorded four-protocol coexistence check and
the existing finite SDK evidence. F1AP/E1AP qualification remains finite;
this is not a claim of full protocol coverage.

The wheel was freshly built under a 10 GiB guard, installed into a fresh
venv and tested. Independent review verified the artifact and evidence hashes.

## Source disclosure — O1

The qualification snapshot uses
`b26e206f6b86ffc21dbebdfe70ee5b129393b72d` as a historical baseline,
plus hash-bound fixes to `python/ngap_sdk/generate_bindings.py` and
`tools/ngap-transfer-sdk/installed_check.py` corresponding to `06c825bb`.
It is not an unmodified checkout of the baseline. Exact execution bytes
were independently verified.

## Nonblocking observations

- O2: Documentation describes a narrower difference-registration policy
  than the 15 actual registered differences.
- O3: Some guard consistency conditions were manually verified by the
  reviewer rather than enforced by `check_guard`.
- O4: Tests do not exercise every input-audit, installation and report
  failure path through the complete recovery workflow.
- O5: The producer's historical `resources` field describes SDK production
  under 6 GiB; the accepted wheel recovery used a 10 GiB guard.

These observations are disclosed with this acceptance; they are not
represented as fixed.

## State at acceptance recording

Publication: **NOT_RUN**. RAN/5GC migration: **NOT_RUN**.
This record does not execute publication, migration, retirement or merge.
The accepted candidate files remain unchanged.
