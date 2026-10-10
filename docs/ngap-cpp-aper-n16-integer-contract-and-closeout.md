# N16 — Bounded INTEGER and effective value intervals

## Authority and baseline

Owner selected N16 and the existing autonomous implementation, independent review, commit/push workflow. Baseline: `fd2b92b6b40e19e1cbdde7757673bcbb6d239d1e`, branch `feature/ngap-cpp-aper-first-message`. Frozen TS 38.413 V18.10.0 source remains unchanged at RAN commit `d6e514a33ec3695925c24aa292900514b371b074`: 131 messages, 81 procedures, roles 81 initiating / 32 successful / 18 unsuccessful.

## Accepted bounded contract

Accept one finite, non-extensible INTEGER interval. Runtime supports every uint64 and int64 interval; owned declarations and effective use-site bounds must fit int64. Use uint64 storage for nonnegative declarations and int64 storage when their lower bound is negative. A narrowed named use-site retains the named type and its storage signedness, even when the effective interval becomes nonnegative. Anonymous INTEGER members own their effective interval. Reject unknown, discontinuous, extensible, unconstrained or inconsistent semantics rather than erase them.

The offset is value minus lower bound, computed through unsigned ordered representations. Full 2^64 cardinality is represented by maximum offset UINT64_MAX; cardinality addition and signed subtraction must not overflow. Effective named constraints are the intersection of individually verified bounded interval terms. Reject disjoint intersections, extension/tail terms, malformed bounds and unrepresented residue. Declared and Fixer-combined anonymous constraints must agree before publication.

APER layouts follow X.691 (2021), clauses 11.5.7 and 13.2:

| Cardinality | Payload |
| --- | --- |
| 1 | No field bits |
| 2–255 | Minimum offset bits, unaligned |
| 256 | Octet-aligned 8-bit offset |
| 257–65536 | Octet-aligned 16-bit offset |
| Above 65536 | Bounded octet-count-minus-one selector, then zero alignment and minimal unsigned offset octets |

Large-range offsets use 1–8 octets; zero uses one octet. Reject spare selector codes, out-of-domain offsets, nonminimal magnitude, nonzero alignment, truncated input and exceeded budgets. Alignment, selector and payload form one atomic primitive; failure preserves cursor/output/counters and replays the first error. Complete encoding padding and empty-field substitution retain the S3 contract. `root_bits` describes offset/cardinality width, not a fixed large-range wire payload width.

Source: https://www.itu.int/rec/T-REC-X.691-202102-I/en

## Implementation and compatibility

New runtime cursor and Field APIs: `read_bounded_uint`, `write_bounded_uint`, `read_bounded_int`, `write_bounded_int`. Historical N1 constrained-uint APIs remain unchanged.

New standalone `asn1typed_render_cpp_owned_integer_*` entry points emit INTEGER types/mapping/codecs. New `asn1typed_render_cpp_owned_value_*` entry points compose INTEGER, BOOLEAN, ENUMERATED, BIT/OCTET STRING, collections, CHOICE and SEQUENCE through owned IR. Named and anonymous SEQUENCE/CHOICE use-site intervals receive distinct mapping/helper bounds. Namespace/symbol preflight occurs before output; failure returns null output and a concrete diagnostic. Exact historical unsigned domains delegate to the previous renderer, preserving existing emitted headers and helper APIs.

Physical IOC generation can lower bounded ordinary inline INTEGER members. Selected IOC primitive INTEGER constraints lack an owned registry range slot and remain refused. No new registry constraint erasure or schema pruning is permitted. Extension and union evidence stays owned where already representable, but its codec generation remains refused.

## Primitive reference evidence

`tools/n16-integer-qualification/qualification-summary.json` records 928 runtime encodes and 928 decodes across 16 profiles and all initial bit residues 0–7. Of these, 926 have exact equality with both an independent bit model and asn1tools 0.167.0, including native decoding. The two constant standalone fields have an explicitly recorded native empty-output versus runtime complete `00` substitution difference. This is primitive evidence, not new complete NGAP qualification. Input fingerprints match the accepted runtime and reference tool sources.

## Validation and review

Runtime `make check`: 7/7. Generated/core/extraction coverage includes single interval boundaries and cardinalities, signed extrema, named/anonymous use-sites, OPTIONAL/CHOICE, deterministic output after Parser destruction, namespace/symbol/refusal boundaries and allocation-failure sweeps. Public APIs also link from C++. Strict C11 renderer and strict C++20 generated-code checks pass. Focused runtime, owned extraction and generated-code ASan/UBSan checks pass. LeakSanitizer was actually attempted but failed with a ptrace/proc attachment restriction; no usable LSan result is claimed. Legacy parser/fixer was not comprehensively sanitizer-instrumented. Independent semantic/evidence review PASS; no unresolved blocker. Typed `make check`: 32/32; tools `make check`: 1/1. Distribution copies of all17 new runtime, test and reference artifacts match current sources. No remaining blocking finding.

## Readiness and next milestone

Accepted full131 scan: physical extraction 122 PASS / 9 FAIL; BODY generation 68 PASS / 54 FAIL / 9 NOT_RUN; strict compilation 68 PASS / 0 FAIL / 63 NOT_RUN. Compared with N15, all63 previous PASS messages retain byte-identical types/mapping/codec headers. New strict BODY PASS messages:

- OverloadStart
- PDUSessionResourceReleaseCommand
- UEContextResumeResponse
- UEContextSuspendResponse
- WriteReplaceWarningRequest

Source authority, 131-message inventory and all26 scan input fingerprints were reconciled against the unchanged source snapshot. Complete-PDU wire qualification remains the historical bounded N11 UEContextReleaseCommand result only.

Remaining generation first-failure clusters:23 compound shape/storage/metadata,17 unsupported INTEGER intervals,11 physical references and3 use-site SIZE metadata. Physical failures remain1 extensible SIZE additions case,7 NULL-related alternatives and1 private-IE key. These are observed first failures, not predicted unlock counts. N16 does not erase extension/union INTEGER semantics to increase coverage.

Recommend N17: inspect and lower the remaining shared compound/reference shapes through a separately scoped owned-IR contract. Extensible INTEGER, NULL, remaining SIZE additions and private-IE semantics need distinct evidence and representation decisions. No benchmark, full adversarial scan, new complete-message wire qualification or general NGAP interoperability claim is included. Stop after N16.
