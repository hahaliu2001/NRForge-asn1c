# N4 — Named ENUMERATED Generation Implementation Evidence

2026-10-09. **Complete / Accepted under continuous Owner authority; independent implementation review PASS.**
Branch: `feature/ngap-cpp-aper-first-message`.
Base / approved authority: `edbcd0746a9011695a03732f820b528cfc3dfdb2`.
Contracts: [N4](ngap-cpp-aper-n4-enumerated-generation-contract.md), [continuous authority](ngap-cpp-aper-autonomous-execution-authority.md).

## Implemented scope

Three enum-only C APIs generate types, mapping and codec separately. Every entry revalidates N3 evidence and common storage/shape/name preflight. Named ENUMERATED-only modules require 1..255 roots, resolved int64 assigned values and complete per-part indexes. Source order stays intact; default construction selects canonical root index zero.

Each schema type owns a distinct nested Known enum of actual assigned numbers. Extensible values use a variant of Known and nested UnknownExtension with full uint64 index. Known addition membership and wire indexes remain independent of signed assigned numbers. Decode canonicalizes known additions to Known; encode rejects fabricated Known casts and fake unknown indexes naming known additions through the runtime primitive, preserving sticky errors and whole-field atomicity.

Mapping emits std::array source entries and forward / independent root/addition reverse tables, including valid zero-length addition arrays. Codec field helpers only use FieldReader/FieldWriter and delegate to N2 enumerated primitives. Complete wrappers publish only after S3 final validation.

Existing renderer emission functions and runtime/IR code remain unchanged. Shared namespace safety is exposed through a forwarding internal function, with no Naming copy. Complete generated runtime and standard-library references are globally qualified. Types/mapping/codec inclusion order follows the contract.

## Independent review and fix closure

Read-only implementation review found one blocking legal-name case: user type Entry was shadowed by mapping's nested Entry. Fixed generically by computing and reusing globally qualified schema type names, rather than rejecting Entry or changing Naming. Added real fixture, metadata type assertion and complete codec vectors. Independent reproduction now compiles under strict C++20. Final review: PASS, no remaining blockers.

Review also prompted input-octet budget, NULL output, header-macro item and nested reserved-name regression additions. No production edit was made by the independent reviewer.

## Verification actually executed

| Check | Result |
| --- | --- |
| Autoreconf/configure and final libasn1typed check | 11/11 PASS |
| libaper check | 1/1 PASS |
| All three outputs rendered twice after Parser destruction | Byte-identical |
| Strict generated C++20 with Werror, conversion/sign-conversion and NDEBUG | PASS; REQUIRE checks remain active |
| Known identities/assigned endpoints/canonical defaults and both inverse mappings | PASS |
| Known roots/additions and unknown 0/1/63/64/255/256/UINT64_MAX, all residues | PASS against independent bit-list model, complete bytes/lengths and decoded values |
| Invalid Known/fake unknown, sticky helper/wrapper replay, truncation, malformed lengths/nonminimal values/spare roots/padding/trailing data | PASS |
| Input/output/wire budgets, exact and one less, singleton substitution | PASS |
| Generator scope/evidence/storage/name/namespace/NULL output rejection | PASS, NULL output and nonempty diagnostics |
| Every allocation point until success for each output, both fixture modules | PASS, transactional output and cleanup |
| Strict C11 renderer warnings incl conversion/sign-conversion/shadow/missing-prototypes | PASS, independently checked |
| Focused instrumented generator/coreIR/Naming/slice test driver ASan+UBSan, no recovery | PASS; linked uninstrumented extraction/Parser/Fixer/common archives |
| Generated C++ harness and runtime ASan+UBSan, no recovery | PASS |
| LeakSanitizer attempts for C driver and C++ harness | Environment fatal /proc task access and ptrace restriction; no usable leak verdict |
| Distribution directory | All six new renderer/test/script/fixture files included and byte-identical |
| Diff and new-file whitespace | PASS |

Additional native external comparison: pycrate 0.7.11 encoded and decoded the same ENUMERATED schemas wrapped in Boolean-prefix sequences. The actual generated C++ field codecs were exercised at residues 0..7. **160 cases, zero failures**, full bytes/lengths compared, C++ decode checked assigned Known values or exact UnknownExtension indexes, then external decode checked C++ output. Cases include negative/sparse/reordered roots, known addition below the largest root and unknown indexes through UINT64_MAX. No oracle wire output was rewritten or normalized. Scratch driver/schema/comparison/result are retained outside the repository; the persistent suite uses its independent bit-list reference without mandatory Python dependencies. This validates standalone enum field codecs, not generated SEQUENCE or NGAP integration.

The focused C generator sanitizer boundary above does not imply Parser/Fixer/extraction were instrumented or qualified. ASan/UBSan runs disabled leak detection after the separate LSan attempts failed. Existing Parser issues recorded under N3 remain out of scope.

## Delivery and subsequent boundary

Implementation, regression sources and this evidence are committed and uploaded under continuous authority after PASS. No benchmark, full frozen-schema qualification, real-message codec, inline enum or compound enum integration was performed. These remain subsequent work.

Previously local N3 contract/implementation and continuous-authority/N4-contract commits were uploaded through GitHub's connected object tools because CLI write credentials are absent. Connector-created commits have different author metadata/SHA; evidence references now point to their canonical remote commits. Local earlier commits remain preserved on archive/n3-local-before-connector-upload. The remote feature branch was fast-forwarded with an expected-head lease; no remote force push or merge was performed.
