# N9-P2 — Owned IOC evidence implementation

2026-10-09. Independent design and focused implementation review accepted the owned-evidence contract and corrections. This phase supplies physical evidence only; it does not implement IOC wire encoding or claim whole-message qualification.

## Delivered ownership and compatibility

The opt-in `asn1typed_extract_physical_message` preserves the physical root component and expands its ordinary and specialized dependency closure. Module-owned registries retain class/set identities, ordered numeric dispatch rows, independently owned payload references, criticality and explicit presence availability. Bound field specializations retain validated selector-role ordinals and registry associations. Finalized evidence is revalidated before use; missing evidence is not numeric zero or an empty table.

The old flattened `asn1typed_extract_message` remains unchanged. Rows are not added to a physical message as invented fields. New scalars/arrays are additive and cleared through module ownership. No borrowed Parser/Fixer pointer survives deletion.

Independent review closed original-reference metadata sanitization, loss of inline constraints, accidental class DEFAULT acceptance, and stale registry-valid flags on malformed storage. Tests now reject these cases before publishing owned state. Frozen source inspection also corrected the study assumption about `NGAP-PROTOCOL-EXTENSION`: its real class has `&presence`. Both the existing three-role synthetic Extension form and the four-role real form are recognized structurally; availability is preserved rather than inferred or fabricated.

## Concrete validation

- Final isolated configured Typed IR regression: **18/18 PASS**. The new test uses REQUIRE under NDEBUG and covers known/empty Value and Extension registries, renamed physical fields, class/set identity, selector roles, lifecycle, invalidation and rejection diagnostics.
- Actual allocation failures are injected in the owned core and extractor translation units through test-only allocator aliases, including transactional row cloning and failed whole extraction. Parser/Fixer/common namespace allocations are deliberately not injected.
- New core/extractor strict warning checks passed, including `-Werror`, `-Wmissing-prototypes` and extractor `-Wcast-qual`. A whole-project `--enable-Werror` attempt encountered existing `libasn1fix/asn1fix_crange.c` missing-field-initializer warnings; the full regression therefore used the normal configuration, not an invented whole-project strict PASS.
- Focused core/extractor/test/Naming ASan/UBSan passed with `detect_leaks=0`; unchanged Parser/Fixer/common archives were not instrumented in that focused run. LeakSanitizer was attempted and failed to inspect `/proc/2/task` in this environment, so no successful LSan or whole-parser allocation qualification is claimed.
- Distribution checks include the fixture and both test-only allocator translation units. Whitespace checks passed.

## Untouched frozen NGAP probe

The six modules listed by `tools/qualification/ngap-rel18.modules` were parsed and fixed without schema editing. The command below exited zero; the driver deletes the Parser tree before inspecting owned metadata:

```sh
physical --module-list tools/qualification/ngap-rel18.modules \
  --asn1-root <frozen-ngap-root> --root-module NGAP-PDU-Contents \
  --message UEContextReleaseCommand
```

The physical closure contains **14 ordinary types, 6 bound instances and 4 validated registries**: the main UEContextReleaseCommand set has two rows, and the UE-NGAP-IDs, Cause and UE-NGAP-ID-pair extension sets are explicitly empty and extensible. The root has one `protocolIEs` field and valid root-only extension evidence. The main container owns SIZE 0..65535 and references its ProtocolIE-Field specialization; the Extension container owns SIZE 1..65535 and references ProtocolExtensionField. Both field bindings preserve id/criticality/selected-value ordinals 0/1/2 and the shared selector.

This probe establishes physical dependency/evidence readiness, not generated codec readiness or complete NGAP-PDU qualification. No benchmark was run. The old common namespace resolver has an allocation-failure defect observed during a broader preliminary injection attempt; it was not modified or counted as qualified by this milestone.
