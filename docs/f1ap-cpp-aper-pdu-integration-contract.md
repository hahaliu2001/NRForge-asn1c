# F1-P3 — owned F1AP-PDU integration contract

Implementation baseline: `0ad019c340934391861e8c80ba5c3e7253adfef5`.
Branch: `feature/f1ap-cpp-aper`. Frozen authority remains TS 38.473 V18.10.0
at NRForge-RAN `d6e514a33ec3695925c24aa292900514b371b074`.

## Shared evidence and framing

The separate owned envelope descriptor supports the existing three-root
extensible NGAP profile and the four-root non-extensible F1AP profile. Source
order, effective tags, PER indexes and procedure roles remain distinct owned
evidence. No outer layout is guessed from a message name. All 94 procedures
and 158 outcome slots are reconciled from the unchanged schema. Parser/Fixer
trees are destroyed before rendering.

The fourth F1AP root is `choice-extension`, a parameterized
ProtocolIE-SingleContainer referencing an empty extensible IE object set. Its
base identity, object-set actual, class/field/selector provenance and tag are
owned and validated. It is not a CHOICE extension addition and is not an
ordinary procedure header. F1-P3 deliberately rejects this unsupported root
immediately after its two-bit root selector. No bytes are misinterpreted as
procedure code, and no opaque value is published for this branch. Supporting
its future IE payloads would require a separate evidenced capability.

## Complete registry API

The existing controller accepts an explicit `--f1ap` profile and emits
`f1ap.hpp`, lightweight message BODY headers, private codec/mapping headers,
one adapter per declared outcome and a complete registry TU. This is generated
shared dispatch, not hand-written message codecs. Missing, duplicate, extra
or ambiguous identities fail; repeated generation must be deterministic.
F1AP generation has a development CMake build, not an installed SDK claim.

`nrforge::f1ap::make_f1ap_pdu`, `encode_f1ap_pdu`, and `decode_f1ap_pdu` reuse
the NGAP ownership/model validation/known-open transaction machinery through
shared source includes. NGAP and F1AP expose distinct types, registries and
public entry points and can coexist. Registry construction rejects a framing
profile belonging to the other protocol. The existing NGAP interface retains
its default framing and generation mode.

Typed construction derives immutable role/code identity from the registered
BODY type. Received criticality is preserved; the checked setter changes only
criticality. BODY/payload copies are deep; moves invalidate their source.
Registry metadata survives the creating Registry object and separate bindings
cannot encode one another's PDUs. Known malformed BODY decoding is an error,
never an opaque fallback. One complete runtime transaction governs physical
offsets, sticky errors, padding, trailing data and all configured budgets.

Unknown root procedure codes and absent outcome slots are owned receive-only
opaque values when the procedure set is extensible. Closed sets reject unknown
codes. Opaque values refuse encode. F1AP has no outer CHOICE extension bit or
unknown outer-extension decoding path; NGAP retains its historical behavior.
Neither registry enforces application/procedure mandatory-IE policy.

## Acceptance and exclusions

Separate evidence is required for all 158 BODY and envelope extraction,
generation and strict compilation gates, full-registry linking, finite typed
construction/roundtrip/error integration tests, NGAP nonregression, and an
independent read-only review. Newly created source/output fingerprints must
match the stable final inputs. Historical P1/P2 reports remain unchanged.

F1-P3 is integration readiness, not independent complete-byte wire
qualification. The finite registry checks use 157 default empty-container
BODY values and one PrivateMessage vendor-opaque local:0/raw00 entry to satisfy
its nonzero minimum container SIZE. These roundtrips do not establish populated
payload coverage or required-IE validity. F1-P4 performs independent batch wire
qualification; F1-P5 delivers the C++ SDK and F1-P6 the Python SDK. Existing
NGAP SDK packaging must remain functional after sharing source includes.
No frozen schema edits, benchmark, merge, branch deletion or force push occur.
Project order remains F1AP → E1AP → RRC.
