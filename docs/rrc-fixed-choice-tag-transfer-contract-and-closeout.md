# RRC-P13-A: existing fixed parameterized CHOICE tag evidence

Baseline: `fe2678655041ad9a3bb8ddce3759dc6f9c6be5d5` (RRC-P12).
Branch: `feature/f1ap-cpp-aper`. This batch does not merge or run benchmarks.

## Contract

The ordinary extractor continues to populate semantic shapes, constraints and
substituted type identities from the generic template. For a parameterized
CHOICE it now reads existing tags from an exact fixed specialization of that
same template. Matching uses every named actual's lexical module/name identity;
clone names, specialization order and empty actual reference caches do not
identify a specialization. Source-use cache consistency checks remain active.

A borrowed fixed clone must have the template's module, CHOICE kind, member
names/order and extension partitions. Formal reference members must resolve to
the bound actual's complete namespace identity. Nonformal references must retain
their lexical target and cache consistency; nested parameter references must
retain every substituted actual identity. Duplicate matching instances,
malformed specialization storage, wrong clones and conflicting generic/fixed
tags fail closed. The fixer's retained parameter structures are not treated as
unbound semantics. All borrowed state expires during extraction; the owned IR
retains tag class/number and finalized indexes only. The AST is never changed.

Only a generic member with no tag borrows a fixed member's tag. Existing generic
tags remain authoritative and conflicting fixed tags are rejected. Missing or
unsupported tags remain unavailable through the existing validator; duplicate
tags do not produce usable indexes. Root ordering is finalized by effective tag
class/number, while extension indexes keep the existing addition rules. No
setup/release indexes, automatic tags or tagging-mode assumptions are synthesized.
No public IR, renderer, runtime, schema or SDK surface changes are included.

## Verification and resulting readiness

`tools/rrc-fixed-choice/check.py` binds the current producer, authenticates the
frozen six-file RRC schema before and after execution, and records fresh outputs.
The focused C checks cover exact identities, nullable actual caches, reordered
specializations, corrupt storage, wrong clones, conflicting/duplicate/invalid
tags, explicit/implicit tagging, extensions and owned lifetime. Existing ordinary
parameter/default tests retain 428 allocation-failure positions. Six generated
fixture roots exercise 12 hand-derived goldens under three completion modes,
including reversed context-tag order and application/context ordering, negative
values/unset choices, truncated indexes/open types and a value-octet quota.
Direct extraction and focused checks run with ASan/UBSan; leak scanning cannot
run in this environment because `/proc` task enumeration is unavailable.

The current 85-root matrix has 74 extraction/text-rendering PASS, eight extraction
blocks and three rendering blocks. All 67 P12 accepted physical headers remain
byte-identical. Seven formerly blocked roots now produce headers:

| Root | Strict C++20 compilation in P13-A |
| --- | --- |
| BCCH-DL-SCH-Message | PASS |
| DL-CCCH-Message | PASS |
| LocationMeasurementIndication | BLOCKED_EXISTING_RENDERER |
| LoggedMeasurementConfiguration-r16 | PASS |
| RRCReestablishment | PASS |
| RRCSetup | PASS |
| SIB1 | PASS |

LocationMeasurementIndication exposes the existing NULL extension open-type
lambda's unused `io` parameter under `-Werror`. The exact two compiler errors
are retained in readiness.json. Its text-rendering PASS is **not** a strict
compile, wire-qualified or SDK delivery PASS. Renderer repair is a separate
follow-up; this batch does not suppress warnings or modify renderer output.

The instance check binds P10's ten named SetupRelease contexts to current owned
metadata. LocationMeasurementInfo and SL-L2RemoteUE-Config-r17 now have the
existing validated fixed tags; the other eight still have no usable indexes.
This explains why RemoteUEInformationSidelink-r17 can remain blocked despite
one of its instances receiving valid tags. The full matrix records every error.

All 65 native tests pass. The shared APER regression covers F1AP 158, NGAP 131
and E1AP 72 messages, comparing 1,773 accepted physical header files byte for
byte. Native outcomes, source hashes, binary hashes and log hashes are frozen in
the verification receipts and independently reviewed before publication.

## Remaining scope

The P8/P9 SDK remains at 64 delivered and wire-qualified roots. Newly generated
real RRC roots receive no new wire qualification here. P13-B addresses missing
fixed tag evidence under its own contract; P14/P15 handle opaque CONTAINING and
constructed aliases; P16-P18 retain new wire qualification and SDK expansion.
The newly exposed NULL-extension renderer warning also requires a separate fix.
Historical P9-P12 artifacts remain unchanged.
