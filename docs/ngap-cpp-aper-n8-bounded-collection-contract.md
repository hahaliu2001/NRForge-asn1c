# N8 — Bounded SEQUENCE OF / Container contract

2026-10-09. Base `5e6ef747593828d6eef1376c7b13a1cf3e51e8cf`. Execution authorized by the Owner for P1–P3 without routine approval, within the complete UEContextReleaseCommand target. Independent design review **PASS**, no blockers. Independent review gates implementation and upload.

## P1: domain and owned SIZE evidence

Initial generation domain: non-extensible SEQUENCE OF with one explicit contiguous, finite SIZE interval `0 <= lower <= upper <= 65535`, including exact/fixed and zero sizes. Reject missing SIZE, extensible/union/unbounded/negative/over-domain SIZE, collection extension metadata, IOC/bound instances/parameterized actuals, unsupported element semantics, inline use-site constraints, external/forward/self references. Do not prune a schema or infer bounds from names.

Existing owned IR has scalar size constraints and owned element references; no new variable-length IR is needed. Ordinary SEQUENCE OF extraction currently drops declaration SIZE. Preserve resolved declaration combined constraints (or declaration constraints) through the existing SIZE extractor, allowing exact sizes. Unsupported constraints fail extraction rather than disappear. Unconstrained SEQUENCE OF remains extractable but is rejected by the new renderer. Existing extractor acceptance for valid supported declarations is retained; unrelated extraction paths do not change.

## P2: bounded length and shared element budgets

Add `BitReader::read_bounded_collection_length(size_t lower, size_t upper)` returning `Result<size_t>` and `BitWriter::write_bounded_collection_length(uint64_t count, size_t lower, size_t upper)` returning `Result<void>`, with matching Field views. Validate live/sticky/finished/moved-from state before bounds/count checks. Invalid bounds are `invalid_argument`; out-of-domain count is `constraint_violation` before narrowing.

For `R = upper-lower+1`, encode the offset `count-lower`:

| Cardinality R | Count layout |
|---|---|
| 1 | No determinant or alignment |
| 2–255 | ceil(log2 R) bits, unaligned |
| 256 | Octet-aligned 8 bits |
| 257–65536 | Octet-aligned 16 bits, most-significant octet first |

Count zero follows the same determinant rules; fixed count zero consumes no bits. Elements follow in order without additional collection alignment; element primitives retain their own alignment. No unconstrained determinant, fragmentation, extension bit or length-in-octets interpretation is authorized. Basis: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), 20.2, 20.5–20.6, 11.9.3.3 and 11.5.7.

Append `Limits::max_collection_elements = 65536` after the existing six fields. Both contexts expose read-only `collection_elements()`. Every successful count primitive charges its full count once, including fixed positive counts with zero wire bits, cumulatively across nested collections. This bounds zero-bit element loops/allocation; no fresh contexts per child. Existing input/output/wire and extension budgets remain separate and unchanged. Zero count charges zero.

Length operations stage alignment, payload, element charges, wire/output changes and allocation; publish once only after all checks succeed. Failure leaves this primitive's cursor/output/counters unchanged and records the first sticky error. Reader priority: live state → valid API bounds → whole determinant availability → wire budget → first nonzero alignment bit → spare offset rejection → collection budget. Writer: live state → valid API bounds → count constraint → collection budget → wire/output budget → allocation. Argument/constraint/resource/allocation errors report primitive-start offset, truncation reports logical input limit, padding reports first bad bit. Fixed lengths undergo live/element budget checks despite no determinant. Successful count charges are not rolled back if a later element or complete wrapper fails; complete wrappers never publish partial objects.

## P3: owned collection generation

Add opt-in `asn1typed_render_cpp_owned_collection_types`, `_mapping`, `_codec` sharing the compound planner. This family includes N6/N7 supported graphs and preceding local named collections. Preserve byte-identical output and rejection boundaries of existing N6 and N7 entrypoints. Generate all outputs from the same unmodified IR/namespace; no mixed families. Reserve runtime namespace `nrforge::aper` and descendants, using final shared Naming spelling and preflight collision checks.

Each collection is an identity-preserving `struct T { std::vector<Element> elements{}; };`, including BOOLEAN/vector<bool>. Elements are supported primitive BOOLEAN or preceding local named N6/N7/collection types; unsupported element semantics remain rejected. This domain does not enable ProtocolIE IOC dispatch merely because a container exists. Collections inside SEQUENCE/CHOICE and nested collections share the same context and budgets.

Mapping exposes `value_type`, `element_type`, `element_payload_mapping`, `lower_bound`, `upper_bound`, `length_bits`, `length_align_to_octet`, `extensible=false` and `elements_member`. Count metadata is derived from SIZE cardinality, not storage width. Helpers write/read the bounded count through runtime before traversing elements. Decode checks count versus vector max_size before reserve, reserves once, moves decoded elements/results, and reports bad_alloc/length_error as sticky allocation_failure/resource_limit through the reviewed FieldReader hook. Handles vector<bool> without assuming its proxy is a bool reference. Encode does not narrow size before a checked count call; no helper finalizes child values. Default empty collections with positive lower bounds fail at runtime, not silently padded with elements.

## Validation, review and completion

P1 verifies exact/range/missing/rejected SIZE extraction after Parser deletion. P2 verifies independently derived bytes across cardinality/alignment boundaries, all live/sticky states, spare codes, truncation/padding, exact/one-short shared budgets, fixed zero-bit charging and allocation atomicity. P3 verifies real extraction, deterministic generation, unsupported metadata/name/reference rejection, owned copy/move/input lifetime, nested budgets and allocation failures, full bytes/length/decoded elements, native independent APER cross-checks and prior-family compatibility. Tests remain meaningful under NDEBUG.

Each phase is independently reviewed and committed/uploaded under continuous authority; findings close without another routine Owner handoff. No benchmark, full NGAP qualification, schema modification, known extension-addition dispatch, opaque extension encoding or arbitrary-length/fragmented collections. N8 contains P1, P2 and P3 only; subsequent IOC/open-type dispatch, message body and complete PDU qualification remain distinct milestones.
