# N9-P4 — IOC generation and bounded native qualification

2026-10-09. Implemented under continuous execution authority after independently reviewed P2/P3/P4 contracts. Final independent implementation/evidence review: **PASS; Ready to Commit YES**. This is bounded IOC entry/container generation; complete message/PDU qualification is a later milestone.

## Delivered implementation

The new owned IOC types/mapping/codec entry points validate the original physical graph and finalized registry/binding evidence, resolve full ordinary and parameterized identities, and topologically plan every supplied declaration. They preserve physical component names, collection order and duplicate identifiers. Expected criticality/presence policy remains separate from received criticality; missing mandatory entries are not fabricated or rejected by codec policy.

Known rows use separate wrappers even when payload types coincide. Known payloads use P3's synchronous Field child callback and shared context, not independent complete calls. Malformed known payloads fail and never downgrade to opaque unknowns. Explicitly extensible unknown rows retain their numeric identifier, received criticality and owned octets; their encoder refuses with sticky error. Closed sets reject unknown identifiers. Selector/wrapper mismatch is rejected before encoding.

The existing compound emitter has a private nullable IOC-mode hook; legacy APIs do not enter lowering. Shared final Naming is reused, all runtime references are globally qualified, and generated symbols are collision-checked before output. Independent review identified a missing macro check for custom selected-value members; it was added and a persistent `errno` refusal protects the fix.

## Persistent and independent evidence

- Final isolated configured Typed IR checks: **19/19 PASS**. Runtime checks: **4/4 PASS**.
- Five physical roots cover known, empty, closed, Extension and empty-Extension sets. Cross-module payloads include BOOLEAN, bounded INTEGER, optional compound fields and constrained collections. Parser trees are deleted before all three output families are generated twice and compared. Actual renderer allocation-failure sweeps exercise every allocation point until success; rejects leave output NULL and concrete diagnostics.
- Structural negatives cover missing references/modules, full actual-key mismatch, residual unsupported metadata, duplicate identities, dependency cycles, stale registry/binding flags, orphan registries, identifier/symbol spelling collisions, NULL parameters and reserved macros.
- Generated C++20 with NDEBUG, active REQUIRE checks and strict conversion/sign warnings compiles and runs. Literal tests compare entire encodings and decoded fields, including reordered duplicates, missing mandatory entries, received-criticality mismatch, unknown input destruction/copy/move, sticky refusal, malformed known frames and shared input/output/wire/collection/depth/staging limits. Actual runtime allocation failures are injected.
- Independent reviewer compiled and ran the generated harness with optimization and strict warnings; no blocking finding remains in the synthetic domain.
- The five prior ordinary generator families (ENUMERATED, INTEGER, compound, SEQUENCE extension, collection), each with types/mapping/codec output, were compared between HEAD's compound source and the new nullable-hook source using identical remaining sources: **15/15 outputs byte-identical**. Full prior regressions also remain passing.
- Fully instrumented runtime/generated harness ASan/UBSan passed with `detect_leaks=0`. A focused C-generator run instrumented owned core/Naming/renderers/extractor and passed all five roots, refusals and allocation sweeps; reused Parser/Fixer/common archives were not instrumented. An explicit generated-harness LSan attempt ended in the environment's fatal ptrace restriction; no LSan PASS is claimed.
- Distribution includes new production headers/source, fixture, C driver, executable shell test and C++ harness. Whitespace checks passed. No benchmark was run.

## Native profile

`tools/n9-ioc-qualification` freezes pinned pycrate 0.7.11 and asn1tools 0.167.0, the independent sender schemas, actual generated-code driver, exact accepted case/oracle/octet-length/SHA-256 profile and reproducible runner. The runner rejects unexpected behavior or any profile change before publishing success; a scratch mutation preserving counts but altering one case ID was rejected without a summary. There is no byte normalization.

| Evidence | Checks | Result |
|---|---:|---|
| pycrate parameterized typed IOC sender to generated decoder | 97 | matched |
| asn1tools explicit raw-framing sender to generated decoder | 97 | matched |
| Generated known encoding compared with both sender byte streams | 49 | matched |
| Owned unknown encoder refusal | 12 | matched |
| Independent strict malformed-known literals | 3 | matched |
| Total | 258 | matched; zero unresolved disagreements |

Native cases preserve received criticalities, order, duplicates and complete payloads. Unknown sender payloads include compound types and OCTET STRING lengths through 65536, with genuine fragmented open frames. Empty Value/Extension registries and closed-set policy are explicit. asn1tools cannot compile the parameterized IOC sender and is not claimed as typed IOC evidence. pycrate accepts some malformed inner padding/trailing octets; three strict rejection expectations are independently derived literals, not claims of oracle agreement. See the tooling README for reproduction and exact limitations.

The bounded profile is not general ASN.1/APER qualification, all-procedure dispatch, or whole NGAP interoperability. N10 body integration and the later complete NGAP-PDU milestone are not performed here.

## Untouched target generation readiness

The frozen six-module UEContextReleaseCommand graph was parsed/fixed and physically extracted without schema editing: 14 ordinary types, 6 bound instances and 4 valid registries. After the Parser/Fixer tree was deleted, all three new IOC output families generated successfully. A translation unit including runtime, types, mapping and codec in that order compiled under strict C++20, NDEBUG, `-Werror`, conversion and sign-conversion warnings, using namespace `readiness::nrforge`.

Generated sizes were types 12,534 bytes, mapping 31,187 bytes and codec 70,523 bytes. These are observed readiness artifacts, not a stable byte-size API. This proves the physical dependency graph can be emitted and compiled; no actual target wire vectors, procedure envelope, complete NGAP-PDU or whole-target interoperability were qualified in N9.

The reusable `tools/asn1typed_ioc_probe` retains this workflow without a target-name gate. For example, after building the tools, reproduce generation using:

```sh
tools/asn1typed_ioc_probe \
  --module-list tools/qualification/ngap-rel18.modules \
  --asn1-root <frozen-ngap-root> --root-module NGAP-PDU-Contents \
  --message UEContextReleaseCommand --namespace readiness::nrforge \
  --output-prefix <existing-scratch-directory>/target
```

It reports owned registry evidence after tree deletion, preflights all three output families before writing and distinguishes CLI/parse/fix/extraction/generation/I/O failures. Output I/O is not promised atomic. Its synthetic CLI regression checks successful files and namespace refusal without files; isolated `make -C tools check` passed **1/1**, and final distribution verification included the probe and all eight native-tool files. Its purpose remains readiness, not wire qualification.

## N9 closeout

All planned N9 phases are complete: P1 architecture (`7d7c157a13d53f2e7f2f37ba1d43b10642ad45fe`), P2 owned evidence (`bc37b3e605aec0a60c3ea824609b45995f3b3c84`), P3 known-open runtime (`89a7193c9c8ee17ed0e409a901de120bff7aa0a5`), and this P4 generator/qualification milestone. No further N9-P5 phase is required by the approved plan. The next milestone is N10: actual UEContextReleaseCommand body integration and independent byte/value qualification, before the distinct complete NGAP-PDU envelope milestone.
