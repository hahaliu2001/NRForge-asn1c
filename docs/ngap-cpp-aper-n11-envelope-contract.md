# N11 — Complete UEContextReleaseCommand PDU envelope contract

Date: 2026-10-09 (Owner local date). Status: **Accepted under continuous authority;
independent design review PASS**, under the
[continuous execution authority](ngap-cpp-aper-autonomous-execution-authority.md).
Baseline: `327dfe742cb894809bb8506dabbe016f6cbdce16` on
`feature/ngap-cpp-aper-first-message`.

## Target and frozen evidence

Complete the first message's `NGAP-PDU` / `InitiatingMessage` envelope, using
the unchanged six frozen Rel-18 modules and N10's actual body codec. Preserve
all existing extraction/rendering/runtime APIs and default developer-tool
output. No procedure policy, other typed procedure body, performance benchmark,
whole-schema qualification or schema pruning is included.

The source authority and ordered Git/SHA-256 identities remain N10's
`tools/n10-body-qualification/source-manifest.json`, from RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`. Verify them before generation and
fresh native compilation. No installed substitute schema is an oracle.

Actual source facts, independently checked before implementation:

- `NGAP-PDU` is an extensible CHOICE with three root alternatives:
  initiatingMessage, successfulOutcome and unsuccessfulOutcome. The extension
  marker declares no known additions. Root PER indexes must come from effective
  tag evidence, not source order or C++ variant indexes.
- Each root payload is a non-extensible three-field SEQUENCE: procedureCode,
  criticality, value. **InitiatingMessage has no SEQUENCE extension bit.** The
  fields select the corresponding elementary-procedure class field through
  `@procedureCode` from the full `NGAP-ELEMENTARY-PROCEDURES` object set.
- The procedure class is different from the N9 IE/extension classes. It has an
  initiating payload, optional outcomes, unique ProcedureCode and a criticality
  DEFAULT. Do not force this evidence into an N9 IE dispatch registry.
- `uEContextRelease` associates initiating `UEContextReleaseCommand`, successful
  `UEContextReleaseComplete`, numeric code **41**, and explicit **reject**.
  ProcedureCode has domain 0..255; Criticality has three non-extensible values.
- The independently constructed minimal N10 body remains
  `0000020072000400010002000f400140`. Its native complete PDU is
  `002900100000020072000400010002000f400140` (20 octets). These literals are
  expectations for tests, never implementation constants used to decide wire
  behavior.

## P1 — Separate owned envelope evidence

Add a separate owned `asn1typed_target_envelope_t` descriptor and opt-in
`asn1typed_extract_target_envelope` API. The unchanged physical-message API
continues to own and close the actual body graph; do not traverse every other
procedure body to claim unsupported generator capability.

The new descriptor owns:

- Full PDU identity, root declarations and source order, effective outer tag
  evidence, independent root PER mapping, extensibility and known-addition count.
- All three root SEQUENCE identities, their physical field spellings and role
  ordinals, class/set identities, selected payload class field and selector
  relation, plus procedure/criticality type references.
- The bounded procedure INTEGER and complete criticality ENUMERATED evidence.
- The complete fixed object-set row list, numeric/symbolic codes, expected
  criticalities, available default provenance, and present/absent payload
  references for initiating/successful/unsuccessful roles. References are owned
  source evidence; they do not advertise codec support for those bodies.
- Exactly one independently validated target body association, target row and
  initiating root identity/index. Do not select a row by declaration ordinal,
  inferred variant order or a hardcoded numeric constant.

Add clear/copy/finalize/validate and bounded construction operations. Validate
the complete supported descriptor shape before publishing finalized evidence:
three uniquely tagged roots, no known CHOICE additions, three non-extensible
physical role-correct fields per root, common class/set/scalar identities,
0..255 domain, complete three-value criticality mapping, unique codes, consistent
optional payload references and unique initiating target association. Class
DEFAULT processing requires actual source evidence and records unavailable
provenance explicitly rather than guessing.

Finalization is atomic; mutations invalidate proof. Public-struct changes require
revalidation. New extraction uses pending owned state, fails closed on ambiguous
or missing evidence and publishes no partial descriptor. Persist parser-deleted
lifecycle, deep copy, invalid/stale evidence, full-table and allocation-failure
tests. Old physical APIs and accepted body generation remain unchanged.

## P2 — Bounded typed envelope generation

Add separate target-envelope types/mapping/codec renderers taking the owned body
module, validated descriptor and namespace. Verify target body identity matches
the body graph and reuse shared final spelling; preflight every emitted
type/wrapper/helper/API name against body types/wrappers/mapping names.

Generated output is included after runtime and the same-body
types/mapping/codec outputs, from the same unchanged owned evidence and namespace.
It supplies complete encode/decode APIs and nested FieldReader/FieldWriter
helpers. The body payload uses its generated helper through N9's transactional
known-open API. There are no nested complete wrappers or independent contexts.

The owned value model distinguishes:

1. Initiating root: received procedure code and criticality, and a variant of
   the typed target body or an owned unsupported opaque payload.
2. Successful/unsuccessful roots: received code/criticality and owned opaque
   payload; these are not qualified typed outcome codecs.
3. Unknown outer CHOICE extension: original extension index and owned payload.

Decode selects the typed body only from the validated initiating root plus
target numeric code. Received criticality is preserved independently of expected
criticality. A malformed known target payload is a failure, **never** an opaque
fallback. All other determinably framed procedures/outcomes/extensions preserve
opaque bytes; this proves framing/ownership only, not payload validity or
application acceptance. Opaque values are explicitly refused on encode. A
typed-body wrapper with a mismatched code also fails; no rewriting or default
fabrication is permitted. Unknown scalar enums inside the typed body retain N10
re-encoding semantics.

Procedure membership still respects the owned object-set boundary: a listed but
unmaterialized procedure can be retained; an unlisted code can be retained only
when the set is extensible. A closed synthetic set must reject an unlisted code.
The actual NGAP procedure set is extensible. The scoped physical root shape is
procedure-code, criticality, payload in ordinals 0, 1, 2; a different physical
field order is rejected at evidence validation rather than silently reordered.

Existing runtime operations suffice: extensible enumerated selector for outer
CHOICE, constrained unsigned procedure code, generated criticality helper,
known-open transactions, and owned opaque open reads. Mapping supplies root
indexes, extensibility, scalar domains and target association. No constant
`41`, root-order assumption or handwritten byte template decides dispatch.

For the actual initiating target, the outer root selector consumes three bits,
ProcedureCode alignment adds five zero bits, code consumes one octet,
criticality consumes two bits, known-open alignment adds six zeros, then the
determinant and body follow. Complete final padding applies only once. Outer
CHOICE extension and inner body SEQUENCE extension mechanisms stay distinct.

## P3 — Complete PDU qualification and closeout

Freshly compile all six exact source modules with pinned pycrate 0.7.11. Generate
headers after Parser/Fixer deletion, repeat rendering from unchanged owned state,
strictly compile with active checks under NDEBUG, and compare full lengths,
bytes and owned values. Use N10's body matrix inside the complete envelope,
including every received outer criticality. Preserve native limitations without
rewriting bytes or relabeling independent literals as native agreement.

Cover all root framing branches, unknown/unmaterialized procedures, unknown
outer extension indexes (including 63/64 and larger), short/long/fragmented open
payload boundaries and known malformed-target refusal. Opaque bodies are
receive-only and independently checked after input overwrite/release and
copy/move. Test criticality preservation, code/wrapper mismatch, reserved root
and criticality selectors, alignment, truncation, inner complete padding and
trailing bytes, malformed determinants, and outer trailing data.

Exercise shared input/output/wire, collection, known-open depth/staging,
unknown-byte/record and extension-bitmap budgets at exact/insufficient limits,
including nested body transactions and fragment boundaries. Test actual
allocation failure, physical error offsets, sticky replay, atomic rollback and
no partial publication. Reuse N9/N10 exhaustive lower-level authority rather
than repeating unrelated scans.

Freeze exact case/operation/wire signatures, generated-header/source identities
and oracle limitations in a persistent accepted profile; changed evidence must
fail, not auto-update acceptance. Require focused ASan/UBSan, honest leak-check
limitations, relevant old regressions, distribution checks and independent
final review before autonomous commit/push. Closeout reports this bounded
complete-message milestone, not full NGAP or multi-vendor interoperability.
