# N10 — UEContextReleaseCommand body integration and qualification

## Scope and authority

N10 implements the accepted [body contract](ngap-cpp-aper-n10-body-contract.md)
on `feature/ngap-cpp-aper-first-message`, after N9-P4 commit
`d44a36aeb4282b89a859c69932724d0593499d37`. The contract was recorded in
`028c88e5847abf16b3c000110489a75508ddea88`.

The target is the actual `NGAP-PDU-Contents.UEContextReleaseCommand` **body**.
The six frozen Rel-18 modules are read without pruning or rewriting. Their
ordered paths, Git blob IDs and SHA-256 identities are recorded in
`tools/n10-body-qualification/source-manifest.json`; the upstream authority is
RAN commit `d6e514a33ec3695925c24aa292900514b371b074`.

No production IR, naming, renderer or runtime behavior is added by N10. The
generic developer probe gains an optional `--verify-determinism` flag. It
renders each header family twice from the same owned graph after Parser/Fixer
deletion and compares the complete outputs before opening output files. Its
default invocation and existing failure categories are preserved.

## Actual generated integration

The persistent target adapter and driver are under
`tools/n10-body-qualification/`. Every qualification invocation verifies all
six source identities, freshly generates the actual types/mapping/codec headers
in namespace `n10::body`, and strictly compiles the adapter, driver and runtime.
It does not accept an unrelated precompiled target driver.

The adapter derives the body container and entry types through generated
mapping traits. It finds the two known IOC rows by numeric ID: 114 for
`UE-NGAP-IDs`, and 15 for `Cause`. Compile-time checks protect payload types,
expected criticalities, mandatory presence and the three empty extensible
nested registries. Row storage position is never treated as a wire ID.

Received IE order, duplicates, empty/missing collections and criticality
mismatches are retained. These are codec values, not evidence of successful
NGAP procedure policy. Unknown scalar ENUMERATED extension indexes are
re-encodable. Opaque IOC payloads and unknown SEQUENCE sidecars remain owned
receive-only data and are explicitly refused on encode.

## Frozen independent evidence

The runner compiles every declaration of the six exact modules with pinned
pycrate **0.7.11**, rather than using an installed NGAP schema. The finite
accepted profile records source identities, all generated-header hashes,
individual case IDs, operations, byte lengths, byte hashes and exact oracle
limitations. A changed profile fails; the runner never updates acceptance.

The inspected matrix contains **375 native body cases** and **757 matched
checks**, with zero unresolved production disagreements:

| Operation | Checks |
|---|---:|
| Native body bytes → generated decode and full owned value comparison | 375 |
| Generated encode → exact native bytes and native value back-decode | 231 |
| Retained opaque payload → explicit encode refusal | 144 |
| Independent receive/refusal/strict-error byte literals | 7 |

Coverage includes all **81 declared Cause enumerators** across the five
enumerated branches, unknown scalar extension indexes, AMF/RAN integer-width
boundaries, all UE ID root branches, both IE orders, duplicate/missing/empty
collections, received criticalities, and unknown payloads of
1/3/127/128/16384/65536 octets in the main and nested empty registries. Each
decoded native model is checked after overwriting and releasing input storage;
copy/move tests also mutate opaque data independently.

### Oracle limitations, without normalization

There are **26 exact native-tool limitations**, not production disagreements:

- Two unknown SEQUENCE suffix API observations: one input spelling is rejected;
  another does not preserve the supplied suffix index. Independent root/pair
  sidecar literals test receiver ownership and encode refusal without claiming
  native SEQUENCE-addition agreement.
- Twenty-four 16K/64K fragmented known-open body cases fail pycrate's body
  self-decode. The runner independently parses canonical outer determinants,
  reassembles the original child octets, compares them byte-for-byte with native
  standalone `UE-NGAP-IDs` encoding, and confirms standalone native child
  decoding preserves the complete supplied value. Generated body decoding
  preserves all original values and opaque octets. Original sender bytes are
  neither rewritten nor normalized.

Opaque cases do not claim generated/native encode agreement. Strict known-open
padding/trailing rejection uses independently expected errors rather than
accepting a lax native decoder result.

## Focused target checks and closeout

The target driver's active `REQUIRE` checks run under `NDEBUG`. They cover
independent body literals, every known Cause value, malformed known payloads,
reserved selectors, ID/wrapper mismatch, over-wide AMF/RAN IDs, body alignment,
truncation and trailing data. Shared input/output/wire, collection, known-open
depth/staging, retained-unknown and bitmap budgets are checked at exact and
insufficient limits. Actual allocation failures are injected into body encode
and decode.

Generated body callbacks exercise atomic known-open rollback and sticky replay
through subsequent generated helpers, primitives and complete wrappers. The
first physical error and budget/cursor state are preserved; partial body values
are never published.

Focused nested-fragment checks use an independently constructed actual body
with a 16390-octet known child containing a 16384-octet opaque payload. They
compare the complete retained payload, verify copy/move independence and
encode refusal, and exercise exact/one-less depth, staging and retained-data
budgets. These complement, rather than duplicate, N9's primitive fragment suite.

| Executed check | Result |
|---|---|
| Actual generated adapter/driver, strict C++20 and `NDEBUG` | PASS |
| Frozen native/literal profile | 757 matched checks; no unresolved production disagreement |
| Existing Typed IR/generator regression | 19/19 PASS |
| Existing runtime regression | 4/4 PASS |
| Developer tool regression, including optional determinism/default output comparison | 1/1 PASS |
| Final target driver/runtime ASan+UBSan with leak detection disabled | PASS |
| Altered-source identity guard, before generation | PASS |
| Eight target tool files in fresh `distdir`, byte-for-byte | PASS |
| Independent design and final implementation/evidence review | PASS; no blocking findings |

LeakSanitizer was actually attempted on the final sanitized target binary.
It terminated with the environment's ptrace fatal error and supplies **no leak
verification result**. The successful ASan/UBSan execution uses
`ASAN_OPTIONS=detect_leaks=0`; it is not labeled a LeakSanitizer pass. The
sanitized scope is the generated target codec and runtime, not a new full
Parser/Fixer sanitizer qualification.

The final target driver source SHA-256 is
`011037e2b20ce8b1853af7d896c8005ec72dea0a25c370c347c931c86ef825e8`.
All source changes remain confined to target tools, optional probe verification
and repository evidence. No production codec/runtime behavior was changed.

This document makes no NGAP-PDU envelope, all-procedure, interoperability,
performance or full-schema qualification claim. No benchmark or whole-schema
adversarial scan was run. The later InitiatingMessage/NGAP-PDU envelope milestone
remains separate.
