# ASN1TYPED Semantic Capability Matrix

## 1. Status and Purpose

**Status:** Authoritative semantic support-boundary inventory

**Applies to:** `libasn1typed` real-message Typed extraction
qualification

**Protocols:** NGAP, F1AP, E1AP, RRC

This document records supported ASN.1 semantics, owned representations,
frozen support boundaries, fail-closed boundaries, and qualification
evidence.

It answers: **Is this semantic already supported, and within what
boundary?**

The companion `docs/asn1typed-real-message-development.md` defines the
development process.

Before work on a new real message, developers MUST review both documents
and run the permanent real-message probe. A frozen semantic MUST NOT be
re-studied merely because it appears in another message. A new study is
permitted only when fixed-tree evidence falls outside the frozen
boundary.

This matrix describes the foundation qualified through real Release-18
NGAP `NGSetupRequest`. It does **not** claim complete ASN.1 support.

## 2. Golden Qualification Baseline

### NGAP Rel-18 `NGSetupRequest`

-   **Status:** Complete / Accepted / Foundation Qualified
-   **Authoritative modules:** 6
-   **Parse:** PASS
-   **Fix:** PASS
-   **Typed extraction:** PASS
-   **Return code:** `0`
-   **Diagnostic:** empty
-   **Owned types:** 29
-   **Bound instances:** 20
-   **Parser-tree independence:** PASS
-   **Repeated clear:** PASS

The permanent tools MUST reproduce this baseline before it changes.

## 3. Status Vocabulary

### Supported

Accepted owned Typed representation within the stated boundary.

### Supported, bounded

Supported only for the explicitly stated shape or context.

### Fail closed

Intentionally rejected because Typed IR cannot retain all required
semantics.

### Not qualified

No accepted real qualification currently establishes support.

## 4. Core Types

### Module-Qualified Type Identity

-   **Status:** Supported
-   **Owned IR:** Owned module plus source name.
-   **Supported boundary:** Stable module-qualified identity independent
    of parser-tree lifetime.
-   **Fail-closed boundary:** Ambiguous or unresolved identity.
-   **Evidence:** Real `NGSetupRequest`.

### Cross-Module Dependency Closure

-   **Status:** Supported
-   **Owned IR:** Owned references and dependency closure.
-   **Supported boundary:** Qualified cross-module dependencies.
-   **Fail-closed boundary:** Unresolved or unsupported dependencies.
-   **Evidence:** Real `NGSetupRequest`.

### SEQUENCE

-   **Status:** Supported
-   **Owned IR:** `ASN1TYPED_TYPE_SEQUENCE` with ordered fields.
-   **Supported boundary:** Supported field semantics in source order.
-   **Fail-closed boundary:** Unsupported child semantics.
-   **Evidence:** `Extended-RANNodeName`.

### CHOICE

-   **Status:** Supported
-   **Owned IR:** Ordered alternatives.
-   **Supported boundary:** Supported alternatives in source order.
-   **Fail-closed boundary:** Unsupported alternative semantics.
-   **Evidence:** `GlobalRANNodeID`, `GNB-ID`.

### SEQUENCE OF

-   **Status:** Supported, bounded
-   **Owned IR:** `ASN1TYPED_TYPE_SEQUENCE_OF` plus owned element
    reference and applicable metadata.
-   **Supported boundary:** Qualified element/reference shapes.
-   **Fail-closed boundary:** Unsupported constrained element semantics.
-   **Evidence:** `ProtocolExtensionContainer`.

### ENUMERATED

-   **Status:** Supported, bounded
-   **Owned IR:** Ordered enum items plus extensibility metadata.
-   **Supported boundary:** Known named items in source order.
-   **Fail-closed boundary:** Unsupported child kinds or extension
    placement.
-   **Evidence:** `PagingDRX`.

## 5. Primitive Kinds

### BOOLEAN

-   **Status:** Supported
-   **Supported boundary:** Plain BOOLEAN.
-   **Fail-closed boundary:** Unsupported attached semantics.
-   **Evidence:** Existing Typed regression.

### INTEGER

-   **Status:** Supported, bounded
-   **Owned IR:** `ASN1TYPED_PRIMITIVE_INTEGER`.
-   **Supported boundary:** Plain INTEGER and one qualified closed
    bounded value range.
-   **Fail-closed boundary:** Unsupported INTEGER constraint shapes.
-   **Evidence:** `ProtocolIE-ID`.

### UTF8String

-   **Status:** Supported, bounded
-   **Supported boundary:** Plain form and SIZE forms accepted by
    qualified generic SIZE rules.
-   **Fail-closed boundary:** Unsupported constraints.
-   **Evidence:** Existing Typed regression.

### PrintableString

-   **Status:** Supported, bounded
-   **Supported boundary:** Plain form and SIZE forms accepted by
    qualified generic SIZE rules.
-   **Fail-closed boundary:** Unsupported constraints.
-   **Evidence:** Existing Typed regression.

### VisibleString

-   **Status:** Supported, bounded
-   **Owned IR:** `ASN1TYPED_PRIMITIVE_VISIBLE_STRING`.
-   **Supported boundary:** Plain form; qualified bounded and extensible
    SIZE.
-   **Fail-closed boundary:** Unsupported primitive constraints.
-   **Evidence:** `RANNodeNameVisibleString`.

### OCTET STRING

-   **Status:** Supported, bounded
-   **Owned IR:** `ASN1TYPED_PRIMITIVE_OCTET_STRING`.
-   **Supported boundary:** Plain form; qualified bounded and exact
    SIZE.
-   **Fail-closed boundary:** Unsupported constraints.
-   **Evidence:** `PLMNIdentity`.

### Opaque OCTET STRING with ContentsConstraint

-   **Status:** Supported, bounded / Level 1 compatibility
-   **Owned IR:** Existing `ASN1TYPED_PRIMITIVE_OCTET_STRING`; the outer
    value remains an opaque sequence of octets.
-   **Supported boundary:** A primitive `OCTET STRING` use site whose
    declared and combined constraint shape is the recognized
    `ContentsConstraint`-only form MAY use the existing OCTET STRING Typed
    IR when the complete outer value is representable as opaque bytes and
    the Typed IR boundary does not claim to validate, decode, or interpret
    the contained value. No additional unsupported constraint may change
    the outer OCTET STRING value or wire semantics.
-   **Contents ownership:** The contained-value semantic is optional
    non-blocking metadata. It does not require a new Typed IR primitive,
    value kind, or contained-payload IR merely to represent the outer
    OCTET STRING.
-   **Fail-closed boundary:** Arbitrary unsupported inline constraints,
    unrecognized `ContentsConstraint` forms, any additional constraint that
    changes outer value or wire semantics, and any context whose contract
    requires contained-value validation, decoding, or interpretation MUST
    remain rejected. This rule MUST NOT be generalized to silently ignore
    unknown constraints.
-   **Semantic family:** Opaque OCTET STRING contained payloads. The rule is
    not NAS-specific; it may cover NAS messages, NGAP transfer/container
    payloads, nested ASN.1 payloads, and other opaque protocol payloads
    within the same bounded outer-value contract.
-   **Evidence:** NGAP Rel-18
    `PDUSessionResourceSetupItemSURes.pDUSessionResourceSetupResponseTransfer`
    (`NGAP-IEs.asn:5517`), fixed-tree base primitive OCTET STRING with
    declared and combined `SET / ContentsConstraint`.
    Focused REQUIRED and OPTIONAL SEQUENCE-field extraction also confirms
    existing OCTET STRING ownership, no contained-payload IR or constraint
    metadata, and parser-tree-independent outer representation. The
    `NGAP-PDU-Contents.PDUSessionResourceSetupResponse` real probe passes
    through this field after the compatibility route was added.

### BIT STRING

-   **Status:** Supported, bounded
-   **Owned IR:** `ASN1TYPED_PRIMITIVE_BIT_STRING`.
-   **Supported boundary:** No named bits; qualified bounded and exact
    SIZE.
-   **Fail-closed boundary:** Named bits and unsupported constraints.
-   **Evidence:** `GNB-ID.gNB-ID`, `NgENB-ID.macroNgENB-ID`.

#### Named-Bit Rule

Named-bit semantics are not represented. `BIT STRING { x(0), y(1) }`
MUST fail closed, including when SIZE is present. Named bits MUST NOT be
silently discarded.

INTEGER named-number behavior is not expanded by this rule.

## 6. Constraint Capabilities

### Bounded SIZE: `SIZE(a..b)`

-   **Status:** Supported, bounded
-   **Owned IR:** `asn1typed_size_constraint_t`
-   **Supported boundary:** Integer bounds in qualified declaration and
    use-site contexts.
-   **Fail-closed boundary:** Unsupported compound or unrepresentable
    SIZE shapes.
-   **Evidence:** `GNB-ID.gNB-ID` with `22..32`.

### Extensible Bounded SIZE: `SIZE(a..b, ...)`

-   **Status:** Supported, bounded
-   **Owned IR:** Bounds plus `is_extensible`.
-   **Supported boundary:** Qualified bounded range plus extension
    marker.
-   **Fail-closed boundary:** Unsupported additions or compound shapes.
-   **Evidence:** `RANNodeNameVisibleString`.

### Exact SIZE: `SIZE(N)`

-   **Status:** Supported, bounded
-   **Owned IR:** `lower_bound=N`, `upper_bound=N`, nonextensible.
-   **Supported boundary:** Qualified primitive/use-site contexts
    including OCTET STRING and BIT STRING.
-   **Fail-closed boundary:** Unsupported exact-size primitive/context
    combinations.
-   **Evidence:** `PLMNIdentity` SIZE 3; `macroNgENB-ID` SIZE 20.

### Extensible Exact SIZE: `SIZE(N, ...)`

-   **Status:** Supported, bounded
-   **Owned IR:** `asn1typed_size_constraint_t` with
    `lower_bound=N`, `upper_bound=N`, and `is_extensible=true`.
-   **Supported boundary:** One supported nonnegative exact bound followed
    immediately by one extension marker, with no additions or other
    constraint components; existing qualified primitive/use-site SIZE
    ownership contexts.
-   **Fail-closed boundary:** Compound or disjoint SIZE, additions after the
    marker, multiple or malformed markers, unsupported bounds, and arbitrary
    constraint AST shapes.
-   **Evidence:** Synthetic parser-tree-independent BIT STRING extraction
    for 8 and 16. Fixed-tree evidence: `NGAP-IEs.RATRestrictionInformation`
    is `BIT STRING SIZE(8, ...)`, and
    `NGAP-IEs.NRencryptionAlgorithms` is `BIT STRING SIZE(16, ...)`.
    Real message traversal: `DownlinkNASTransport` passes extraction with
    50 owned types and 26 bound instances, establishing traversal through
    `RATRestrictionInformation SIZE(8, ...)`. `InitialContextSetupRequest`
    traverses `NRencryptionAlgorithms SIZE(16, ...)` successfully, then
    stops at `ExpectedActivityPeriod: unsupported or unrepresentable
    primitive constraint`.

### Closed INTEGER Value Range

-   **Status:** Supported, bounded
-   **Owned IR:** `asn1typed_integer_value_range_t` with lower and upper
    bounds and `is_extensible`.
-   **Supported boundary:** One closed inclusive range representable in
    `intmax_t`; `is_extensible` is false.
-   **Fail-closed boundary:** Compound, MIN/MAX, or unrepresentable ranges.
-   **Evidence:** `ProtocolIE-ID` with `0..65535`.

### Bounded Extensible INTEGER Value Range

-   **Status:** Supported, bounded
-   **Owned IR:** The same `asn1typed_integer_value_range_t`, with
    `is_extensible` true.
-   **Supported boundary:** One bounded inclusive range representable in
    `intmax_t`, followed immediately by exactly one extension marker:
    `INTEGER (a..b, ...)`. F1-P2 additionally owns bounded explicit
    extension ranges/unions separately; see the F1-P2 entry below.
-   **Fail-closed boundary:** Unsupported extension addition shapes, multiple
    or malformed markers, intersections, MIN/MAX, arbitrary Generic
    Constraint AST forms, and unrepresentable bounds. Disjoint root unions
    are accepted only within the separate bounded permitted-value-set rule
    below.
-   **Evidence:** Synthetic extraction and parser-tree-independent ownership
    for `INTEGER (0..65535, ...)`; NGAP-IEs `AveragingWindow` fixed-tree
    shape (`0..4095, ...`). The `NGSetupRequest` golden probe passes, but its
    output does not establish that extraction traversed `AveragingWindow`.

### Bounded INTEGER Permitted-Value Set

-   **Status:** Supported, bounded
-   **Owned IR:** Existing `asn1typed_integer_value_range_t`. The first
    canonical interval uses `lower_bound` / `upper_bound`; `tail` owns only
    subsequent `asn1typed_integer_interval_t` values. `is_extensible` remains
    a separate fact. A simple range has no tail and preserves its prior field
    values.
-   **Supported boundary:** A single root `UNION` containing at least two
    terms, each an inclusive bounded INTEGER `ValueRange` or INTEGER
    `SingleValue`, optionally followed by exactly one final extension marker.
    Values must be representable in `intmax_t`. Extraction sorts by lower
    then upper bound and merges overlapping or adjacent intervals. This is a
    bounded permitted-set semantic, not generic constraint AST ownership.
-   **Fail-closed boundary:** Standalone single values; MIN/MAX; intersections;
    EXCEPT; unsupported nested compounds; marker inside the union, before the
    root, multiple or malformed markers, unsupported additions after the marker;
    unrepresentable or non-integer terms; and arbitrary Generic Constraint
    AST forms.
-   **Evidence:** Synthetic fixture `libasn1typed/fixtures/integer-permitted-set-t8.asn1`
    covers mixed range/singleton terms, multiple singletons, sorting, overlap
    and adjacency merging, extensibility, and ExpectedActivityPeriod's exact
    root set. The test inspects owned intervals after parser-tree deletion,
    verifies deep field-copy ownership and repeated clear, and verifies a
    malformed union fails after an earlier valid term without publishing
    partial IR. Permanent fixed-tree inspection confirms
    `NGAP-IEs.ExpectedActivityPeriod` has declared and combined root
    `1..30 | 40 | 50 | 60 | 80 | 100 | 120 | 150 | 180 | 181` followed by
    an extension marker. The rebuilt permanent probe extracts
    `InitialContextSetupRequest` successfully with 179 owned types and 144
    bound instances, establishing real traversal through this declaration.

### Generic Constraint AST

-   **Status:** Not qualified
-   **Owned IR:** None.
-   **Fail-closed boundary:** Constraint semantics outside accepted
    bounded representations.

SIZE metadata and INTEGER value-range metadata are separate. Type
extensibility and SIZE extensibility are separate.

## 7. Use-Site Constraint Ownership

### Named Declaration SIZE

-   **Status:** Supported, bounded
-   **Owned IR:** Type-level SIZE metadata.
-   **Supported boundary:** Qualified SIZE shapes.
-   **Fail-closed boundary:** Unsupported shapes.

### SEQUENCE Field Inline SIZE

-   **Status:** Supported, bounded
-   **Owned IR:** Field base reference plus field-level SIZE metadata.
-   **Supported boundary:** Complete inline constraint representable by
    existing SIZE metadata.
-   **Fail-closed boundary:** Other unsupported inline constraints.

### SEQUENCE Field Inline INTEGER Value Range

-   **Status:** Supported, bounded
-   **Existing semantics:** Primitive INTEGER, OPTIONAL presence, and one bounded inclusive INTEGER value range representable in `intmax_t` are already supported. Family B owns the extensible range flag, and this field ownership reuses that same range representation. No new INTEGER value-range semantic is required.
-   **Owned IR:** Field-level `asn1typed_integer_value_range_t`, independent of field presence and primitive INTEGER identity.
-   **Supported boundary:** An inline primitive INTEGER SEQUENCE field whose complete constraint is one supported bounded inclusive value range, closed or followed immediately by the supported extension marker, may reuse the existing INTEGER value-range semantic. REQUIRED and OPTIONAL presence remain orthogonal.
-   **Fail-closed boundary:** Compound, MIN/MAX, unrepresentable, or otherwise unsupported INTEGER constraint shapes, and arbitrary unsupported inline constraints, MUST remain rejected.
-   **Semantic family:** Inline INTEGER value-range ownership on SEQUENCE fields; extensible inline ranges are a Level-1 composition of existing Family B range semantics and Family D field ownership.
-   **Evidence:** Synthetic REQUIRED and OPTIONAL closed inline field extraction and parser-tree-independent ownership; synthetic OPTIONAL extensible inline `INTEGER (1..3600, ...)` extraction and parser-tree-independent ownership. NGAP Rel-18 `ExpectedUEMovingTrajectoryItem.timeStayedInCell` (`NGAP-IEs.asn:2144`) has an OPTIONAL inline primitive INTEGER with declared and combined `0..4095` closed value range. Fixed-tree inspection confirms `UE-DifferentiationInfo.periodicTime` is OPTIONAL inline primitive INTEGER with declared and combined `1..3600, ...`; real message probes establish traversal only where extraction passes the field.

### SEQUENCE Field Inline ENUMERATED

-   **Status:** Supported, bounded
-   **Existing semantic:** ENUMERATED ordered named items, type/body extensibility, and per-item root-versus-known-extension-addition status are already represented by the named ENUMERATED IR and Family-A item model. No new ENUMERATED semantic is introduced.
-   **Owned IR:** A SEQUENCE field may own an identity-free nested `asn1typed_type_t` whose kind is ENUMERATED and whose ordered items reuse `asn1typed_enum_item_t`. Field presence remains on the field and is independent of the body.
-   **Supported boundary:** Direct ordered named-value items; at least one root item; at most one marker after a root item; direct known named additions after the marker; existing mandatory/OPTIONAL presence.
-   **Fail-closed boundary:** Malformed or unnamed items, multiple markers, marker before any root item, extension groups or non-direct additions, unsupported constraints or parameterization, DEFAULT presence, and arbitrary inline constructed types. This does not qualify inline ENUMERATED in CHOICE alternatives or SEQUENCE OF elements.
-   **Evidence:** Synthetic mandatory and OPTIONAL extensible inline fields plus a known extension addition; item order/status, deep copy, cleanup, repeated clear, and inspection after parser-tree destruction. A synthetic IOC message dependency test reaches a named SEQUENCE with an inline ENUMERATED field, completes dependency closure without inventing a named identity for that field, and still collects a genuine named dependency. Fixed-tree inspection confirms `NGAP-IEs.AUN3DeviceAccessInfo.aUN3DeviceAccess` (mandatory; `true`, marker) and `NGAP-IEs.UE-DifferentiationInfo.periodicCommunicationIndicator` (OPTIONAL; `periodically`, `ondemand`, marker). `InitialUEMessage` passes extraction with 50 owned types and 26 bound instances, establishing its field dependency path. After inline extensible INTEGER compatibility, `DownlinkNASTransport` and `InitialContextSetupRequest` both extract beyond `UE-DifferentiationInfo.periodicTime`, establishing traversal through the preceding `periodicCommunicationIndicator` field in those paths. The later `DownlinkNASTransport` extraction passes with 50 owned types and 26 bound instances; `InitialContextSetupRequest` traverses `NRencryptionAlgorithms` and later stops at `ExpectedActivityPeriod: unsupported or unrepresentable primitive constraint`.

### CHOICE Alternative Inline SIZE

-   **Status:** Supported, bounded
-   **Owned IR:** Alternative base reference plus alternative-level SIZE
    metadata.
-   **Supported boundary:** Complete inline constraint representable by
    existing SIZE metadata.
-   **Fail-closed boundary:** Other unsupported inline constraints.
-   **Evidence:** `GNB-ID.gNB-ID`, `NgENB-ID.macroNgENB-ID`.

### CHOICE Alternative Inline INTEGER Value Range

-   **Status:** Supported, bounded
-   **Owned IR:** Alternative-level
    `asn1typed_integer_value_range_t`, including owned interval data,
    independent of the alternative's primitive type reference.
-   **Supported boundary:** The extractor reuses the existing bounded
    INTEGER permitted-value-set domain for a constrained primitive INTEGER
    alternative. This includes supported root UNION forms and owned
    interval tails, with an extensibility flag for that alternative.
    Alternative names and source order are retained. Existing CHOICE
    alternative SIZE metadata and parameterized alternative references
    remain supported independently.
-   **Fail-closed boundary:** INTEGER constraints outside the bounded
    permitted-value-set rules above, malformed constraint shapes, and
    arbitrary inline constraints MUST remain rejected.
-   **Evidence:** Implementation in `d003c926`
    (`libasn1typed/asn1typed.h`, `libasn1typed/asn1typed.c`, and
    `libasn1typed/asn1typed_extract.c`). Focused synthetic coverage in
    `check_inline_integer_choice_ranges`
    (`libasn1typed/check_asn1typed_extract.c`) verifies separate ranges and
    extensibility, alternative order and names, retained parameterized
    binding, ownership after parser-tree deletion, and rejection of an
    unsupported compound range. The focused API test
    `check_choice_alternative_size_api` (`libasn1typed/check_asn1typed.c`)
    verifies deep-copy behavior, interval-tail ownership, invalid metadata
    rejection, and repeated cleanup. Real NGAP qualification demonstrates
    bounded extensible ranges in `NGAP-IEs.ClockAccuracy`:
    `clockAccuracyValue INTEGER (1..40000000, ...)` and
    `clockAccuracyIndex INTEGER (32..47, ...)`. The accepted
    `TimingSynchronisationStatusReport` qualification report records PARSE /
    FIX / EXTRACT PASS, 16 owned types, and 11 bound instances. Union-form
    CHOICE extraction is not separately qualified by real-message evidence;
    per-run qualification reports are not committed in the inspected
    repository. This is Typed IR extraction support; it does not establish
    that the C++ CHOICE renderer consumes range metadata or that runtime
    Codec support is complete.

### SEQUENCE OF Element Inline Constraint

-   **Status:** Fail closed / not qualified
-   **Owned IR:** No additional element use-site constraint ownership.
-   **Fail-closed boundary:** Constrained element use-sites outside the
    frozen element contract.

**Generic rule:** an inline field or CHOICE constraint MAY be accepted
only when its complete semantics can be represented by existing owned
metadata. Otherwise extraction MUST fail closed.

### Capability Composition Boundary

-   **Full-domain reuse:** When a use-site ownership capability directly reuses an existing owned semantic representation, that ownership SHOULD retain the representation's complete frozen supported domain unless evidence requires a narrower boundary.
-   **Level-1 composition:** If frozen capabilities combine without a new semantic fact, IR representation, ownership location, identity/lifetime rule, or interaction semantic, their bounded combination is Level 1 compatibility/composition rather than a new semantic family.
-   **Not automatic closure:** Independent support does not prove every combination. The complete combined semantic MUST remain representable without loss and unsupported compound or malformed shapes MUST remain fail closed.
-   **Negative-test discipline:** Negative coverage protects genuinely unsupported or unrepresentable semantics. It MUST NOT freeze an otherwise losslessly representable combination merely to preserve an earlier task boundary.
-   **Qualified example:** `asn1typed_integer_value_range_t` owns bounds and range extensibility. SEQUENCE-field inline INTEGER ownership reuses that representation, so supported closed and bounded-extensible ranges are both within the qualified field ownership boundary. `UE-DifferentiationInfo.periodicTime INTEGER (1..3600, ...) OPTIONAL` provides real evidence for this Level-1 composition.
-   **Fail-closed boundary:** This rule does not authorize arbitrary composition of independently supported semantics. Additional constraints, interaction semantics, ownership requirements, or AST shapes outside a frozen combined boundary remain unsupported unless completely represented.

## 8. Extensibility

### ENUMERATED Extensibility

-   **Status:** Supported, bounded
-   **Owned IR:** Type-level `is_extensible` plus ordered owned enum items
    carrying root-versus-extension-addition status.
-   **Supported boundary:** One marker after at least one known root item;
    direct named-value items before the marker are roots, and direct
    named-value items after it are known extension additions.
-   **Fail-closed boundary:** Multiple markers, marker before all known
    root items, malformed children, and extension-addition groups or
    structures outside the direct named-value shape.
-   **Evidence:** `PagingDRX`; synthetic parser-tree-independent coverage;
    NGAP `QosMonitoringRequest` fixed-tree shape (`ul`, `dl`, `both`,
    marker, `stop`).

### SEQUENCE Extensibility

-   **Status:** Supported, bounded
-   **Owned IR:** Type-level `is_extensible`.
-   **Supported boundary:** One marker after known root fields; no
    qualified known post-marker additions.
-   **Fail-closed boundary:** Multiple markers, post-marker fields,
    unsupported addition groups.
-   **Evidence:** `Extended-RANNodeName`.

### SIZE Extensibility

-   **Status:** Supported, bounded
-   **Owned IR:** `size_constraint.is_extensible`.
-   **Supported boundary:** Qualified bounded exact or range SIZE followed
    by its sole extension marker.
-   **Fail-closed boundary:** Unsupported compound/addition shapes.
-   **Evidence:** `RANNodeNameVisibleString`.

### Unknown Runtime Extensions

-   **Status:** Not qualified
-   **Supported boundary:** Schema extensibility metadata only.
-   **Out of scope:** Runtime unknown-extension representation.

Skipping an extension marker without retaining extensibility is
forbidden.

## 9. Parameterized References and Bound Instances

### Parameterized Object-Set Reference Identity

-   **Status:** Supported, bounded
-   **Owned IR:** Generic module/name plus ordered actual identities.
-   **Supported boundary:** Qualified object-set-reference actual shape.
-   **Fail-closed boundary:** Unsupported or malformed actuals.
-   **Evidence:**
    `ProtocolIE-SingleContainer{{GlobalRANNodeID-ExtIEs}}`.

### Extensible Empty IOC Set Binding

-   **Status:** Supported, bounded
-   **Owned IR:** The message retains its parameterized container identity
    and object-set reference, IOC-table presence and object-set
    extensibility, and message SEQUENCE extensibility as separately stored
    metadata. No placeholder IOC rows are synthesized.
-   **Supported boundary:** A valid parameterized container binding to an
    object set with zero known entries and an extension marker. The message
    SEQUENCE extension marker is stored separately from object-set
    extensibility.
-   **Fail-closed boundary:** A missing or unresolved IOC table or binding
    remains an error. The extractor rejects an empty object set without its
    extension marker; this non-extensible-empty rejection does not have a
    focused test.
-   **Evidence:** Implementation commit `b9caffe5`
    (`libasn1typed/asn1typed.h`, `libasn1typed/asn1typed.c`, and
    `libasn1typed/asn1typed_extract.c`).
    Synthetic fixture `libasn1typed/fixtures/ioc-empty-typed.asn1` and
    focused checks in `libasn1typed/check_asn1typed_ioc.c` cover empty-set
    extensibility, owned container/object-set binding, separate storage of
    message SEQUENCE extensibility, and missing-table rejection. The real
    NGAP object set is `NGAP-PDU-Contents.OverloadStopIEs`; the accepted
    `OverloadStop` qualification report records PARSE / FIX / EXTRACT PASS,
    1 owned type, and 0 bound instances. Per-run qualification reports are
    not committed in the inspected repository. This documents Typed IR
    extraction only and does not claim complete runtime Codec support.

### Parameterized Field Type Reference

-   **Status:** Supported, bounded
-   **Owned IR:** Complete field type reference including actuals.
-   **Supported boundary:** Accepted object-set actual model.
-   **Fail-closed boundary:** Unsupported actual forms.
-   **Evidence:** `Extended-RANNodeName.iE-Extensions`.

### Bound-Instance Ownership and Deduplication

-   **Status:** Supported
-   **Owned IR:** Module-owned instance keyed by semantic parameterized
    identity.
-   **Supported boundary:** Generic identity plus ordered actual
    kind/module/name; identical identities deduplicate.
-   **Fail-closed boundary:** Invalid or malformed keys.
-   **Evidence:** B7b and SetA/SetB qualification.

### Bound SEQUENCE Materialization

-   **Status:** Supported, bounded
-   **Owned IR:** Bound instance with owned SEQUENCE body.
-   **Supported boundary:** Unique semantic specialization recovery and
    qualified body semantics.
-   **Fail-closed boundary:** Unsupported or ambiguous
    specialization/body.
-   **Evidence:** `ProtocolIE-SingleContainer`.

### Bound SEQUENCE OF Materialization

-   **Status:** Supported, bounded
-   **Owned IR:** Bound instance with owned SEQUENCE OF body, element
    reference, and SIZE.
-   **Supported boundary:** Qualified specialization body and
    parameterized element.
-   **Fail-closed boundary:** Unsupported body kinds or element shapes.
-   **Evidence:** `ProtocolExtensionContainer`.

### Fixer Pointer / `spec_index`

-   **Status:** Forbidden as durable identity.
-   **Rule:** Pointers, synthetic locations, and `spec_index` MAY be
    diagnostic evidence but MUST NOT become Typed semantic identity.

## 10. Class-Field and Selected-Type Semantics

### Fixed Class-Field-Derived Type

-   **Status:** Supported, bounded
-   **Owned IR:** Fixed field type plus applicable relation.
-   **Supported boundary:** Qualified fixed class fields in materialized
    bound bodies.
-   **Fail-closed boundary:** Unsupported class-field shapes.
-   **Evidence:** `criticality` in `ProtocolIE-SingleContainer`.

### Selected / Open Class-Field Type

-   **Status:** Supported, bounded
-   **Owned IR:** `CLASS_FIELD_SELECTED_TYPE` plus owned relation; no
    fake ordinary reference.
-   **Supported boundary:** Qualified object-set-bound selected field
    with selector relation.
-   **Fail-closed boundary:** Missing or invalid relation.
-   **Evidence:** `value` in `ProtocolIE-SingleContainer`.

### Class-Field Relation

-   **Status:** Supported, bounded
-   **Owned IR:** Class identity, class-field identity, actual index,
    optional selector source name.
-   **Supported boundary:** Qualified relation tied to an enclosing
    bound instance.
-   **Fail-closed boundary:** Invalid metadata or out-of-range actual
    association.
-   **Evidence:** B7b.2.

### Object-Set Row Enumeration

-   **Status:** Not qualified / not required by current body contract.
-   **Current boundary:** Selection semantics may be retained without
    enumerating rows.
-   **Future capability:** Row-specific selected-type resolution.

## 11. Ownership, Lifetime, and Cleanup

### Deep Ownership

-   **Status:** Required
-   **Rule:** Owned IR must not depend on parser/fixer storage.

### Parser-Tree Independence

-   **Status:** Required
-   **Rule:** Relevant IR remains usable after parser-tree deletion.
-   **Evidence:** Golden `NGSetupRequest`.

### Atomic Bound-Body Publication

-   **Status:** Required
-   **Rule:** Partial bodies are not published; `body_materialized` is
    set only after complete attachment.

### Repeated Clear

-   **Status:** Required
-   **Rule:** Cleanup is safe when repeated.
-   **Evidence:** Golden `NGSetupRequest`.

### Silent Semantic Loss

-   **Status:** Forbidden
-   **Rule:** Reject instead.

## 12. Explicit Fail-Closed Boundaries

Known boundaries include:

-   unsupported primitive kinds;
-   BIT STRING named bits;
-   unsupported primitive constraint shapes;
-   unsupported/compound SIZE outside frozen rules;
-   unsupported INTEGER ranges outside the closed/extensible range and
    bounded permitted-value-set rules;
-   unsupported inline constraints not representable by accepted
    metadata;
-   constrained SEQUENCE OF element use-sites outside the frozen
    contract;
-   unsupported SEQUENCE extension placement or post-marker fields;
-   unsupported ENUMERATED marker placement, malformed items, or
    post-marker structures beyond direct named-value additions;
-   malformed/unsupported parameter actuals;
-   unresolved dependencies;
-   unsupported bound specialization body kinds or semantics;
-   invalid class-field relation metadata;
-   selected-type semantics without a complete owned relation.

A future message encountering one of these MUST use
`asn1typed_tree_inspect` and compare the observed fixed-tree shape with
this matrix. It MUST NOT simply weaken the rejection.

## 13. Permanent Qualification Tools

### `tools/asn1typed_real_probe`

Permanent real-message qualification path. It parses configured modules
separately, combines them in explicit order, runs the fixer, calls
production Typed extraction, and reports phase status, diagnostic,
owned-type count, and bound-instance count.

Golden result:

``` text
PARSE PASS
FIX PASS
EXTRACT PASS
ROOT NGAP-PDU-Contents.NGSetupRequest
OWNED_TYPES 29
BOUND_INSTANCES 20
```

### `tools/asn1typed_tree_inspect`

Permanent module-qualified fixed-tree inspector. It reports type/member
identity, source location, meta/expr type, declared and combined
constraints separately, references/resolved targets, formal parameters,
RHS actual structure, and direct-child reference/parameter structure.

Inspector output is fixed-tree evidence, not durable Typed identity.
Routine Level-1/2 work SHOULD use these tools instead of new scratch
harnesses.

## 14. Real Qualification Evidence Index

### `NGAP-PDU-Contents.NGSetupRequest`

-   **Evidence for:** Golden end-to-end Typed extraction.

### `NGAP-IEs.PagingDRX`

-   **Evidence for:** Extensible ENUMERATED.

### `NGAP-IEs.Extended-RANNodeName`

-   **Evidence for:** Extensible SEQUENCE and parameterized field
    reference.

### `NGAP-IEs.RANNodeNameVisibleString`

-   **Evidence for:** VisibleString plus bounded extensible SIZE.

### `NGAP-CommonDataTypes.ProtocolIE-ID`

-   **Evidence for:** INTEGER plus closed bounded value range.

### `NGAP-IEs.PLMNIdentity`

-   **Evidence for:** OCTET STRING plus exact SIZE.

### `NGAP-IEs.PDUSessionResourceSetupItemSURes.pDUSessionResourceSetupResponseTransfer`

-   **Evidence for:** Level-1 opaque OCTET STRING contained-payload
    compatibility for the recognized `ContentsConstraint`-only use-site
    shape.

### `NGAP-IEs.ExpectedUEMovingTrajectoryItem.timeStayedInCell`

-   **Evidence for:** OPTIONAL SEQUENCE-field inline primitive INTEGER ownership of the closed `0..4095` range. Permanent fixed-tree inspection confirms the declared and combined shape; synthetic owned extraction validates the new field metadata. Real message-path traversal is not established by these results.

### `NGAP-IEs.AUN3DeviceAccessInfo.aUN3DeviceAccess` and `NGAP-IEs.UE-DifferentiationInfo.periodicCommunicationIndicator`

-   **Evidence for:** SEQUENCE-field inline ENUMERATED fixed-tree shapes and synthetic owned-body extraction. `InitialUEMessage` passes full message extraction, establishing traversal of the mandatory `aUN3DeviceAccess` field. After inline extensible INTEGER compatibility, `DownlinkNASTransport` and `InitialContextSetupRequest` both extract beyond the earlier `periodicTime` field, establishing traversal of the preceding `periodicCommunicationIndicator` field in those paths.

### `NGAP-IEs.GNB-ID.gNB-ID`

-   **Evidence for:** BIT STRING plus inline CHOICE SIZE range.

### `NGAP-IEs.NgENB-ID.macroNgENB-ID`

-   **Evidence for:** BIT STRING plus inline CHOICE exact SIZE.

### `NGAP-IEs.ClockAccuracy`

-   **Evidence for:** CHOICE alternative inline INTEGER range ownership for
    `clockAccuracyValue` (`1..40000000, ...`) and `clockAccuracyIndex`
    (`32..47, ...`). Implementation commit `d003c926`; focused synthetic
    extractor evidence in `check_inline_integer_choice_ranges`
    (`libasn1typed/check_asn1typed_extract.c`) and focused API evidence in
    `check_choice_alternative_size_api` (`libasn1typed/check_asn1typed.c`).
    The accepted `TimingSynchronisationStatusReport` qualification records
    PARSE / FIX / EXTRACT PASS, 16 owned types, and 11 bound instances.
    Real-message evidence demonstrates these bounded extensible ranges, not
    union-form CHOICE extraction.

### `NGAP-PDU-Contents.OverloadStopIEs`

-   **Evidence for:** Extensible empty IOC-set binding with a valid
    parameterized container reference and no synthesized placeholder rows.
    Implementation commit `b9caffe5`; synthetic fixture
    `libasn1typed/fixtures/ioc-empty-typed.asn1` and focused checks in
    `libasn1typed/check_asn1typed_ioc.c`. The accepted `OverloadStop`
    qualification records PARSE / FIX / EXTRACT PASS, 1 owned type, and 0
    bound instances. Non-extensible empty IOC rejection is implemented but
    lacks a focused test.

### `NGAP-Containers.ProtocolIE-SingleContainer` specialization

-   **Evidence for:** Bound SEQUENCE materialization and class-field
    selected semantics.

### `NGAP-Containers.ProtocolExtensionContainer` specialization

-   **Evidence for:** Bound SEQUENCE OF materialization.

Evidence qualifies only the stated boundary; it does not imply every
variant of the construct is supported.

## 15. How to Use This Matrix

For every new real message:

1.  Review `docs/asn1typed-real-message-development.md`.
2.  Review this matrix.
3.  Run `tools/asn1typed_real_probe`.
4.  If extraction passes, proceed directly to qualification/review.
5.  If extraction fails, inspect the exact construct with
    `tools/asn1typed_tree_inspect`.
6.  Compare the fixed-tree shape with this matrix.
7.  Classify:
    -   inside frozen boundary but incorrectly routed: **Level 1**;
    -   safe composition of frozen semantics/ownership with no new semantic fact or ownership requirement: **Level 1 composition**;
    -   one bounded missing semantic within existing architecture: **Level 2**;
    -   new ownership/identity/dependency/materialization architecture: **Level 3 / FOUNDATION ESCALATION**.
8.  Before freezing a Level-2 ownership boundary, compare the reused owned representation with its complete frozen supported domain and check directly adjacent capability compositions.
9.  Do not re-study a frozen semantic merely because it appears in another message.

## 16. Matrix Change Control

This document is authoritative. Support-boundary changes MUST be
evidence-based.

A matrix entry MAY be expanded only after:

-   the new fixed-tree shape is directly evidenced;
-   the owned representation is implemented;
-   fail-closed boundaries are reviewed;
-   focused tests pass;
-   real qualification passes;
-   parser-tree independence is demonstrated where relevant;
-   independent review accepts the change.

Do not change a support boundary merely to make a new message pass.

When a new semantic is accepted, update this matrix in the same logical
task or an immediately following documentation task.

## 17. Foundation Summary

The accepted foundation includes:

-   module-qualified owned dependencies;
-   core constructed types;
-   bounded extensibility metadata;
-   qualified primitive strings, OCTET STRING, and BIT STRING;
-   bounded and exact SIZE;
-   bounded closed and extensible INTEGER value ranges;
-   extensible empty IOC-set binding;
-   bounded CHOICE-alternative inline INTEGER value-range ownership;
-   bounded capability composition where an ownership location can retain an existing semantic representation's complete frozen supported domain;
-   field and CHOICE SIZE ownership;
-   parameterized object-set identity;
-   bound specialization ownership/materialization for qualified
    SEQUENCE and SEQUENCE OF shapes;
-   bounded class-field selected-type semantics;
-   parser-tree-independent owned IR;
-   fail-closed behavior outside frozen boundaries.

This summary is not a substitute for the detailed entries above.

## N14 bounded C++ APER OCTET capability

The Owned IR OCTET/opaque-Contents rules above are retained. N14 adds vector<byte> C++ types, mapping and codecs for named non-extensible SIZE intervals 0..65535 and opaque unconstrained unfragmented values up to 16383 octets at runtime. Physical IOC selected direct Contents-only Value/Extension cells reuse the existing outer-octet ownership rule; unsupported anonymous SIZE/constraint combinations remain rejected. No contained-value validation, SIZE extension or fragmentation support is claimed. Earlier non-IOC compound/collection API acceptance boundaries are unchanged. See [the exact N14 contract and evidence](ngap-cpp-aper-n14-octet-contract-and-closeout.md).

## N15 owned BIT STRING and effective SIZE lowering

New opt-in `asn1typed_render_cpp_owned_bit_*` types/mapping/codec supports owned runtime BitString values with explicit bit counts, fixed/non-extensible bounded SIZE0..65535, and unfragmented unconstrained values up to16383 bits. Effective BIT/OCTET field and CHOICE SIZE retains named identity and uses separately generated site-specific framing. Physical IOC accepts plain anonymous BIT and supported named BIT references. Named-bit lists, anonymous constrained IOC payloads, extensible SIZE, unrepresented known SIZE additions and fragmentation fail closed; no semantic is reduced to a weaker range. Existing N14 outputs remain unchanged. N15 primitive reference evidence and63/131 BODY compile readiness do not qualify additional complete NGAP messages. See `ngap-cpp-aper-n15-bit-contract-and-closeout.md` and `tools/n15-bit-qualification/`.


## F1-P2 shared body semantics

These shared capabilities extend the earlier bounded entries; they do not introduce per-message codecs or change the frozen ASN.1 sources.

- **Parameterized collection elements:** A named SEQUENCE OF may own a complete parameterized element reference, including the accepted object-set actual kind/module/name. Physical dependency closure reuses existing bound specialization materialization and IOC selector/registry binding. Unsupported actuals, ambiguous specializations and unsupported collection/element semantics still fail closed.
- **Inline constructed lifting:** SEQUENCE fields, CHOICE alternatives and SEQUENCE OF elements may lift supported inline SEQUENCE, SEQUENCE OF and CHOICE bodies into module-qualified owned types. Internal `$inline$` keys encode the declaration/member path (including collection element steps); `$` is unavailable in ASN.1 source identifiers. Nested bodies retain presence, SIZE, source location and original CHOICE tag evidence without modifying the Parser/Fixer tree. Ordinary module extraction closes anonymous dependencies, and physical IOC extraction closes their payload dependencies through the existing worklist. The internal keys are compiler identities, not new schema declarations. SET/SET OF, DEFAULT and constraints outside the supported constructed-body contracts remain rejected.
- **Known INTEGER extension additions:** `asn1typed_integer_value_range_t` owns a separate canonical `extension_additions` interval array and count. Bounded explicit ranges or unions after one extension marker retain their own exact intervals; root membership and PER-visible root bounds remain separate. Overlapping/adjacent intervals within each set normalize, while sparse gaps remain intact. Deep copy, cleanup and renderer preflight include the new metadata. Generated mappings expose known extension intervals. The existing extensible INTEGER runtime remains unchanged: known additions and future unknown values outside the root use signed unconstrained extension encoding within the existing int64 domain. Known additions do not close the extension domain or admit forbidden gaps inside the root hull.

- **Constrained selected IOC primitives:** Physical Value/Extension rows may lift supported anonymous constrained primitive payloads into an owned type keyed by object-set module/name and numeric IE ID. Named primitive/SIZE/domain renderers then retain the complete constraints; the earlier opaque OCTET STRING CONTAINING path remains unchanged. This covers the fixed SIZE(3) OCTET payload in `RRC-Version-ExtIEs`; constructed or parameterized selected payloads outside the accepted model remain rejected.
- **Root-only extensible CHOICE:** A single terminal marker after the root alternatives owns `choice_root_only_extension_owned` evidence. Root tag/PER ordering remains validated; encoding writes the zero extension bit before the existing root selector. An incoming true extension bit returns sticky `constraint_violation` before root dispatch. Known additions, malformed marker placement and manually fabricated incomplete evidence remain rejected. This is root-value support, not unknown-extension ownership or complete value-space qualification.
- **Large bounded collections:** Non-extensible SEQUENCE OF bounds retain any nonnegative range representable in size_t. When the upper bound is at least 65536, existing APER segment determinants interleave with element payloads; each fragment has at most 65536 elements and an exact-fragment ending emits a final zero determinant. Full schema bounds and aggregate runtime budgets are checked, with budget refusal before reserve/allocation. Historical types with upper bounds at most 65536 preserve generated output. No huge maximum-sized allocation is required to qualify the accepted finite boundary tests.

Focused tests inspect owned data after Parser/Fixer destruction, exercise nested constructed graphs and collection bindings, and compare extensible INTEGER bytes against an independent bit model. Real-message evidence and exact regression results are recorded in the F1-P2 closeout and batch report. Body extraction, generation and strict compilation are distinct readiness gates; F1-P2 does not qualify complete F1AP-PDU framing or independent wire interoperability.
