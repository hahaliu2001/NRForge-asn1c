# N8-P3 — Owned bounded collections and N8 closeout

2026-10-09. Base `91a2aabc8c5af7bf737109af9e24f69ec89d794a`.

The [N8 contract](ngap-cpp-aper-n8-bounded-collection-contract.md) passed independent design review. New opt-in collection types/mapping/codec entrypoints extend the compound planner without changing existing N6/N7 generation families. Each collection has a distinct struct identity and owned `elements` vector, SIZE-derived length metadata and element mapping. Earlier local N6/N7/nested collection elements use shared runtime views and cumulative budgets.

Encoding validates count through the sticky runtime primitive before element traversal. Decode reads/charges count atomically, checks max_size, reserves once, moves elements/results and converts allocation/length errors into sticky failures. BOOLEAN vectors do not rely on bool references. Extensible SEQUENCE elements preserve N7 sidecars; retained extensions remain non-encodable. Collection constraints do not enable IOC/open-type dispatch or modify frozen schemas.

## Existing-family compatibility

Separate scratch drivers compiled HEAD and current renderer objects against the same libraries; all fifteen old-entrypoint outputs were byte-identical:

| Family/input | types | mapping | codec |
|---|---:|---:|---:|
| N6/main compound | 2931 B | 9869 B | 24541 B |
| N6/reversed compound | 1305 B | 4399 B | 10781 B |
| N7/main compound | 2931 B | 9869 B | 28160 B |
| N7/reversed compound | 1305 B | 4399 B | 12370 B |
| N7/extension generation | 1704 B | 7341 B | 26718 B |

## Executed verification and independent review

| Executed verification | Result |
|---|---|
| Fresh typed suite (all 17 declared log/trs artifacts explicitly removed before execution) | 17/17 PASS |
| Runtime suite | 3/3 PASS; runtime unchanged since the fresh P2 run |
| Final generated C++/runtime strict C++20, NDEBUG, Werror, shadow/conversion/sign-conversion | Compile/run PASS |
| Final generated code and full runtime ASan/UBSan | PASS, detect_leaks=0 |
| C driver, all owned core/naming/extraction/renderers instrumented | ASan/UBSan PASS; Parser/Fixer/common archives uninstrumented |
| LeakSanitizer actual final attempt | Fatal `/proc` task/ptrace restriction; no LSan PASS |
| Typed/runtime distribution | Thirteen relevant source/header/test/fixture files byte-identical |
| Native-tool distribution | All seven tools byte-identical in a fresh explicit scratch distdir |
| Whitespace | PASS |

The frozen fixture includes BOOLEAN vectors; directly named INTEGER collections; SEQUENCE/CHOICE elements; nested collections; fixed SIZE(0)/(3); cardinality 255/256/257/65536; and extensible SEQUENCE elements/parents. Persistent independent count/bit models compare complete lengths/bytes and decoded elements, including nonzero lower bounds, optional presence and root alignment. A fixed three-element collection of empty SEQUENCEs encodes complete `00`, retains three values and charges three elements despite zero field bits; budget two fails before reserve/traversal with no count/cursor publication.

Real extraction destroys Parser trees before generation. Every output repeats byte-identically. The C driver checks missing/extensible/negative/over-domain/reversed SIZE, unsupported elements/actuals/metadata, external/forward/self/missing references, invalid namespaces, normalized name collisions and NULL arguments, with concrete diagnostics and NULL output. Allocation injection walks generator allocation points until success and verifies failure cleanup/determinism. Generated tests cover spare counts, lower/upper violations, padding/truncation/trailing errors, nested cumulative budgets, committed prior charges on later failure, actual reserve OOM, sticky empty-collection helpers and complete publication rules. Input destruction, deep copies/moves and unknown extension sidecars within collection elements are exercised. Tests use REQUIRE/abort under NDEBUG.

Independent implementation/source and independent strict generated harness review: **PASS**, no blocking findings. Native qualification and final tools/evidence review are recorded below before upload. No full NGAP qualification or benchmark is implied.

## Native qualification

The persistent tools in `tools/n8-collection-qualification/` use actual generated Field helpers, pycrate 0.7.11 and asn1tools 0.167.0. They compare full encoded bytes and complete decoded element values, then perform native reverse decoding. Prefix residues 0–7, scalar/compound/nested elements, finite SIZE thresholds and the maximum count are covered. Patterns can repeat wire vectors; the counts below are executed cases, not distinct vectors.

| Native oracle | Normal cases | Separate declared-extension sender cases |
|---|---:|---:|
| pycrate | 1128/1128 exact bidirectional matches | 48/48 PASS |
| asn1tools | 1122/1128 exact matches; six explicit discrepancies | 48/48 PASS |

The six asn1tools discrepancies are three pattern cases for standalone fixed SIZE(0) and three for standalone fixed three-element collections of zero-bit empty SEQUENCE elements. asn1tools emits zero octets; generated code and pycrate emit the complete-encoding substitution octet `00`. Generated decode rejects the empty oracle input as `truncated_input@0`. This follows X.691 (02/2021) §11.1; no bytes are normalized and these cases are not claimed as matches.

The separate sender schema declares SEQUENCE additions absent from the generated receiver schema. Both oracles' 48 cases verify owned roots, bitmap widths, addition indexes and exact opaque payload octets. Retained sidecars are not re-encoded. The exact accepted case/signature profile rerun passed with unresolved=0, meaning no discrepancy beyond the six recorded oracle cases. It does not mean universal zero-difference qualification. Commands, pinned dependencies and signatures are retained in the tool README and JSON evidence.

Independent native-tool review: **PASS**. Budget, allocation and malformed-input evidence comes from persistent generated/runtime tests, not from success-only native comparisons. Local LSan limitations above remain applicable.

## N8 closeout

| Phase | Completed result | Commit |
|---|---|---|
| P1 | Frozen contract and owned SIZE extraction evidence | `4c4880cfaeed74340bcd38dfc7242451cf070929` |
| P2 | Atomic bounded count primitives and shared element budgets | `91a2aabc8c5af7bf737109af9e24f69ec89d794a` |
| P3 | Owned generated collections, persistent tests and native qualification tools | This change |

P1–P3 are complete within the frozen finite, non-extensible collection domain. There are no defined N8-P4/P5 tasks. IOC/open-type dispatch, complete target message/PDU qualification, fragmented or unbounded collections, and full NGAP qualification remain separate work; none is claimed by this closeout.
