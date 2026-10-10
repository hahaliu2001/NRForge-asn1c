# Unified NGAP-PDU typed dispatch and public API contract

Owner accepted this milestone on 2026-10-10, following complete-PDU qualification at `2122667859367e3a0482f31373a0ae9846c7f58c`. Autonomous implementation, independent review/fix, qualification, commit and push are authorized on the existing feature branch.

## Public boundary

`libngap/pdu.hpp` exposes `make_ngap_pdu(body, optional_criticality)`, `encode_ngap_pdu(pdu, limits)` and `decode_ngap_pdu(octets, limits)`. Generated `ngap.hpp` includes this lightweight header; each `messages/<Name>.hpp` independently exposes its stable message namespace and `Body` alias. Include only the message BODY headers needed by an application. The generated registry and all adapters must be linked once. No giant public variant or combined repeated BODY graph is introduced.

Typed construction derives the immutable outcome role/procedure identity from the concrete registered BODY type. Received criticality is preserved on decode and defaults to the declared criticality on construction; a checked setter permits changing only that field. `body_if<T>()` checks the actual concrete model and returns null for another type. Type-erased models retain both actual model and actual value type evidence; inconsistent adapter results are rejected inside the known-open transaction before publication. Adapter callbacks are trusted generated code, not wire-provided declarations.

PDU values own their BODY or opaque bytes. Copying deeply clones BODY and payload storage; moving is noexcept and explicitly invalidates the source. Copy assignment has the strong exception guarantee. Immutable registry metadata remains owned through shared const storage after the creating Registry object is destroyed. Copies of one Registry share its binding; a separately created Registry cannot encode a PDU belonging to another binding, even if its descriptive entries agree. A default PDU is invalid.

## Registry proof

The generation controller reads the exact frozen six-module schema through Parser/Fixer, extracts each owned BODY and envelope, verifies identical outer envelope evidence, and requires every declared `(role, procedure code)` payload slot exactly once. The accepted frozen profile has 81 procedures and 131 identities (81 initiating, 32 successful, 18 unsuccessful). These counts are qualification assertions, not generator hardcoded acceptance gates. Roles, PER selector indexes, criticalities, payload identity and code bounds come from validated owned evidence. Missing adapter definitions are link failures. Registry creation rejects missing/duplicate slots, invalid keys, duplicate BODY/model tokens, empty metadata and inconsistent declared criticality.

Parser/Fixer trees are destroyed before output rendering. Each types/mapping/codec output is generated twice and compared. Message namespaces use shared final Naming spelling. BODY types are message-specific even when their underlying declarations are repeated in other target graphs. Public alias and namespace collisions are generation errors.

## Wire and receive-only boundary

One outer complete encode/decode call owns the entire physical PDU budget and completion checks. Header decoding selects `(outcome role, procedure code)` once, then invokes the registered BODY FieldReader callback within the existing known-open transaction. Model allocation happens inside that callback. Known BODY failure never falls back to raw opaque data. The existing runtime governs atomicity, sticky first error, physical offsets, staging/retention budgets, padding and trailing-data checks; no runtime contract is changed.

Unknown root procedure codes and absent outcome slots of known procedures are owned opaque receive-only values when the object set permits them. Unknown outer CHOICE extensions retain their extension index and bytes separately. Both forms refuse encoding. Closed object sets reject unknown procedure codes. A malformed known payload is an error, not an unknown message. This is transport-level dispatch and does not enforce NGAP procedure/application policy or interpret NAS/RRC contents.

## Acceptance gates

Replay all 3432 historical complete-PDU accepted cases through one linked executable and the actual unified API. Construct independent native values anew, compare full bytes and decoded semantics, and require every historical case wire/native-semantic hash unchanged. Record new generated adapters, source fingerprints and executable provenance separately from the historical per-target qualification profile; do not rewrite that profile to conceal integration changes.

Run focused identity/model mismatch, registry completeness, deep ownership/copy/move, allocation failure, unknown receive-only, malformed known BODY and resource/error tests. Independently review implementation and tools, fix blocking findings, and record actual sanitizer availability accurately. Existing Typed IR and runtime checks remain required. Qualification remains finite, not exhaustive ASN.1 value coverage, live vendor interoperability or performance qualification.

Stop after this integration milestone. RAN application integration, Python bindings, benchmark, schema changes, merge, branch deletion and force push are excluded.
