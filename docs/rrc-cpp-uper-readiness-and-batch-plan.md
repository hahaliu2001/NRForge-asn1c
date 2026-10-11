# RRC-P1 — frozen RRC C++ / UPER readiness and batch plan

Status: **Study complete (2026-10-11)**; independent acceptance is recorded in
`tools/rrc-readiness/review.md`.
Baseline: `3a237c1f283352d6898be708249e1b89dc634dc3`, after accepted E1-P6.
Branch: `feature/f1ap-cpp-aper`; no merge, source normalization or benchmark.
This batch adds a read-only study and reproducible inventory, not a production
codec or SDK. Later batches require their own execution authorization.

## Authority and inherited foundation

RRC source authority is NRForge-RAN commit
`abc850e5461e1b15cf5c81907d0d0fdd5e3a1179`,
[`src/rrc/asn1/README.md`](https://github.com/hahaliu2001/NRForge-RAN/blob/abc850e5461e1b15cf5c81907d0d0fdd5e3a1179/src/rrc/asn1/README.md):
**TS 38.331 V18.10.0, Release 18, June 2026, UPER**.
The earlier E1 source pin `d6e514a33ec3695925c24aa292900514b371b074`
does not contain these RRC sources and cannot serve as their authority.
The six unchanged modules are authenticated by byte length, Git blob and
SHA-256 in `tools/rrc-readiness/source-manifest.json`. In normative module order,
direct concatenation is 1,715,754 bytes with SHA-256
`97a7f13be4c515c9c78e5f752647807621d59568ca33eeea6258513731462d2b`.
No schema is vendored, edited, or re-extracted by this study.

NRForge-RAN's accepted
[`AF.005 foundation`](https://github.com/hahaliu2001/NRForge-RAN/blob/abc850e5461e1b15cf5c81907d0d0fdd5e3a1179/planning/components/AF.005-rrc-asn1-foundation.md)
is already **Frozen**, with all twelve tasks complete. Its
[`raw C baseline`](https://github.com/hahaliu2001/NRForge-RAN/blob/abc850e5461e1b15cf5c81907d0d0fdd5e3a1179/src/rrc/generated/README.md)
uses compiler `58d60f19de362c0216e30c57683a67be8604983d`, explicitly enables
UPER and disables APER, and records 2,703 C files, 2,673 headers and one support
file (48,117,431 bytes; manifest SHA-256
`8b9261aea7f599dface3918b5be5379739b118a5bdee464d4989eb1acf7118df`).
Raw C compilation, sanitizer checks, canonical artifacts, isolated definitions
and four-protocol coexistence are **inherited historical evidence**, not checks
rerun here and not qualification of the new C++ typed path.

The historical 338 SetupRelease specialization notices and this build's 264
fixed template specialization records are different measurements. This study
counts 338 source parameter-actual sites, without asserting equivalence between
compiler notices, deduplicated specializations or generated file counts.

## Measured inventory and gates

The probe loads all six modules once and runs the current parser/fixer.
The scanner authenticates bytes before invoking it, inventories every named
type (AMT_TYPE and AMT_TYPEREF), records imports and every selected channel's
CHOICE path, and reconciles the channel list against all named
`*-MessageType[-rN]` CHOICE declarations. It stops at payload references:
this is not an instantiated ordinary-root dependency closure.

| Module | Named types | Named values | DEFAULT fields | Type-actual sites | AST extension markers | Source `[[...]]` pairs | CONTAINING constraints |
|---|---:|---:|---:|---:|---:|---:|---:|
| NR-RRC-Definitions | 2399 | 436 | 19 | 328 | 666 | 586 | 39 |
| PC5-RRC-Definitions | 75 | 0 | 0 | 8 | 22 | 8 | 4 |
| NR-UE-Variables | 38 | 0 | 0 | 0 | 0 | 0 | 0 |
| NR-Sidelink-Preconf | 9 | 0 | 1 | 0 | 3 | 6 | 0 |
| NR-Sidelink-DiscoveryMessage | 1 | 0 | 0 | 0 | 1 | 1 | 0 |
| NR-InterNodeDefinitions | 108 | 28 | 0 | 2 | 21 | 39 | 26 |
| Total | **2630** | **464** | **20** | **338** | **713** | **640** | **69** |

Feature counts walk source declaration AST members and actual-parameter
subtrees after fixing, excluding specialized clones. Marker counts cover both
constructed and enumeration extension markers; they are not addition counts.
Group counts are lexical token pairs after stripping line comments, not proof
of preserved group wire indexes. Named types and raw C artifact counts have
different denominators. Per-type source lines and nonzero features are retained
in `readiness.json`; anonymous inline types are included in feature counts.

| Channel root | Distinct payload branches |
|---|---:|
| BCCH-BCH-Message | 1 |
| BCCH-DL-SCH-Message | 2 |
| DL-CCCH-Message | 2 |
| DL-DCCH-Message | 13 |
| MCCH-Message-r17 | 1 |
| MulticastMCCH-Message-r18 | 1 |
| PCCH-Message | 1 |
| UL-CCCH-Message | 4 |
| UL-CCCH1-Message | 1 |
| UL-DCCH-Message | 27 |
| SBCCH-SL-BCH-Message (PC5) | 1 |
| SCCH-Message (PC5) | 12 |
| Total | **66** |

All 66 payload references have distinct module-qualified identities. NULL spare
branches and empty messageClassExtension branches remain in the recorded CHOICE
paths, but do not increase this payload count. DedicatedNAS-Message and other
octet containers are not channel roots. UE variables, sidelink preconfiguration,
discovery and inter-node types remain in the whole-schema inventory even when
not reached through a channel. This is a full declaration inventory and a
bounded channel-root study, not a claim of all RRC entry points being supported.

| Existing API / check | PASS | FAIL | NOT_RUN |
|---|---:|---:|---:|
| Six-module parse/fix (one combined tree) | 1 | 0 | 0 |
| Whole-module typed extraction | 1 | 5 | 0 |
| Physical single-container IOC extraction (12 channels + 4 bare values) | 0 | 16 | 0 |
| C++ generation / compile (same 16 roots) | 0 | 0 | 16 |
| New C++ UPER wire qualification (same 16 roots) | 0 | 0 | 16 |

Whole-module NR-UE-Variables succeeds with 61 owned types including inline
materialization. That does not validate its imported references, C++ rendering,
UPER or a self-contained SDK. NR-RRC stops first at
`SetupRelease.measurementIndication: unsupported parameterized target/formal`.
PC5, sidelink preconfiguration, discovery and inter-node extraction stop first
at SEQUENCE components after an extension marker. Exact first failures are
recorded for all gates. They mask later semantic gaps.

Fifteen physical-root probes report an unsupported single-container IOC graph;
the SIB2 probe reports its extension-component failure first. RRC channels use
ordinary SEQUENCE/CHOICE framing, not an IOC procedure envelope. These failures
show an API/profile boundary, not sixteen unsupported ASN.1 messages or sixteen
independent fixes. No renderer or wire operation was invoked.

## Reuse boundary and shared gaps

* Parser/fixer, module-qualified identities, owned type data, diagnostics and
  deterministic rendering conventions are reusable foundations. Public
  `asn1typed_extract_module` is whole-module; the message and target-envelope
  entry points in `asn1typed_extract.h` are IOC-oriented. There is no public
  ordinary root-reachable multi-module graph API. RRC-P2 must prove ownership
  after tree destruction, import resolution, alias handling, actual parameter
  identity and bounded traversal; cycles and concrete reachable closures are
  still unmeasured here.
* SetupRelease is an ordinary parameterized CHOICE (`release NULL`,
  `setup ElementTypeParam`), not an object-set governed IOC substitution.
  `put_parameterized_object_set_ref` cannot establish its semantics.
  Shared specialization support must preserve each actual type and source
  identity without expanding templates by hand or patching the ASN.1.
* `presence_of` rejects DEFAULT explicitly. Default suppression, reconstructed
  values, equality and invalid default evidence need an owned contract;
  accepting DEFAULT as OPTIONAL would change semantics. Examples include
  q-OffsetFreq and sidelink sl-MaxCID-r16.
* Ordinary whole-module extraction rejects fields after a SEQUENCE extension
  marker. Existing APER IOC extension support does not prove ordinary graph
  extension support. Parser grammar `ComponentTypeLists` represents a `[[...]]`
  group as an anonymous optional SEQUENCE. Preserve the group/addition distinction,
  extension order and root counts through IR and rendering; do not flatten it
  into independent outer addition bits. CHOICE indexes, future additions and
  nonCriticalExtension chains need separate explicit treatment.
* CONTAINING constraints identify nested ASN.1 payloads in OCTET STRINGs.
  Preserve the constraint and bit boundary as evidence; default storage should
  remain owned opaque octets. A helper that recursively encodes/decodes contained
  payloads needs an explicit opt-in contract and its own qualification. Never
  interpret a CONTAINING annotation as IOC open-type dispatch.
* The current `libaper/runtime.cpp` bounded-integer layout aligns at maximum
  offset 255, uses aligned 16-bit storage up to 65535, and uses a size selector
  plus aligned octets for larger ranges. `read_constrained_uint(16)` also aligns.
  Existing renderer codec families call APER APIs. An UPER path needs a separate
  explicit policy and entry points; renaming libaper or disabling one alignment
  call is insufficient. Audit INTEGER, ENUMERATED, CHOICE, lengths, OCTET/BIT
  STRING, extension bitmaps/open types and normally-small values under pinned
  X.691 rules before implementing them. Low-level bit I/O, error/budget concepts
  and transactionality may be reused only after their UPER contract is tested.
* Historical RRC ranges reach 39 bits (`0..549755813887`), not UINT64_MAX.
  The E1 full uint64 APER fix is useful representation infrastructure, but does
  not validate UPER 39-bit behavior. Require non-octet offsets, exact semantic
  bit counts, unused-code rejection and explicit terminal-padding handling.

## Frozen wire seeds and bounded qualification

The accepted fixture authority is
[`tests/fixtures/f1/rrc/README.md`](https://github.com/hahaliu2001/NRForge-RAN/blob/abc850e5461e1b15cf5c81907d0d0fdd5e3a1179/tests/fixtures/f1/rrc/README.md).
The four fixture files were fetched at the pinned commit and their hex bytes
rechecked in RRC-P1. `fixtures-manifest.json` records Git blobs, decoded-byte
SHA-256 and semantic bit lengths. Their earlier independent pycrate decoding
is inherited evidence; this study does not perform new independent decoding.

| Bare ASN.1 value | Semantic bits | Stored bytes | Frozen hex |
|---|---:|---:|---|
| MIB | 23 | 3 | `020008` |
| SIB1 | 117 | 15 | `800000082002010000010000040008` |
| SIB2 | 48 | 6 | `000000000000` |
| MeasurementTimingConfiguration | 5 | 1 | `00` |

MIB/SIB1 are not BCCH envelopes. MTC selects
`criticalExtensions.c1.measTimingConf` with both inner optionals absent.
Padding after semantic length is not semantic content. Keep these four frozen
assets unchanged; create separately named envelope and boundary vectors later.
Self round trips and these four seeds alone cannot qualify 66 payloads or all
2,630 declarations. Future qualification must publish a finite value profile,
independent encode/decode agreement, exact bytes and bit counts, nested framing,
negative vectors, budgets and rollback results. Cross-protocol C++ and Python
coexistence is a future SDK gate, not inherited from raw C coexistence.

## Proposed batches and exit gates

| Batch | Authorized scope when invoked | Exit gate |
|---|---|---|
| RRC-P1 | Frozen authority, complete declaration/channel inventory, existing API probes, this plan | Reproducible scan, honest NOT_RUN gates, independent study review |
| RRC-P2 | Ordinary multi-module root graph extraction contract and minimal shared implementation | MIB/MTC and suitable channel roots extracted; imported aliases and owned lifetime proven; all selected roots rerun with exact first-gap ledger; no wire claim |
| RRC-P3 | Ordinary type parameter specialization and DEFAULT ownership | SetupRelease actual identities and default semantics proven with focused shared tests; six-module/root matrix rerun; later gaps retained |
| RRC-P4 | Ordinary SEQUENCE additions/groups and CHOICE wire evidence | Root/addition/group indexes and unknown-addition policy owned after tree destruction; nested groups and future branches tested; no schema patches |
| RRC-P5 | Dedicated UPER runtime contract and primitive implementation | Pin X.691 edition/clauses; bit-offset/range/length/string/enum boundary vectors, independent oracle, malformed inputs, budgets and transactional behavior; APER regressions unchanged |
| RRC-P6 | UPER compound rendering and ordinary channel framing | All 12 channel roots and 66 payload identities accounted for; strict C++20 generation matrix; four bare seeds and auxiliary modules explicitly scoped; unsupported closures block full coverage claim |
| RRC-P7 | Finite independent wire qualification | Frozen seeds plus separately owned channel/compound vectors, exact semantic bits/bytes and independent codecs; negative matrix and explicit qualification limitations |
| RRC-P8 | Installed sealed C++ SDK | Fresh relocated install consumed without generator/schema/build tree; manifest/archives/headers closure, public UPER API and four-protocol coexistence verified |
| RRC-P9 | Installed Python SDK | Wheel/sdist, owner lifetime, bit-aware APIs and isolated installed consumer; APER protocols and RRC imports/coexistence verified; no source-tree fallback |

P2–P4 may need reviewed sub-batches as concrete closures expose further gaps
(including recursive dependencies, constrained aliases or additional primitive
shapes). Reclassify the measured ledger after each change rather than promising
the first fix unlocks every message. Preserve existing APER APIs and sealed
F1AP/NGAP/E1AP deliveries. Each implemented batch gets independent review before
commit/push; no merge and no proactive performance benchmark.

Immediate next step: **RRC-P2**, an ordinary root graph contract and minimal
implementation. UPER runtime work follows owned semantic evidence, not a new
RRC-specific hand-written encoder.
