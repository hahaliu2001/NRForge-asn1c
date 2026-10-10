# Target envelope outcome role generalization

## Scope

The target-envelope descriptor now resolves a target body by its exact owned
module/source identity across all present InitiatingMessage, SuccessfulOutcome
and UnsuccessfulOutcome cells in the full procedure table. Exactly one cell
must match. The matched role selects the physical CHOICE root by role evidence;
its source ordinal and canonical PER index remain separate. Missing or
ambiguous matches are rejected before publishing finalized evidence.

The renderer emits the typed/opaque payload variant on that selected root,
not invariably on InitiatingMessage. Existing descriptor APIs, procedure
bounds, role-presence checks, default provenance, opaque decoding of other
roots/procedures, and complete runtime boundaries are unchanged. Encoding is
still bounded to the one configured typed target. This is not a registry that
materializes every body simultaneously.

## Verification

- Baseline 3312ff011b24fe72e5297fd5129495ac32caac7e envelope core/renderer
  were independently compiled into a baseline driver. All twelve initiating
  body/envelope headers generated for the existing normal/acronym fixture
  are byte-identical after this change.
- The normal fixture is preserved. Owned copies place the target in each
  outcome role, with reversed physical tag order: source success ordinal 0
  has PER index 2, and failure ordinal 2 has PER index 0. Strict generated
  C++ compares complete seven-octet vectors, typed payload, wrong-code/
  opaque-target refusal and trailing data. Repeated generation and injected
  allocation-failure checks cover both outcome variants.
- An identity present in two roles is rejected as ambiguous and leaves
  mapping invalid. The matching initiating cell is then replaced with a
  different identity; finalize correctly selects the intended outcome role.
- Frozen-source replay extracts UEContextReleaseComplete and
  AMFConfigurationUpdateFailure from the untouched six-module source set,
  deletes the Parser tree, and generates body/envelope headers. The optional
  replay source checks all three received criticalities, exact complete bytes,
  typed payload, every shorter octet prefix, and trailing-byte rejection.

Independent pycrate 0.7.11 compiled the original six modules and selfdecoded
both complete NGAP-PDU values with empty protocolIEs:

| Role | Body | Procedure | Criticality | Complete APER hex |
| --- | --- | --- | --- | --- |
| successfulOutcome | UEContextReleaseComplete | 41 | reject | `20290003000000` |
| unsuccessfulOutcome | AMFConfigurationUpdateFailure | 0 | reject | `40000003000000` |

Empty protocolIEs validates envelope/body wire framing, not application-level
mandatory-IE policy or qualification of all possible values.

## Reproduction

In a configured tree with the Typed IR test driver built:

```sh
mkdir -p "$OUTPUT_DIR"
libasn1typed/check_asn1typed_envelope_render --actual \
  tools/qualification/ngap-rel18.modules "$ASN1_ROOT" "$OUTPUT_DIR"
c++ -std=c++20 -Wall -Wextra -Werror -pedantic-errors \
  -Wconversion -Wsign-conversion -DNDEBUG -Ilibaper -I"$OUTPUT_DIR" \
  libasn1typed/check_asn1typed_envelope_outcomes.cpp libaper/runtime.cpp \
  -o "$OUTPUT_DIR/check_outcomes"
"$OUTPUT_DIR/check_outcomes"
```

Native source identity is independently checked by
`tools/n11-envelope-qualification/native_reference.py:verify_sources` against
its committed six-module source manifest. `compile_native` builds that exact
source set rather than importing an installed NGAP schema. Set each PDU to
`(role, {procedureCode: code, criticality: reject, value: (body,
{protocolIEs: []})})`, compare `to_aper()`, then `from_aper()` and its full value.

No frozen schema, primitive runtime, benchmark or NGAP application semantics
were changed. Full 131-target interoperability evidence is a separate combined
qualification task.
