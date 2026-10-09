# N5 — Named Constrained INTEGER Generation Evidence

2026-10-09. **Complete / Accepted under continuous Owner authority; independent implementation review PASS.**
Branch: `feature/ngap-cpp-aper-first-message`.
Base: `93bdc5955b49b675a214ddd373de372358ab1f26`.
Contracts: [N5](ngap-cpp-aper-n5-constrained-integer-generation-contract.md), [N1](ngap-cpp-aper-n1-constrained-integer-contract.md), [continuous authority](ngap-cpp-aper-autonomous-execution-authority.md).

## Implemented scope

Three standalone named INTEGER-only APIs emit types, mapping and codec independently. Each common preflight validates storage, identity, exact constraint equality, unsupported metadata and final scoped names before output. The accepted non-extensible zero-based domains are exactly 8/16/32/40 bits; mixed modules and unsupported domains are rejected rather than filtered.

Types use uint64 aliases and bounds; aliases do not establish distinct schema identities. Mapping derives root bits and fixed/variable payload metadata from the actual Owned IR constraint. The 32/40-bit payloads remain minimal-length 1..4/1..5 octets, with a 2/3-bit length prefix before octet alignment. No fixed encoded-length or per-type final-padding metadata is invented.

Generated FieldReader/FieldWriter helpers delegate directly to N1 constrained integer primitives without narrowing or local non-sticky range errors. Complete wrappers use existing S3 publication/padding/trailing rules. Shared final Naming and namespace validation, globally qualified types/runtime/standard references and scoped collision checks are used. Existing runtime, IR, Naming and previous renderer implementations remain unchanged.

## Executed verification

| Check | Result |
| --- | --- |
| Independent contract review before production implementation | PASS |
| Focused N5 generated suite and final libasn1typed check | 12/12 PASS |
| libaper check | 1/1 PASS |
| Parser/Fixer extraction, tree destruction, three outputs rendered twice | Byte-identical, both original and renamed modules |
| Strict generated C++20, Werror/conversion/sign-conversion/pedantic-errors, NDEBUG | PASS; REQUIRE checks remain active |
| uint64 aliases, bounds, all mapping metadata and zero initialization | PASS |
| Independent bit-list model, complete bytes/length/decoded values, payload transitions and residues 0..7 | PASS |
| Domain overflow without narrowing, helper/wrapper sticky replay | PASS |
| Bit-bounded truncation, alignment first-nonzero offsets, short byte prefixes, trailing data, spare 40-bit selectors and nonminimal payloads | PASS |
| Exact/one-less wire budgets at every residue; complete input/output/wire budgets | PASS; primitive failure cursor/counters unchanged |
| Unsupported domain/shape/metadata, naming/namespace/macros and NULL argument/output rejection | PASS, NULL output and nonempty diagnostics |
| Every allocation point until success for all three outputs and both fixture modules | PASS |
| Strict renderer C11 with Werror, conversion/sign-conversion/shadow/missing-prototypes | PASS |
| Focused instrumented C generator/coreIR/Naming/slice/test driver ASan+UBSan, no recovery | PASS; existing extraction/Parser/Fixer/common archives uninstrumented |
| Strict generated C++ harness plus runtime ASan+UBSan, no recovery | PASS |
| LeakSanitizer attempts for C and C++ harnesses | Fatal proc/ptrace environment restriction; no usable leak verdict |
| Distribution directory | Final renderer, driver, generated harness/script, both fixtures and Makefile.am included and byte-identical |
| Diff and new-file whitespace | PASS |

Native external comparison exercised the actual generated field codecs against **asn1tools 0.167.0 APER** in both directions. **66,272 cases, zero failures**: exhaustive 8/16-bit values at residue zero, boundary/payload transitions at residues 0..7, and deterministic 32/40-bit samples. Complete byte strings and external decoded values were compared without normalization or rewriting oracle output. The external schema uses Boolean-prefix wrappers solely to establish cursor residues; this qualifies the standalone generated integer field layout, not generated compound or NGAP codecs. Scratch driver/comparison/result are retained outside the repository; persistent tests use an independent bit-list model without a mandatory Python dependency.

The sanitizer boundary does not qualify extraction/Parser/Fixer. ASan/UBSan runs disabled leak detection only after separate LSan attempts failed; no leak-clean claim is made.

## Review and delivery

Independent implementation review: **PASS, no blocking findings**. The reviewer independently generated all three headers for a hand-built 32-bit ValueType under foo::nrforge, compiled them with the runtime under strict C++20, and verified maximum encoding c0ffffffff, decoding and max+1 constraint_violation at offset zero. Review prompted three small test additions for module count/capacity, mismatched type identity and nested standard-header macro namespace; all reject with specific diagnostics. No production change was needed. The updated focused generated suite passed again. Contract, implementation, regression sources and this evidence are delivered under continuous authority after finding closure. No benchmark, inline/compound integration, frozen schema editing, complete-message codec or NGAP qualification was performed.
