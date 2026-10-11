# F1-P6 independent review — ACCEPT

Reviewer: independent agent `/root/f1p6_review`, read-only review of this
candidate on `feature/f1ap-cpp-aper`, baseline `6234562884c6e1d509193484d1b00347c8039eee`.
This file records the independently supplied review; it is not a new wire claim.

## Verdict

ACCEPT. No remaining technical blocking findings.

## Independently checked

- Current implementation hashes in verification-summary.json.
- Entire inventories and hashes of both actual wheels; source-distribution
  staging receipt and exact shared source/test agreement.
- Installed package and extension bytes against those inspected wheels.
- Isolated `/tmp` F1AP consumer rerun: 9/9 PASS, covering all 4,728 inherited
  finite bytes, registry, populated setup semantics and explicit construction,
  receive-only unknown data, fourth-root policy, limits/errors, concurrency
  and both protocol import orders.
- ELF dependencies: system libraries only; no SDK shared dependency or RPATH.
- Both SDK public-header seals and fail-closed declaration handling.
- Current generated C++ agrees exactly with independently recomputed render
  for all 158 F1AP and 131 NGAP messages.

## Findings resolved during review

1. Source-distribution generator tests originally used a repository-specific
   ancestor. They now locate the shared generator in the actual source tree.
2. Repository-only historical fixture regeneration helper is excluded from the
   source distribution.
3. Canonical forward include checking prevents reverse normalization from
   erasing edits to installed protocol-qualified includes. Both protocol
   negative tests cover this case.

## Evidence boundary

This accepts Python integration over the existing finite C++ SDK contract.
NGAP's original 12 installed consumer methods are recorded passing; the reviewer
independently reran the nine F1AP methods. Source archive inventory was verified,
but a second full clean build from an extracted archive was not run. No live
RAN/vendor interoperability, expanded F1-P4 wire qualification, benchmark,
alternate platform, stable ABI or full native-extension sanitizer is claimed.
Default-linker memory exhaustion was repaired in the local build environment
using serial lld linkage of already compiled objects, without production/schema
changes. Documentation acceptance closeout is recorded before commit.
