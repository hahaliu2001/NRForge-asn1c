# RRC-P4 — ordinary extension additions/groups and CHOICE evidence

Baseline: `97d099f4edfb2d463bd564dc5cec8e586a81c8e4` (accepted P3).
Branch: `feature/f1ap-cpp-aper`. This batch owns semantic structure only.
Frozen six-module source and four bare fixtures remain unchanged. No merge,
performance benchmark, UPER runtime, generated RRC C++ or SDK delivery.
Final checks/review are in `tools/rrc-readiness/verification-rrc-p4.json` and
`tools/rrc-readiness/review-rrc-p4.md`.

## Owned ordinary structure

Only `asn1typed_extract_root_graph` enables known ordinary additions. Legacy
whole-module/IOC/envelope APIs still reject them; existing root-only SEQUENCE
APER evidence setters keep their original zero-known-additions contract.
The new `ordinary_structure_owned`, root/addition counts, policy and validation
flag are independent evidence. Each SEQUENCE field owns its ROOT, ADDITION or
GROUP role and a separate zero-based root/extension component index. Root order
is physical field order. A known addition remains semantically MANDATORY,
OPTIONAL or DEFAULT inside its payload; outer extension presence does not
change it into an ordinary OPTIONAL field.

A `[[...]]` SEQUENCE group occupies exactly **one** outer addition slot, never
one per inner field. Its optional outer field references a separately owned
anonymous SEQUENCE, retaining inner mandatory/optional/default fields. A stable
`@groupN` segment uses its outer addition ordinal; `$inline$` dependency keys
retain module/owner identity. Nested groups inside an extended inner SEQUENCE
own another bitmap boundary. Root/addition/group references, locations,
DEFAULT values and indexes remain independent after fixed-tree deletion.
Alias bodies receive their own inline dependency paths. Traversal still charges
all dependencies and AST nodes against the inherited P2 budgets, with no partial
published graph after failure.

The parser now explicitly marks `ComponentTypeLists` group wrappers. Clone and
compare retain that provenance. Ordinary extraction rejects malformed provenance,
groups outside an addition list and repeated retained extension markers. It does
not infer a group from an unnamed optional SEQUENCE. No source normalization,
AST mutation, hand-expanded RRC messages or flattening is introduced.

P1's **640** lexical bracket pairs are not 640 SEQUENCE wire groups. Fresh
source-declaration traversal owns **634** explicit SEQUENCE wrappers: NR 580,
PC5 8, preconfiguration 6, discovery 1, inter-node 39, UE variables 0. The other
six bracket lists are NR CHOICE alternatives at source lines 8472, 11529, 11546,
11563, 11767 and 11792. Parser `AlternativeTypeLists` preserves the individual
alternatives; these brackets do not create a SEQUENCE bitmap slot. The focused
future-CHOICE fixture exercises both bracketed lists and individual additions.
These whole-source shape counts do not claim successful closure of every group.

## CHOICE and future extensions

Ordinary CHOICE owns root/addition counts and per-alternative addition roles.
Root PER indexes require unique, resolved effective source tags and follow the
inherited canonical tag rank. Extension indexes are separate, zero-based and
follow declaration order, independent of their tags. Appending a future known
branch preserves all earlier root/extension indexes. A root must exist. Missing
or duplicate root tag evidence does not produce guessed indexes: finalization
returns UNAVAILABLE and clears indexes/validation, while supported semantic
extraction may still succeed. The readiness report lists these unqualified
CHOICE mappings separately. Known additions never set the legacy root-only
CHOICE marker or its legacy full-root mapping flag.

The owned future decode contract is **skip unknown SEQUENCE open additions after
validating framing**, and **preserve unknown CHOICE extension index plus opaque
payload**. A future CHOICE cannot silently become a root alternative, NULL, or
DEFAULT. Non-extensible types own policy NONE. These policies are IR contracts;
there is no unknown-open-type decoder, forwarding encoder, padding decision or
wire proof in P4. P5/P6 must implement and test the framing, budgets and rollback.
Existing APER sequence-extension generation rejects known ordinary additions;
focused tests check rejection for both SEQUENCE and CHOICE graphs.

`asn1typed_ordinary_extension_structure_finalize/validate` recheck storage,
counts, roles, indexes, tag ranks and policy. Field copying preserves component
metadata and deep-copies references; field/alternative/tag mutation invalidates
validation. UNAVAILABLE clears the entire accessible index mapping. ERROR also
invalidates type flags and clears indices within accessible count/capacity
bounds; corrupt storage must be repaired before use. These functions validate a
local group's reference shape; root-graph closure and the recorder additionally
verify its target is an owned SEQUENCE. They do not prove wire compatibility.

## Frozen matrix and remaining gaps

Same 80 deduplicated P1 roots; no filtering of optional fields or extension paths.
Five P3 DEFAULT seeds remain outside the denominator and all still pass.

| Scope | P3 PASS / FAIL | P4 PASS / FAIL |
|---|---:|---:|
| Selected roots | 45 / 35 | **64 / 16** |
| Channels | 5 / 7 | **8 / 4** |
| Payloads | 40 / 26 | **54 / 12** |
| Four overlapping bare seeds | 1 / 3 | **3 / 1** |

Nineteen new full closures pass, with no P3 success regression. MTC and SIB2 now
pass semantic extraction. MIB continues to pass; SIB1 still fails at an inline
constrained collection in PCCH-Config. All 31 previous first addition/group or
CHOICE-addition blockers are removed; later gaps remain visible.

| Remaining first gap | Roots |
|---|---:|
| Inline SIZE/use-site constraints | 10 |
| Primitive constraints | 4 |
| Inline INTEGER constraint | 1 |
| Constrained constructed alias | 1 |

Successful closures contain 100 distinct extensible SEQUENCE identities, 46
distinct outer group fields and 16 extensible CHOICE identities. Ten inherited
SetupRelease instances still lack root tag evidence; their successful semantic
closures are **not** qualified CHOICE wire mappings. All 13 distinct P3 DEFAULT
seed fields retain fc4 / 4 or dB0 / 15. `readiness-rrc-p4.json` stores full owned
type bodies once, reconciles identical identities across roots and retains each
root's ordered closure membership. Its raw probe hash pins the complete
post-tree-deletion output before report normalization.

## Validation and boundaries

Focused tests cover direct/grouped additions, mandatory/optional/default group
fields, nested groups, aliases, empty roots, terminal markers, explicit parser
provenance clone/compare, malformed group flags, limits, legacy rejection,
CHOICE tag ordering, bracket lists, appended future branches, invalid/duplicate/
unavailable evidence, deep copy and failure cleanup. Every extraction allocation
is failed in turn: Root **83**, NestedPick **79** positions. Inherited parameter
and DEFAULT OOM checks remain **366/62**; the ordinary P2 graph now has **139**
positions (the new allocation-free ordinary CHOICE finalizer removes one).

Strict GNU C11 uses `-Wall -Wextra -Werror`. Typed suite **45/45**, parser **1/1**,
APER runtime **10/10**, PDU **3/3** pass. ASan/UBSan covers focused tests and all
85 requested RRC roots, instrumenting typed core/extractor, parser expression
clone/compare and (for the root probe) developer-tree glue. Remaining parser
sources including generated grammar, fixer and remaining libraries are ordinary
builds. `detect_leaks=0` is explicit; no completed leak scan is claimed. Fresh P1
probe/scanner preserves historical P1 report bytes. Frozen input authentication
occurs before each matrix run.

The separate APER artifact receipt guards all changed production sources and
regenerates all 361 BODY/target-envelope families (F1AP 158, NGAP 131, E1AP 72),
comparing all 1773 available historical headers. NGAP historical hashes cover
BODY only; its envelopes are generated without historical byte comparison.
This gate does not rerun complete installed SDKs, all-message C++ compilation or
wire corpora. New RRC C++/UPER/wheel/install checks remain NOT_RUN.

RRC-P5 is the next planned UPER runtime batch. The remaining primitive/use-site/
alias gaps must stay explicit during subsequent rendering coverage; they have
not been patched per message or folded into this extension batch.
