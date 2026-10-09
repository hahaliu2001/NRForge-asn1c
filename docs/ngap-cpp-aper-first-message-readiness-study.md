# NGAP C++ / APER — First Real Message Readiness Study

Date: 2026-10-09. Status: **Owner Approved / Accepted** for first-message
selection and the complete-PDU target. This is not a detailed implementation
contract or codec qualification.

## Baseline and source authority

`vlm_master` was fast-forwarded from
`6171992831f4640717f34487a5c0526a872095eb` to
`096781515d3f4ac120b2160a2546bb621db3604d`, including S1–S4 and synthetic
qualification. `feature/ngap-cpp-aper-first-message` starts at that new baseline.
No history was rewritten and neither older feature branch was deleted.

The ASN.1 source remains TS 38.413 V18.10.0 / Rel-18, from NRForge-RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`, as recorded in the
[extraction closeout](ngap-full-schema-typed-ir-extraction-closeout.md).
All six modules in `tools/qualification/ngap-rel18.modules` were read directly
at that commit. Their retrieved local snapshots matched these Git blob hashes:

| Module | Blob SHA |
| --- | --- |
| NGAP-CommonDataTypes.asn | acaba01d1fad8d656ae8f568bf951de3b67278ac |
| NGAP-Constants.asn | ba80f8fd34d41c48087417dc702622abff4bbaaf |
| NGAP-IEs.asn | a8974dd27e5c7f37bf439990fb39da397ec97044 |
| NGAP-Containers.asn | 978bbf5864787b87eefcfbf26c84d471de330eb5 |
| NGAP-PDU-Contents.asn | fb48dd32c66479cf539c7e1d9676c5a90919b996 |
| NGAP-PDU-Descriptions.asn | aad6567aad39cebe0591c9a16b7c25bc012d1a45 |

## Recommended first message

**UEContextReleaseCommand** is the recommended first message. Deterministic UE
release is in the Owner's Tier-A procedure scope supplied in the conversation;
the extraction closeout itself does not select Tier-A messages. This is a
bounded engineering recommendation, not a claim of globally minimal complexity
among all 131 NGAP messages.

Its object set has only two mandatory root IEs and no optional root IEs:

| IE | ID | Criticality | Payload |
| --- | ---: | --- | --- |
| UE-NGAP-IDs | 114 | reject | CHOICE: UE-NGAP-ID-pair, AMF-UE-NGAP-ID, choice-Extensions |
| Cause | 15 | ignore | CHOICE: radioNetwork, transport, nas, protocol, misc, choice-Extensions |

The enclosing elementary procedure is `uEContextRelease`, initiating message
UEContextReleaseCommand, procedure code 41, procedure criticality reject. Its
successful outcome UEContextReleaseComplete is a separate message and is not
part of this first codec task.

Compared candidates, by source inspection:

- UEContextReleaseRequest adds optional PDU-session resource lists.
- UEContextReleaseComplete adds optional location, paging, session and diagnostic structures.
- UplinkNASTransport adds mandatory NAS-PDU and UserLocationInformation and optional opaque identities.
- NGSetupRequest remains the extraction golden baseline, but carries a wider setup payload dependency graph.
- NGSetupFailure has one mandatory Cause, but optional TimeToWait and CriticalityDiagnostics; it remains an alternative candidate if the Owner prefers setup-outcome coverage.

## Minimum semantic work still required

| Family | Concrete source evidence | Current boundary / required work |
| --- | --- | --- |
| Larger constrained INTEGER | AMF-UE-NGAP-ID 0..1099511627775; RAN-UE-NGAP-ID 0..4294967295; ProcedureCode 0..255 | S2/S4 mapping only supports 0..65535. Freeze wider-domain wire primitives before implementation; storage width alone is not wire mapping. |
| ENUMERATED | Criticality; five Cause ENUMERATED payloads, including extension values | Synthetic types/mapping/codec have no ENUMERATED support. Preserve schema root/extension identity and unknown numeric values; freeze representation and mapping separately. |
| General root CHOICE | UE-NGAP-IDs has three alternatives; Cause has six | Owned tag evidence exists, but synthetic mapping requires exactly two. Audit evidence for all alternatives before mapping; never substitute source or variant order. |
| Extensible SEQUENCE | UEContextReleaseCommand and UE-NGAP-ID-pair contain extension markers | S1/S4 reject these. Need extension bit, addition boundaries and owned preservation consistent with D1/D2. |
| Variable-length containers | ProtocolIE-Container SIZE 0..65535; optional ProtocolExtensionContainer in ID pair | Need owned collection generation, length mapping and shared decode/output resource budgets. |
| Bound IOC / open type | ProtocolIE-Field's id, criticality and selected Value; empty extensible object sets for choice-Extensions | Extraction has bounded binding support; current renderer rejects bound instances. Complete object-set dispatch evidence and an owned unknown-payload model remain gaps, not implied by successful extraction. |
| Complete PDU envelope | NGAP-PDU → InitiatingMessage → procedure-selected Value | A message-body codec alone is not a complete NGAP wire codec. Need independently scoped single-procedure envelope dispatch and bounded open-type processing. |

The explicit `choice-Extensions` alternatives are normal root alternatives,
not ASN.1 CHOICE extension markers. Cause enum extensions, SEQUENCE additions,
IE-set extensibility and NGAP-PDU CHOICE extensibility are separate mechanisms.
An implementation must not merge them into one generic extension flag.

## Proposed implementation boundary

The target is one complete NGAP-PDU for UEContextReleaseCommand, with the two
known mandatory IEs represented as typed values and both ordinary UE-NGAP-IDs
forms supported. All five ordinary Cause families must retain their schema
identity. Unknown values/payloads with determinable boundaries must be owned and
preserved at the codec layer, consistent with approved D1-R2; protocol ignore,
reject or reporting policy remains outside the codec. Encoding unknown opaque
payloads is not implicitly authorized by preservation.

This target requires further contracts. The concrete unknown-data layout,
dispatch registry, duplicate/missing-IE handling boundary and new resource/error
API are not frozen by this study. Unsupported schema semantics remain generation
errors; the frozen schema must not be pruned or edited to make the renderer pass.

Keep work sequential and individually reviewable:

1. Freeze and implement constrained-INTEGER / ENUMERATED primitives in separate small tasks.
2. Freeze and implement owned extensions, collections and bounded open types separately.
3. Add complete IOC/dispatch evidence, then typed generation and wire mapping.
4. Integrate this message body; separately integrate its complete PDU envelope.
5. Independently qualify wire vectors and cross-decode against the untouched frozen schema.

## Evidence and decisions

This study inspected schema and current source contracts; it did not implement
codec capability, benchmark, or rerun full-schema qualification. A fresh
multi-candidate extraction probe was started but interrupted before completion;
no fresh extraction counts or PASS results are claimed. Historical extraction
acceptance remains the closeout's bounded evidence.

Owner approval on 2026-10-09: **UEContextReleaseCommand is the first message**
and **the complete-PDU target above is accepted**, following the Owner's explicit
agreement in the project conversation. Subsequent detailed contracts remain
separate approval candidates. Approval does not close any capability gap listed
above and does not authorize pruning the frozen schema.

Independent read-only review on 2026-10-09: **PASS as an Owner approval candidate**.
The reviewer verified all six source hashes, message/procedure facts and the
capability gap boundaries. No factual fixes were required; review did not
approve the proposed scope or run builds/probes.
