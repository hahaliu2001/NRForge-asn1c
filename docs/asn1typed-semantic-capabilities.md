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

### Closed INTEGER Value Range

-   **Status:** Supported, bounded
-   **Owned IR:** Bounded INTEGER value-range metadata.
-   **Supported boundary:** One closed inclusive range representable in
    `intmax_t`.
-   **Fail-closed boundary:** Extensible, compound, MIN/MAX, or
    unrepresentable ranges.
-   **Evidence:** `ProtocolIE-ID` with `0..65535`.

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

### CHOICE Alternative Inline SIZE

-   **Status:** Supported, bounded
-   **Owned IR:** Alternative base reference plus alternative-level SIZE
    metadata.
-   **Supported boundary:** Complete inline constraint representable by
    existing SIZE metadata.
-   **Fail-closed boundary:** Other unsupported inline constraints.
-   **Evidence:** `GNB-ID.gNB-ID`, `NgENB-ID.macroNgENB-ID`.

### SEQUENCE OF Element Inline Constraint

-   **Status:** Fail closed / not qualified
-   **Owned IR:** No additional element use-site constraint ownership.
-   **Fail-closed boundary:** Constrained element use-sites outside the
    frozen element contract.

**Generic rule:** an inline field or CHOICE constraint MAY be accepted
only when its complete semantics can be represented by existing owned
metadata. Otherwise extraction MUST fail closed.

## 8. Extensibility

### ENUMERATED Extensibility

-   **Status:** Supported, bounded
-   **Owned IR:** Type-level `is_extensible`.
-   **Supported boundary:** One marker after known root items; no
    qualified known post-marker additions.
-   **Fail-closed boundary:** Multiple markers, unsupported placement,
    known items after marker.
-   **Evidence:** `PagingDRX`.

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
-   **Supported boundary:** Qualified bounded SIZE plus marker.
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
-   unsupported INTEGER ranges outside one closed bounded range;
-   unsupported inline constraints not representable by accepted
    metadata;
-   constrained SEQUENCE OF element use-sites outside the frozen
    contract;
-   unsupported SEQUENCE/ENUMERATED extension placement or known
    post-marker additions;
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

### `NGAP-IEs.GNB-ID.gNB-ID`

-   **Evidence for:** BIT STRING plus inline CHOICE SIZE range.

### `NGAP-IEs.NgENB-ID.macroNgENB-ID`

-   **Evidence for:** BIT STRING plus inline CHOICE exact SIZE.

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
    -   one bounded missing semantic within existing architecture:
        **Level 2**;
    -   new ownership/identity/dependency/materialization architecture:
        **Level 3 / FOUNDATION ESCALATION**.
8.  Do not re-study a frozen semantic merely because it appears in
    another message.

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
-   bounded INTEGER value ranges;
-   field and CHOICE SIZE ownership;
-   parameterized object-set identity;
-   bound specialization ownership/materialization for qualified
    SEQUENCE and SEQUENCE OF shapes;
-   bounded class-field selected-type semantics;
-   parser-tree-independent owned IR;
-   fail-closed behavior outside frozen boundaries.

This summary is not a substitute for the detailed entries above.
