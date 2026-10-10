# N11-P2 — Metadata-driven target PDU generation

## Implementation

P2 follows the accepted [N11 contract](ngap-cpp-aper-n11-envelope-contract.md)
and owned-evidence commit `b2e1cbcf0b487ea3499f4c882e629b7a08ea9963`.
Three new opt-in renderer entry points generate target envelope types, mapping
and codec from the same unchanged body graph, validated envelope descriptor and
namespace. Include runtime, the three body headers, then envelope types,
mapping and codec. Existing renderer entry points are preserved.

The generated model retains source-ordered root wrappers, received ProcedureCode
and Criticality, a typed target-body wrapper, owned unsupported payloads, and
owned outer CHOICE extensions. Criticality is an envelope-scoped scalar derived
from the descriptor; it does not assume that the body graph declares it.
Mapping exposes both root/PER directions, field member pointers, the complete
procedure table and the target association. Wire behavior does not depend on
fixture names, code 41 or variant storage indexes.

Decode interprets only the validated initiating target association as a typed
body. Other determinably framed roots/codes/extensions retain owned bytes;
unlisted codes are rejected when the procedure set is closed. Malformed known
target bodies fail, without opaque fallback. Encoding accepts only the typed
initiating target with its matching code; opaque representations fail through
the existing sticky error mechanism. Received criticality is preserved, not
replaced by the procedure's expected policy.

Known body framing uses FieldReader/FieldWriter helpers and the same call
context, not nested complete-codec calls. The runtime API and implementation
are unchanged. Name checking reuses final spelling and the existing complete
body symbol preflight. An initially identified module/type concatenation defect
was corrected to use the same component separator as the body generator.

The developer probe optionally accepts the three envelope flags together and
preflights/repeats all six requested renderings after Parser deletion, before
opening files. Without those flags, its existing output/failure behavior is
preserved. Identical body/envelope prefixes are rejected before parsing. Output
I/O errors may leave earlier files; no atomic filesystem publication is claimed.

## Verification and review

The isolated Typed IR suite passed 21/21 and runtime suite 4/4. Persistent
synthetic tests cover deterministic generation, null/stale/mismatched evidence,
full symbol/member/macro collisions, allocation failures, code 73, reversed
root tags, and an ABC/DEF acronym boundary through actual strict C++ compilation.
Focused new renderer paths passed ASan/UBSan with no recovery and leak detection
disabled. LSan availability is documented in P1 and final qualification evidence.

Fresh actual six-module extraction produced all six headers. All three body
headers remained byte-identical to N10. Strict C++20 target self-tests passed,
as did separately generated manual owned-IR variants using code 73 and tags
9/5/2, and a closed procedure set. Those variants are explicitly manual metadata
regressions, not claims about the untouched NGAP source.

Independent P2 review closed the spelling issue and accepted descriptor-driven
mapping, ownership/refusal, shared transactional helpers and persistent tests.
Complete-PDU external qualification and final evidence are recorded separately
in N11-P3; this document alone is not that qualification claim.
