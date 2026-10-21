# Unified ASN1 SDK 0.3.0 retirement inventory

This is the reviewed pre-delete inventory for the Owner-authorized retirement
of assets superseded by `asn1-sdk-v0.3.0`. The inventory is exact-ID based; no
release, tag, branch, Git history, or current 0.3.0 asset is a target.

## Replacement and gates

- Candidate SHA-256: `2468a666ed5f670914328736bad68e3965e1cc68479c9e8b3521c3a87b84c93e`.
- Published replacement: `hahaliu2001/NRForge-asn1c`, release `418742565`, tag
  `asn1-sdk-v0.3.0`.
- Acceptance evidence: the candidate-side acceptance JSON, with independent
  review `PASS` and Owner acceptance `ACCEPTED`.
- RAN migration: commit
  `89d1c34e8fd8e091131577ecc2b20ade8eea54eb`; its lock selects the 0.3.0 SDK
  archive SHA-256 `86403de2270c51ae17487733a3c1a91e5dd238c45d617d65c9a00b1e537c7d46`.
- 5GC migration: commit
  `af368e22c20071eb7e6150c977fb6a0556b17b27`; its lock selects the 0.3.0 wheel
  SHA-256 `f9ac87b4263fd3f8460906dd623bea58e499a1120417669e6b3304aa8a2de831`.

The 5GC migration record explicitly remains bounded: it adds no NGAP wire
adapter or recursive Transfer-root/carrier decoding, and its TUN-related tests
remain environment-gated/skipped where applicable. This retirement does not
upgrade those conclusions.

## Delete candidates

The two exact rows, including size, update time and downloaded-byte SHA-256,
are recorded in `unified-sdk-0.3.0-retirement-plan.json` and are passed to
`tools/unified-sdk/release.py retire`.

| Repository | Release/tag | Asset ID | Name | Size | SHA-256 |
|---|---|---:|---|---:|---|
| `hahaliu2001/NRForge-RAN` | `411867507` / `nrforge-asn1-linux-x86_64-gcc13-477c5affaee9-v1` | `637544628` | `nrforge-asn1-linux-x86_64-gcc13.tar.zst` | 387215990 | `701f2dd4d524b05772335c8b31c67c94c1f0733c279c6ccf3554ddfae42a9508` |
| `hahaliu2001/NRForge-5GC` | `415526784` / `ngap-python-sdk-0.1.0-cp312-linux-x86_64-r1` | `649257906` | `nrforge_ngap-0.1.0-cp312-cp312-linux_x86_64.whl` | 33813868 | `0d304917fc885f0bf1b3d0976b4b88ac42d7b1e1a2b6df45eeabac2d33fae9e3` |

## Retain list

- All eight assets in `asn1-sdk-v0.3.0`, including both 0.3.0 binaries,
  `candidate.json`, producer/qualification/recovery evidence and both locks.
- RAN asset `637544626`, the old bundle's `.sha256` sidecar, retained as a
  historical integrity record; it is not itself an SDK binary and is not an
  acceptance or source artifact.
- All six non-wheel assets in the 5GC 0.1.0 release: installed receipts,
  layout/package metadata, `release.json`, `wheel.json` and `SHA256SUMS`.
  They are retained as historical qualification, provenance, review and source
  inventory evidence and to preserve rollback records.
- Any other release/tag/asset not listed above, including any asset whose
  current installation or CI use is unclear.

## Persistent evidence

Before deleting binaries, the evidence remains available in the published
0.3.0 release and the historical 5GC release. The candidate-side
`publication.json`, acceptance JSON, qualification receipt and recovery
evidence are retained at the supplied candidate path; this repository records
their identity and the exact remote release/asset IDs above. No large binary is
added to Git.
