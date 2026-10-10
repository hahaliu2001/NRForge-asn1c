# N9-P1 — IOC / open-type dispatch architecture and readiness

2026-10-09. Base `d97fc16627b9eec0c7d4bedcd0524cba4d71dec4`.
Executed under the [continuous authority](ngap-cpp-aper-autonomous-execution-authority.md), for the untouched UEContextReleaseCommand complete-PDU target. This milestone studies evidence and fixes architectural boundaries; it does not implement dispatch or claim message qualification. Detailed owned metadata and runtime API contracts remain separate reviewed implementation prerequisites.

## Source and normative authority

The six frozen TS 38.413 V18.10.0 modules and their Git blob identities remain those recorded in [first-message readiness](ngap-cpp-aper-first-message-readiness-study.md), NRForge-RAN commit `d6e514a33ec3695925c24aa292900514b371b074`. No schema is pruned, edited or regenerated.

Primary PER authority: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), §§11.1, 11.2.1–11.2.2 and 11.9. An open-type payload is an inner **complete encoding**, including final zero padding or empty-value `00` substitution. Its enclosing determinant counts octets, with aligned payload and fragmentation where required. Table constraints do not turn an open field into an ordinary inline payload or make row order a wire order.

## Actual physical shape

| Boundary | Frozen shape | Consequence |
|---|---|---|
| UEContextReleaseCommand | One mandatory `protocolIEs` field followed by a trailing SEQUENCE extension marker | Encode an extension bit, then a container; not two ordinary mandatory IE fields |
| ProtocolIE-Container | SIZE(0..65535) OF ProtocolIE-Field, parameterized by object set | Physical entry order and multiplicity are wire data |
| ProtocolIE-Field | Ordered id, criticality, selected value | id is 0..65535; criticality has three roots; value is separately framed open type |
| Command registry | ID 114 → UE-NGAP-IDs/reject/mandatory; ID 15 → Cause/ignore/mandatory; extensible set | Dispatch by numeric selector, never row ordinal or type name |
| UE-NGAP-IDs / Cause choice-Extensions | Root alternatives referencing single containers bound to empty extensible sets | Preserve unknown selected payload; these are not CHOICE extension markers |
| UE-NGAP-ID-pair.iE-Extensions | OPTIONAL extension container SIZE(1..65535), bound to an empty extensible set | Distinct lower bound and selected field/class identity |

ProtocolExtensionField uses id, criticality, extensionValue and the NGAP-PROTOCOL-EXTENSION class, rather than assuming every selected field is named value. Wire criticality must be decoded and retained independently of the registry's expected criticality. Application conformance/criticality policy is separate from lossless codec representation.

## Existing Owned IR: evidence and gaps

`asn1typed_extract_message` deliberately publishes a flattened logical SEQUENCE whose fields are object-set rows. Its public API documents that representation. It retains row symbolic/numeric IDs, expected criticality, presence, payload references and the message's parameterized `ioc_container`. N7 explicitly marks flattened physical extension-structure evidence unsupported. No later generator may reinterpret these rows as physical components.

Bound bodies independently own SEQUENCE field order, fixed types, class-field selected semantics, class/field identity, actual index and selector component identity; bound collections own SIZE and parameterized element identity. The selector's fixed UNIQUE class field is currently retained as an ordinary typed field without class relation metadata. Existing actual identity is not proof of a complete dispatch table.

Missing durable generation evidence must be added separately: per-object-set owned rows and extensibility (including empty extensible sets), association of every selected bound body to its registry, explicit numeric selector identity and role, and physical message/container structure. Row completeness, duplicate numeric IDs, cross-module identities and matching selector/class/actual relationships require validation. Unavailable/unsupported evidence must remain distinguishable from a valid empty registry or selector/index zero. Publication must be transactional; direct public-struct edits require revalidation, as with earlier wire evidence.

Preserve the legacy flattened extraction API and all previous renderer output/acceptance promises. Introduce an opt-in physical/dispatch evidence path rather than silently changing logical fields into container entries. No borrowed Parser/Fixer pointer survives extraction. Known rows retain full payload reference identity; no name-specific dispatch or synthesized table based on observed values is permitted.

## Owned codec model and protocol policy boundary

The planned physical representation is an ordered owned collection of entry records. Each entry retains its numeric ID, received criticality and either a schema-associated typed known value or an owned unknown payload. Known alternatives need independent wrappers even when payload C++ types coincide. Empty extensible registries remain valid and can receive unknown records.

Never collapse entries into a map or overwrite duplicates. Missing mandatory rows, duplicate entries and criticality mismatches remain inspectable protocol-policy inputs; the codec does not fabricate missing values, defaults or application success. Exact policy/convenience projections belong to the later message integration contract. An unknown ID in a supported extensible registry preserves its raw payload and metadata. A malformed known-ID payload is a codec error, not an excuse to silently downgrade it to unknown. Encoding retained unknown opaque data remains refused through sticky failure; preservation alone does not authorize re-encoding.

## Known open-type runtime architecture

The current N7 `read_open_type_owned()` consumes complete outer framing and charges unknown payload/record retention plus wire bits. It is correct for unknown data, but is **not** a typed-dispatch API. Reading with it and then calling `decode_complete` with a fresh context resets shared budgets; reusing the same ordinary context double-charges wire and risks marking the parent complete.

A separately reviewed scoped child boundary is required:

- Inner alignment starts at payload bit zero; validate inner final zero padding, empty substitution and absence of trailing octets before publishing the known value.
- Share sticky first error and cumulative collection/extension/unknown-retention budgets; do not call an outer complete wrapper or globally finish the parent from a child.
- Charge every physical input/output/wire unit once. Logical inner parsing and scratch buffering are not additional received wire. Known transient payload storage must not consume unknown-retention quotas, and must have explicit bounded scratch/depth accounting.
- Translate decode child error positions to original wire positions, including determinant gaps in fragmented frames. Nested opens require composable position mapping, not merely adding one payload start offset. Encode child errors occur before the final determinant/payload positions are known; the later contract must explicitly choose an open-field-start diagnostic or a structured child path, never present a local bit offset as a proven global position.
- Define scoped framing/allocation transaction boundaries and earlier-charge behavior explicitly. No partial complete value may be published, and an ignored child error must still stop the outer wrapper.
- Encoding completes a typed child locally before writing its determinant/payload, checks sizes before narrowing and stages output atomically. It must not reset limits or permit child finish to finish the parent.

Exact public signatures, new limit names/defaults, charge/reservation rules, depth limits, callback exception handling and error priorities are **not frozen by this study**. Freeze and review them before runtime edits. Existing N7 unknown decoding and ordinary primitives must retain byte and lifecycle compatibility.

## Individually reviewable progression

| Milestone | One primary objective | Acceptance gate |
|---|---|---|
| N9-P1 (this study) | Establish physical/dispatch evidence gaps and architecture | Independent source/design review and honest execution evidence |
| N9-P2 | Add durable, validated IOC registry/physical evidence | Core ownership tests followed by untouched target inspection; no codec implementation |
| N9-P3 | Add scoped known open-type runtime boundary | First API/accounting design review, then focused atomicity/budget/error tests and independent implementation review |
| N9-P4 | Generate bounded IOC entries/containers using evidence and runtime | Synthetic structural/name variations, native full-byte/value comparisons and rejection tests |

Each milestone may be split into smaller implementation/evidence tasks without bundling a new ownership model with complete-message qualification. After N9, separately integrate UEContextReleaseCommand body and its complete NGAP-PDU envelope, then qualify against the untouched frozen schema. No invented N8-P4/P5, no generic all-procedure NGAP registry, no benchmark and no WSLg dependency.

## Executed evidence and review

Source inspection covered `asn1typed.h`, ordinary/IOC extraction and bound materialization, N7 open framing and complete-wrapper runtime, and the frozen container/message/IE/PDU declarations. All six local source Git blob hashes exactly matched the recorded frozen authority.

The existing developer probe was executed against all six untouched modules:

```sh
tools/asn1typed_real_probe --asn1-root /absolute/frozen-ngap \
  --module-list tools/qualification/ngap-rel18.modules \
  --root-module NGAP-PDU-Contents --message UEContextReleaseCommand
```

Result: PARSE/FIX/EXTRACT PASS, 14 ordinary types and four bound instances. The already-built developer tool supplies the aggregate observation; a separate scratch driver compiled against current owned libraries supplied fresh detailed inspection, deleting the Parser tree before reading owned data.

| Fresh owned observation | Result |
|---|---|
| Command flattened rows | Two: ID114/reject/mandatory → UE-NGAP-IDs; ID15/ignore/mandatory → Cause |
| Command physical root proof | Explicitly unsupported; published root count zero, not inferred two |
| Root CHOICE mappings | UE-NGAP-IDs has three alternatives; Cause has six |
| UE-NGAP-ID-pair | Three ordered fields; extensible root structure resolved with root count three |
| Two single-container specializations | UE-NGAP-IDs-ExtIEs and Cause-ExtIEs actuals; ordered id/criticality/value bodies |
| Extension-container specialization | UE-NGAP-ID-pair-ExtIEs actual; SIZE(1..65535); selected element references same actual |
| Extension-field specialization | Ordered id/criticality/extensionValue; selected class relation refers to selector id and actual index zero |
| Main command container and IE-field specializations | Absent from the four bound instances; `ioc_container` association alone did not expand this physical graph |
| Dispatch tables in specialized bodies | No independently owned finalized row table; the three relevant extension sets are empty and extensible in source |

Independent scratch synthetic IOC feasibility inspection used native pycrate to encode and decode table-selected BOOLEAN true (`01 01 01 80`) and a 16-bit integer value 258 (`01 02 02 01 02`). These are minimal synthetic examples, not target-message vectors. asn1tools accepts that synthetic schema but exposes the selected value as raw octets rather than typed dispatch; its separately encoded payload/framing can be an independent wire oracle, not evidence of typed IOC support. The later target qualification must use the untouched frozen schema with an IOC-aware oracle; no filtering workaround is authorized.

No production code or existing test was changed; no new runtime/codec tests, sanitizer pass or target-message qualification are claimed. Existing full suites were not rerun for this documentation-only milestone. Whitespace and independent final design review results are recorded at closeout.

Independent architecture/source review: **PASS**, no blocking amendments. Frozen source shapes, registry IDs, physical/logical distinction, protocol-policy boundary and deliberately deferred exact runtime contract were independently checked. `git diff --check` and new-document whitespace checks passed. N9-P1 is complete; the next task is N9-P2 owned physical/registry evidence, not an unreviewed codec implementation.
