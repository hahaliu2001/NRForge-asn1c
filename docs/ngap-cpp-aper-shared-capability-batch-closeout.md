# N19 and seven shared capabilities: aggregate closeout

2026-10-10. Executed under the owner's continuous authorization:
“那么从1到7一次做完，不用每次都反馈”. Baseline is N18 commit
`c06613a7e9693e68d4ffbea011347fd0cb111fb7`. Implementation, component replay,
independent review and fixes were completed internally without individual
owner handoffs. Frozen NGAP sources were not changed or pruned.

## Observed final readiness

The final scanner used the original six modules at the existing source
authority (TS 38.413 V18.10.0, RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`). It reconciled 81 procedures and
131 messages: 81 initiating, 32 successful and 18 unsuccessful outcomes.
Parser/Fixer ownership was destroyed before generation.

| Gate | N17 baseline | Final batch |
|---|---:|---:|
| Physical extraction | 122/131 | 131/131 |
| BODY types + mapping + codec generation | 69/131 | 131/131 |
| Strict C++20 compilation | 69/131 | 131/131 |
| Previously accepted header triples unchanged | — | 69/69 |

All 207 previously successful headers retain their exact SHA-256 values.
This is physical/BODY generation and compilation readiness; it does not
qualify every complete NGAP-PDU or every legal value. Full rows and source
fingerprints are in `tools/shared-capability-qualification/readiness.json`.

## Completed capability scope

| Batch | Implemented bounded capability |
|---|---|
| N19 prerequisite + 1 | Atomic signed extensible INTEGER; retained discontinuous roots, canonical hull-offset encoding and explicit gap rejection |
| 2 | Extensible BIT/OCTET SIZE selector, root/extension length and owned payload operations |
| 3 | Printable, Visible and UTF8 strings; constrained/extensible and absent-SIZE declarations; Unicode scalar validation |
| 4 | Lossless effective use-site SIZE and finite addition evidence; bounded wide BIT SIZE through 131072 |
| 5 | Fragmented nonextensible collections with upper 65536; interleaved determinants/payload and final zero determinant |
| 6 | NULL zero-bit payloads; owned inline ENUMERATED CHOICE alternatives exposed by the same graphs |
| 7 | Empty extensible private IE sets with real local/global keys, canonical lossless OID contents and retained raw open payload |

An initial combined scan had 125 BODY successes. The six masked failures were
four unconstrained `URI-address` VisibleString graphs and two BIT use-site
`SIZE(1..131072)` graphs. Both were completed within this batch, then the final
131-message scan was rerun against unchanged final production source.

New ordinary entry points opt into their respective domains; physical IOC
generation combines the capabilities. Existing successful output remains
unchanged. Shared INTEGER/NULL/character feature selectors are distinct;
existing primitive enum values remain stable. New SIZE metadata is checked
across old public validators and generation-local inline enum alternatives.

## Verification and independent review

Final integrated tests: Typed **40/40**, runtime **10/10**; no skipped, expected
failure or error cases. Generated code is compiled with strict C++20 warning,
pedantic, conversion and sign-conversion checks. Changed production C passes
GCC strict C11 checks; existing extractor sign/const warnings were excluded
from the additional conversion checks, and no Clang result is claimed here.

| Reference profile | Replayed evidence |
|---|---|
| Extensible INTEGER | 768 actual generated/native/model encode/decode vectors |
| Extensible BIT SIZE | 256 explicit native/model/runtime comparisons |
| Wide fragmented BIT SIZE | 152 actual-schema native vectors, 304 encode/decode comparisons |
| Characters | 427 generated/model vectors; 415 native, 12 explicitly model-only |
| Fragmented collection | 8 actual generated/native boundary vectors through 65536 |
| NULL/inline ENUM CHOICE | 11 canonical native reference cases plus generated owned/IOC checks |
| Private keys/raw payload | 240 generated/native framing vectors; 120 independent pycrate global OID decode checks |

All component reference reports were replayed on the integrated final source.
The aggregate verifier rejects stale fingerprints, failed/incomplete component
outcomes, failed focused tests and changed previously accepted headers. It also
captures delegated naming/core/extraction inputs. Its report is
`tools/shared-capability-qualification/summary.json`; replay instructions are
in the adjacent README.

Component reviews were performed by agents other than the implementation
author. Final cross-integration review passed. Findings closed internally
include named INTEGER refinement/type mismatch, orphan SIZE addition metadata,
NULL physical-name preflight, discarded inline enum metadata, OID atomic
rollback, selected private open-field lowering, character addition refusal,
and nonmaximal wide-BIT fragment framing. Success/failure/OOM ownership,
Parser-deleted lifetime, deterministic generation and sticky/budget paths
have focused regression evidence.

ASan/UBSan component checks passed, including allocation-failure sweeps.
Integrated INTEGER, extensible SIZE, collections, final fragmented BIT and
final character generated checks also passed with leak detection disabled.
An actual LSan attempt failed with the environment's ptrace restriction;
this batch does not claim a passing LSan result. Distribution and whitespace
checks cover the new implementation, fixtures and replay artifacts.

## Explicit limits and next milestone

The INTEGER native parser drops union tails, so retained-set hull-offset
behavior uses independent models and legacy constraint evidence; hull-gap
values are conservatively refused even when extensible. Extension storage is
bounded to int64. Character/open/OID and existing unconstrained extension
payloads retain documented pre-fragmentation resource ceilings. New wide BIT
fragmentation applies to nonextensible bounded SIZE up to 131072; collection
fragmentation is bounded to upper 65536. Private known object-set rows remain
unsupported, and raw vendor payload semantics are not interpreted.

asn1tools 0.167.0 lacks some extensible string operations; relevant reports
explicitly identify model-only or surrogate evidence. Its global OID decoder
has a first-arc bug, so global OID semantics were independently checked with
pycrate 0.7.11. NULL's native zero-octet encoding is normalized only at the
frozen complete-encoding boundary; reordered CHOICE references use explicitly
canonical equivalent source order. None is presented as full-schema native
qualification.

No benchmark, complete external differential campaign or full 131-message
NGAP-PDU wire qualification was performed. The seven capability batches and
the all-message generation/compilation gate are complete. The next milestone
is complete NGAP-PDU interoperability qualification with acceptance profiles,
independent expected vectors and external references.
