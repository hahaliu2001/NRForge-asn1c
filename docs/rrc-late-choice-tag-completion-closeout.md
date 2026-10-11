# RRC-P13-B: complete late fixed CHOICE tags

Baseline: `698ba246456d785a6c72ab3e39c0a821813e767c` (RRC-P13-A).
Branch: `feature/f1ap-cpp-aper`. Primary objective: correct final tag scheduling
for late-created fixed parameterized CHOICE trees. See
`tools/rrc-late-choice/contract.md` for the independently approved contract.

## Cause and repair

`asn1f_process` runs phase I completely for each module before proceeding to the
next. The template owner's tag decision and automatic tagging can finish before
a later module instantiates that template. Phase II does not repeat those tag
passes. The minimal Templates/Consumer fixture reproduced fixer success with
untagged Pick<Consumer.Local> and NestedChoice<Consumer.Local>, auto_tags_OK=0.
The frozen RRC schema has the same eight late instance contexts identified in
P10/P13-A. This batch repairs their fixed evidence at its source.

Only `libasn1fix/asn1fix.c` changes production behavior. After normal phases,
it walks actual fixed-specialization subtrees under each template owner's
namespace. CHOICE root tags are validated with the existing rule; bodies not
already marked automatic reuse the existing member-tag decision and automatic
tagging. Existing automatic bodies are not retagged. Explicit and mixed-tagged
bodies retain the existing module-default semantics and canonical tag ordering.
Nested CHOICE nodes in specialization trees are included; generic source bodies
and SEQUENCE/SET completion are not added to this pass.

Tag lookups are prewarmed with the same outmost-tag path and flags as distinct
validation. Completion/lookup repeats across all modules until the specialization
count stabilizes, then existing distinct validation runs. If validation adds an
instance the completion/validation cycle repeats. Real errors are not swallowed.
Array entries are reloaded after callbacks because lookup may reallocate them.
This is tag scheduling, not a new tagging algorithm or branch-index assignment.
P13-A extraction continues consuming validated fixed tags unchanged.

## Evidence and limits

Focused checks verify template-owner AUTOMATIC/EXPLICIT/IMPLICIT modes despite
different consumer defaults, qualified actuals, nested specialization and inline
CHOICE, extension additions, explicit/mixed tags and CHOICE actuals requiring
EXPLICIT tagging. Reversed module order yields the same seven physical codec
headers. Generic templates remain untagged and late SEQUENCE bodies remain
outside this repair. Owned metadata is checked after AST destruction. Illegal
IMPLICIT root CHOICE, duplicate explicit tags and tagged extensions with untagged
roots fail the fixer. The legacy fixer can permit an unresolved actual, but
existing typed extraction still rejects that graph; this check is preserved.

Seven strictly compiled fixture roots exercise fourteen hand-derived golden
encodings in three completion modes, plus value/unset-choice rejection,
truncation and quota failures. Direct ASan/UBSan covers the current fixer,
tag helpers and focused extraction test; LSan is NOT_RUN because the environment
cannot enumerate process tasks. Native regression includes 49 typed tests,
two fixer test programs (including the existing full compiler-fixture suite),
three UPER, ten APER, three NGAP and one parser test: 68 total PASS.

The fresh frozen 85-root readiness matrix is 77 text-rendering PASS and eight
extraction blocks, with no remaining rendering blocks. All 74 P13-A accepted
physical headers remain byte-identical. The newly rendered roots are:

| Module | Root | Strict C++20 |
| --- | --- | --- |
| PC5-RRC-Definitions | RRCReconfigurationSidelink | PASS |
| PC5-RRC-Definitions | RemoteUEInformationSidelink-r17 | PASS |
| PC5-RRC-Definitions | SCCH-Message | PASS |

All ten P10 named instance contexts now have usable fixed/owned tags. The eight
newly completed contexts retain full NR/PC5 actual identities. Text rendering
is not wire qualification: LocationMeasurementIndication still retains its
P13-A renderer compile block (two unused-io errors), and its header is unchanged.
The shared APER regression verifies F1AP 158/948 physical headers, NGAP 131/393,
and E1AP 72/432: 361 messages and 1773 byte-identical accepted header files.
Verification receipts bind source, current archives/binaries and logs, followed
by independent exact-snapshot acceptance before commit/push.

No extractor, public IR, schema, renderer, runtime or SDK implementation changes.
The P8/P9 SDK and wire-qualified profile stay at 64 roots. P14/P15 address opaque
CONTAINING and constructed aliases; P16-P18 qualify and deliver an expanded finite
profile. The known NULL-extension renderer warning needs a separate repair.
Historical receipts remain unchanged. No benchmark and no merge.
