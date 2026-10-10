# N11-P1 — Owned target-envelope evidence

## Accepted scope

This implements P1 of the [N11 contract](ngap-cpp-aper-n11-envelope-contract.md),
recorded in `470c598debe1cc3f1977528b9a4c2ff132f68f2b`, on
`feature/ngap-cpp-aper-first-message`. It adds a separate owned envelope
descriptor and an opt-in extractor. Existing IE registries and body extraction
are unchanged; this does not generate or qualify an outer codec yet.

The descriptor owns the three NGAP-PDU root associations, effective tags and
canonical PER ranks; sequence field roles and source identities; ProcedureCode
and Criticality domains; the complete procedure table and explicit/class-default
criticality provenance; and the unique initiating target-body association.
Other table payload references are metadata, not materialized codecs.

## Source proof and lifecycle

Fresh Parser/Fixer extraction from all six untouched frozen modules produced
three roots and 81 unique procedure rows. The set is extensible with no declared
additions. The target row is code 41, expected reject, initiating
UEContextReleaseCommand and successful UEContextReleaseComplete, with no
unsuccessful payload. Root source order and PER ranks are initiating 0,
successful 1 and unsuccessful 2. After deleting the Parser tree, deep copying
and clearing the original descriptor, validation still passed. Existing body
extraction remained 14 ordinary types, six bound instances and four IE registries.
Source identities are the N10 six-module manifest and RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`.

Fixer constraint pullup does not preserve the outer object-set extension flag
on the flattened table. The new extractor therefore checks the retained declared
constraint: only an exact top-level root/terminal-extension pair proves an
extensible set. Supported closed UNION/SET and single-child CSV wrappers may be
expanded by Fixer; nested markers, additions and other operators are rejected.
The independently validated fixed table supplies rows, never the extension
boundary. No schema or Fixer changes were made.

Successful descriptor mutations invalidate all published indexes and target
proof. Finalization publishes proof only after complete validation; malformed
storage, stale indexes and unavailable evidence fail closed. Deep-copy and
mutator allocation failures are atomic. Direct public-struct edits require
validation before use.

## Verification and independent review

The focused persistent test runs under NDEBUG with REQUIRE checks. It covers
renamed code 73 and reversed root tags, source order, explicit and class-default
criticality, closed and extensible union sets, Parser deletion, deep copying,
malformed metadata/storage, stale proof and allocation-failure sweeps.

The isolated Typed IR regression suite passed 20/20 before the final bounded
set-wrapper refinement; the updated focused test passed thereafter. A focused
ASan/UBSan build of the new core/extractor test paths passed with no recovery
and leak detection disabled. LeakSanitizer was attempted with leak detection
enabled and failed because it could not inspect processes under ptrace; no
LSan success is claimed. Whitespace checks passed.

Independent P1 review accepted the owned representation, mutation/finalization
behavior and exact source-boundary recovery after checking the real 81-row,
extensible-set result. P2 generation and P3 complete-PDU qualification remain
separate gates within N11.
