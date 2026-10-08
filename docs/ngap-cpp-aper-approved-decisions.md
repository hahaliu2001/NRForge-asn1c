# NGAP C++ Generator & APER — D1/D2 Approved Decisions

## Status and authority

D1 and D2: **Complete / Accepted** by the Project Owner on 2026-10-08.
This document records the explicitly approved decisions from the project
conversation and its D1 review clarifications. It does not invent missing
implementation details or claim to reproduce a complete earlier study report.
Repository baseline: `vlm_master`,
`e71cdc38f69cbd4db654e7a5ab4c06e9db5cae57`.
Schema baseline: NGAP TS 38.413 V18.10.0 / Rel-18.

The owner's current instruction authorizes recording, independent review and
commit of these decisions, followed by a read-only D3 study. Approval of a
contract does not establish implementation or codec qualification.

## D1 — Architecture decisions

| Decision | Approved contract |
| --- | --- |
| D1-01 | Hybrid C++20 type model. |
| D1-02 | Owned-by-default values; recursive types retain deep-copy value semantics. |
| D1-03 | Shared generic APER runtime boundary and semantic contract. |
| D1-04 | Protocol-specific generated codec and schema metadata. |
| D1-05 | Structured result-based encode/decode API. |
| D1-06 | NGAP, F1AP, E1AP and RRC have independent Typed IR instances and generated types. Generic compiler and codec algorithms may be shared. |
| D1-07 | Generator fails closed on unrepresentable or unsupported ASN.1 semantics; no silent semantic loss. |
| D1-08 | Unknown IE handling separates codec preservation from protocol policy. |

### Accepted review clarifications

- **D1-R1 — INTEGER:** use `int64_t` / `uint64_t` for a bounded INTEGER
  only when its complete permitted domain is representable. Use an owned
  arbitrary-precision integer for unbounded or wider domains. Selection uses
  schema constraints, not observed test values. APER must not truncate.
- **D1-R2 — Unknown extensions:** the codec identifies and preserves unknown
  extension payloads with determinable boundaries. Protocol processing decides
  ignore, reject or reporting from NGAP criticality and procedure rules.
  Preservation does not establish semantic support or authorize unchecked
  re-encoding.
- **D1-R3 — Recursion:** owned indirection such as `std::unique_ptr<T>` may
  resolve recursive storage; generated types provide deep-copy construction
  and assignment where needed to retain value semantics.
- **D1-R4 — Runtime reuse:** freeze the shared interface and semantic boundary
  only. Reuse, wrapping or replacement of the legacy C APER implementation
  requires a separate Runtime Reuse Study.

NGAP/F1AP/E1AP use APER; RRC uses UPER. Shared algorithms do not permit
protocol-specific type or registry dependencies between generated artifacts.

The accepted initial vertical slice is a synthetic module containing bounded
INTEGER, OPTIONAL BOOLEAN, CHOICE and SEQUENCE. Type generation, primitive
codec, constructed codec and NGAP dispatch remain separate implementation
tasks with independent review before commit.

## D2 — C++ generated type model contracts

| Decision | Approved contract |
| --- | --- |
| D2-01 | Primitive mapping follows the C++20 hybrid value model, including the D1-R1 INTEGER strategy. |
| D2-02 | SEQUENCE, SEQUENCE OF and CHOICE have strongly typed representations. |
| D2-03 | OPTIONAL, DEFAULT and CONDITIONAL retain distinct ASN.1 semantics. |
| D2-04 | Unknown extensions have an independent owned representation. |
| D2-05 | Owned values and recursive deep-copy value semantics. |
| D2-06 | Naming and generated artifacts preserve protocol isolation. |

These are the six contracts explicitly approved in the conversation. Exact
support-type layouts, DEFAULT storage/access APIs, extension container layouts,
namespace spellings and generated ABI are not specified by that approval
summary and are not newly frozen by this record. Subsequent detailed contracts
must remain traceable to approved evidence or receive separate approval.

## Current repository evidence and remaining gaps

| Evidence | Existing capability and limit |
| --- | --- |
| `libasn1typed/asn1typed.h` | Owned types, references, constraints, presence and IOC relations; presence has mandatory/optional/conditional categories, not a DEFAULT value model. INTEGER bounds use `intmax_t`; this is not arbitrary-precision ownership. |
| `libasn1typed/asn1typed_render_cpp.h` | C++20 baseline renderer; OPTIONAL and CONDITIONAL use optional storage, with no condition evaluation. It emits no codec, namespace, numeric enum values or constraints; cycles are rejected. |
| `docs/asn1typed-semantic-capabilities.md` | Authoritative bounded extraction support; unknown runtime extensions and object-set row enumeration are not qualified. |
| `docs/ngap-full-schema-typed-ir-extraction-closeout.md` | Accepted NGAP extraction coverage; does not establish full generator or APER readiness. |
| `skeletons/aper_*.h` | Existing descriptor-driven C APER interfaces; no approved C++ runtime reuse conclusion. |

Conditional predicates, DEFAULT values, named bits, arbitrary precision,
runtime unknown extensions, recursive generated values and complete object-set
dispatch metadata remain subject to explicit implementation and qualification.
No gap is closed by D1/D2 approval alone. Existing extraction support boundaries
remain governed by the semantic capability matrix.

## Next stages and execution boundary

1. D3 — APER Runtime Interface Contract Study: read-only study and approval candidate.
2. D4 — Generator Semantic Mapping Contract: separate task.
3. Implementation: individually scoped type generation and codec tasks.
4. NGAP Python generation/APER and C++ ↔ Python interoperability.
5. Independent F1AP, E1AP and RRC full-schema development.

CLI-only; no WSLg dependency. This recording task changes documentation only,
runs no builds or tests, and does not start generator or codec implementation.

## Independent recording review

2026-10-08: a separate review agent checked this record against the supplied
owner approvals, `asn1typed.h`, the renderer header/source and the capability
matrix. Verdict: **PASS**, no blocking or minor findings. The review confirmed
that detailed layouts were not invented and unimplemented capabilities were
not claimed. Review was read-only; no tests or implementation work occurred.
This reviews faithful recording, not a new approval or codec qualification.
