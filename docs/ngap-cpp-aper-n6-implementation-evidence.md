# N6 — Mixed Compound Generation Implementation Evidence

2026-10-09. **Complete / Accepted under continuous authority; independent contract and implementation reviews PASS.**
Branch: `feature/ngap-cpp-aper-first-message`.
Base: `38cb53c36ff4c93547c0a6d9291a53da4c7fff39`.
Contract: [N6](ngap-cpp-aper-n6-compound-generation-contract.md).

## Implemented boundary

New standalone compound types/mapping/codec APIs accept mixed named BOOLEAN, the exact N5 INTEGER domains, N4 ENUMERATED values, non-extensible root CHOICE with 1..255 alternatives, and non-extensible fixed SEQUENCE with mandatory/OPTIONAL fields. References must be to earlier types in the same module or inline unconstrained BOOLEAN. Existing runtime, IR, Naming and renderer implementations remain unchanged.

Read-only single-type views delegate INTEGER/ENUMERATED output to N5/N4, including their distinct enum Known/UnknownExtension model and full uint64 unknown indexes. Compound validation and globally scoped collision checks happen before emission. Standard includes remain outside user namespaces; planned references are globally qualified.

CHOICE wrappers/variant order retain Owned IR source order. Validated tag evidence supplies independent forward/reverse root permutations; selector encoding delegates to the non-extensible N2 small-range primitive. Ordinal-only mapping aliases associate actual wrapper, payload and payload mapping. SEQUENCE emits the complete OPTIONAL bitmap first and present fields in declaration order. Nested helpers never finalize and complete wrappers never publish partial objects/bytes.

DEFAULT/CONDITIONAL, sequence/choice extensions, collections, IOC/open type/bound instances, inline INTEGER/ENUMERATED, forward/self/external references and unsupported metadata remain rejected. No supported graph is obtained by filtering away schema members.

## Finding closure and independent review

Initial source inspection found that reference lookup could scan a later unvalidated NULL type identity. Lookup now scans only already validated earlier owners; the missing-reference/future-NULL negative case checks safe rejection in every entry. No IR/runtime change was needed.

Independent implementation review: **PASS, no blocking findings**. The reviewer inspected validation, delegation, permutations, OPTIONAL ordering, qualification and cleanup, independently executed the latest strict generated compound test script, and checked strict renderer C11 warnings. Review did not alter production or test code.

## Executed verification

| Check | Result |
| --- | --- |
| New focused generated compound suite | PASS |
| Final libasn1typed check / libaper check | 13/13 PASS / 1/1 PASS |
| Real Parser/Fixer fixture extraction, tree deletion and three outputs rendered twice | Byte-identical, canonical and fully renamed/reversed modules |
| Strict generated C++20 with Werror, pedantic-errors, conversion/sign-conversion and NDEBUG | PASS; REQUIRE checks remain active |
| Mixed primitive metadata, root permutation inverses, wrapper/payload associations and sequence bitmap ordinals | PASS |
| 3/6/1 root CHOICE, duplicate C++ payload types, nested compounds, OPTIONAL absent/false/true and empty SEQUENCE | PASS against independent recursive bit-list model |
| Known enum additions and unknown indexes through UINT64_MAX; variable wide integers; all residues 0..7 | Full bytes, lengths, metrics and decoded semantics agree |
| Spare root selectors, nested integer overflow/fabricated enum/fake unknown and sticky replay | PASS with concrete error offsets |
| Every short byte prefix, alignment/final padding individual bit flips, trailing bytes | PASS with concrete offsets |
| Complete input/output/wire budgets exact and one less | PASS; N1/N2/S3 retain deeper primitive atomic/error-priority coverage |
| Shape/storage/identity/evidence/ref/actual/IOC/collection/extension/normalized collision/namespace/NULL rejects | PASS, transactional NULL output and diagnostics |
| All allocation points until success, each output and both modules | PASS |
| New renderer strict C11, Werror/conversion/sign-conversion/shadow/missing-prototypes | PASS |
| Focused C generator/coreIR/Naming/slice/enum/uint/test driver ASan+UBSan, no recovery | PASS; extraction/Parser/Fixer/common archives uninstrumented |
| Generated C++ compound harness and runtime ASan+UBSan, no recovery | PASS |
| Separate LeakSanitizer attempts, C and C++ | Fatal proc/ptrace environment restriction; no usable leak verdict |
| Clean final distribution directory | Renderer, driver, harness/script, both fixtures and Makefile.am byte-identical |
| Diff/new-file whitespace | PASS |

An old generated enum-evidence executable initially lacked execute permission. Its verified ELF artifact permission was restored, and the full typed suite was rerun successfully; no tracked source changed to bypass the failure.

## External comparison and oracle limitation

Native **pycrate 0.7.11 APER** compared actual generated compound codecs in both directions: **768 cases, zero differences**. The AUTOMATIC TAGS schema was compiled without pruning; Boolean-prefix wrappers establish residues 0..7. Cases exercise all six alternatives, all three nested Tri alternatives, nested OPTIONAL bitmaps, enum roots/known additions/unknown indexes through UINT64_MAX, and 32/40-bit variable payloads. C++ decoding checks every value/presence/wrapper, not just roundtrip output; external decoding independently checks C++ bytes. No oracle wire output was rewritten or normalized.

The explicit reversed-tag fixture is deliberately **not** claimed as native external cross-qualification: both asn1tools and pycrate encode its counter[2] alternative using source index zero (100100 for value 256), while the normative tag-rank index is two (900100). The persistent independent tag-rank bit model and numeric permutations validate that case. This external-tool limitation does not change generated wire order or justify using variant index as PER index.

Scratch driver/oracle/comparison/result are retained outside the repository. Persistent tests do not require a Python package dependency. Sanitizer PASS does not include instrumentation or qualification of extraction/Parser/Fixer; leak detection was disabled after separate LSan failures.

## Frozen real-source readiness boundary

All six local frozen module blobs match the first-message readiness study hashes. Direct source inspection confirms Cause has six root alternatives and UE-NGAP-IDs three, with choice-Extensions bound open-container payloads. UE-NGAP-ID-pair contains OPTIONAL ProtocolExtensionContainer and a SEQUENCE extension marker. These remain unsupported whole graphs; synthetic fixtures do not replace or prune those real definitions.

Focused real-target probe: untouched six-module Parser/Fixer **PASS**; UEContextReleaseCommand extraction **PASS**, yielding **14 ordinary types and 4 bound instances**, inspected after Parser destruction. UE-NGAP-IDs and Cause retain 3/6 alternatives and valid root mapping, respectively indexes 0..2/0..5; each choice-Extensions reference retains one actual parameter. UE-NGAP-ID-pair retains three fields and is_extensible=1. The complete unfiltered target graph is rejected by the compound types entry with rc=-1, out=NULL and `compound module has invalid storage or unsupported bound instances`. This is a concrete readiness boundary, not a real-message codec acceptance claim; all-entry bound-instance rejection is separately covered by the persistent generator suite.

No benchmark, full-schema qualification or complete UEContextReleaseCommand PDU qualification was performed. Extension ownership, collections and bounded IOC/open types remain subsequent work. Contract, implementation, focused regressions and this evidence are committed/uploaded after PASS under continuous authority; no branch merge or force push is performed.
