# N7-P2 — Bounded extension framing runtime evidence

2026-10-09. Base: `11961777d080ea571f2192f6b5a59ad662d10378`.

The [framing contract](ngap-cpp-aper-n7-p2-framing-contract.md) was checked against the primary X.691 clauses and independently reviewed **PASS before implementation**. It distinguishes normally-small bitmap lengths from enum indexes, bit-count versus octet-count fragmentation, and empty complete payloads from zero terminal determinants.

This milestone adds runtime primitives only. Generated extensible values, unknown-addition sidecars, known-addition dispatch, opaque encoding and real NGAP message qualification are not implemented here.

## Implementation and review

`BitReader`/`FieldReader` now provide `read_sequence_extension_bitmap()` and `read_open_type_owned()`. `BitWriter`/`FieldWriter` provide sticky `reject_sequence_extension_data()`. `Limits` retains its first three fields and appends the three retention limits; DecodeContext exposes cumulative read-only usage counters.

The implementation scans bounded input in multiple passes instead of allocating fragment descriptors. It establishes availability and total lengths, checks shared budgets, validates alignment and determinant/fragment rules, allocates one owned result, copies contents, then commits cursor/counters. The bitmap result is packed MSB-first with zero unused storage bits. Open payloads concatenate fragments without retaining outer determinants. No nested decoder or fresh complete context inspects opaque bytes.

Independent final implementation review: **PASS, no remaining blockers**. The reviewer independently compiled and ran the persistent suite under strict C++20/NDEBUG, and ran an ASan/UBSan probe covering fragment multipliers, bitmap fragmentation, ownership, sticky errors and rollback after prior success. One test-only finished-writer offset expectation was corrected from 0 to 8, matching the existing empty-complete-encoding substitution; production behavior was unchanged.

## Verification

| Check | Result |
| --- | --- |
| Runtime suite | 2/2 PASS |
| Forced fresh typed suite, including generated codecs against current runtime | 14/14 PASS |
| Strict C++20/NDEBUG with Werror, pedantic, conversion/sign-conversion/shadow | Compile and run PASS |
| Literal framing and independent bit builder | All residues 0–7; complete packed bits/payload/cursor/counters agree |
| Bitmap widths 1–65537, sparse/all-present patterns | Short/large/fragmented forms, trailing absent positions and normalized storage PASS |
| Payload lengths 1–131072 | All fragment multipliers, C4 chains, boundaries and exact-multiple zero terminal PASS |
| Invalid selectors, nonminimal lengths/nonmaximal fragments, empty payload/all-absent bitmap | Exact error codes/offsets and atomic rollback PASS |
| Exhaustive small truncations and selected large-fragment boundaries | Availability/error-priority checks PASS |
| Nonzero alignment, late missing/invalid terminators, readable oversized frames | Padding/canonicality/budget priorities PASS |
| Input/wire and all three retention budgets | Exact/one-less and cumulative cross-call boundaries PASS |
| Actual allocation-failure injection | One owned-result allocation; cursor/counters unchanged and no partial result |
| Source mutation/destruction, copies | Retained values independent; deep copies PASS |
| Sticky, finished/moved state, ignored failure and later bad_alloc callbacks | Original error retained; complete wrappers publish no failed result |
| Original three-element Limits aggregate initialization | First three values and appended defaults checked by static_assert |
| Fully instrumented runtime and new focused suite ASan+UBSan | PASS, leak detection disabled after separate failure |
| Actual LSan attempt | Fatal proc/task/ptrace environment restriction; no usable leak verdict |
| Distribution runtime/header/new test source | Byte-identical |
| Diff/new-file whitespace | PASS |

No IR or renderer source changed. This establishes bounded runtime framing, not generated sidecar preservation, external message interoperability or NGAP codec qualification. Large-frame truncation tests sample determinant/payload/terminal boundaries rather than sweeping every prefix quadratically. No benchmark, external differential or full frozen-schema parse was performed. N7-P3 will integrate reviewed structure evidence, owned sidecars, decode retention and root-only encode rejection into generation.

Contract, implementation, persistent tests and this evidence are committed/uploaded after PASS under continuous authority. No branch merge, deletion or force push.
