# N7-P2 — Bounded extension framing runtime contract

2026-10-09. Base: `11961777d080ea571f2192f6b5a59ad662d10378`. Accepted under continuous execution authority after independent design review **PASS**, with no blocking findings.

## Scope and normative basis

Implement bounded **decode** primitives for a SEQUENCE extension bitmap and owned unknown open-type payloads, plus a sticky encode rejection operation. No generator, IR, sidecar value integration, known-addition dispatch or opaque encoding changes. N7-P1 structural evidence and existing ordinary runtime operations retain their contracts.

Primary source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), clauses 11.1, 11.2, 11.9.3.4–11.9.3.8.4 and 19.1/19.7–19.9. The runtime targets BASIC aligned PER; it does not require removal of trailing absent bitmap positions.

Wire rules: a bitmap of 1–64 bits uses a zero flag and six-bit count-minus-one, then its bits without alignment. Larger counts use a one flag followed by an octet-aligned unconstrained length in bits. Open types use octet-aligned unconstrained lengths in octets. One-octet lengths cover 0–127; two-octet lengths cover 128–16383. Fragment headers C1–C4 specify 1–4 blocks of 16384 units, using the largest permitted multiplier for the remaining contents; another determinant follows each fragment, including zero after an exact multiple. Retained payload concatenates fragment contents without framing. A whole open payload cannot be empty, since complete encoding substitutes one zero octet for an empty inner value. Bitmap width is positive and at least one presence bit must be set when an extension section is present; trailing absent positions remain significant. These requirements are encoding rules, not a claim that opaque inner semantics are qualified.

## Public API

Append these members to `Limits`, preserving the positions/defaults of its original three members and ordinary aggregate initialization:

```cpp
std::size_t max_extension_bitmap_bits = 1024;
std::size_t max_retained_unknown_payload_octets = 1u << 20;
std::size_t max_retained_unknown_records = 1024;

struct SequenceExtensionBitmap {
    std::size_t bit_count;
    std::vector<std::byte> packed_bits;
};
```

The result packs presence bits MSB first, with unused final storage bits zero. The returned vectors own their data; copies deep-copy, moves transfer, and input lifetime is independent. No borrowed input view or fragment descriptors escape.

`BitReader` and `FieldReader` expose:

```cpp
Result<SequenceExtensionBitmap> read_sequence_extension_bitmap();
Result<std::vector<std::byte>> read_open_type_owned();
```

The caller reads the SEQUENCE extension bit and root fields first, then calls the bitmap primitive only for an extension section. It calls the open-type primitive once per present unknown addition, in bitmap order. The bitmap primitive covers the normally-small length and all bitmap fragments, not the extension bit or root fields. The open-type primitive covers initial alignment, every determinant and all payload fragments. It does not decode inner content or apply a fresh complete wrapper.

`BitWriter` and `FieldWriter` expose `Result<void> reject_sequence_extension_data()`. On a healthy writer this records `constraint_violation` at its current cursor with no wire/output advancement. Sticky failure dominates, and finished/moved-from writers return `invalid_state` without changing context. P3 will invoke it whenever the received extension sidecar is non-default; P2 emits no opaque bytes.

## Shared budgets and publication

`DecodeContext` exposes cumulative `extension_bitmap_bits()`, `retained_unknown_payload_octets()` and `retained_unknown_records()`. Successful bitmap reads charge their complete received width, including absent positions; successful open-type reads charge concatenated payload bytes and one retained record. Determinants/alignment do not count as retained payload; they do count toward existing wire bits. Ordinary primitives consume none of the new budgets. Limits apply across all calls sharing the context, including nested SEQUENCE helpers. No fresh context is introduced for opaque payloads. This milestone introduces no nested inner-payload decoder API.

Each primitive stages parsing locally, checks size arithmetic, availability, budgets and legal framing before owned allocation, then publishes its result and cursor/counters together. Failure marks the context sticky but leaves cursor and all usage counters unchanged. Previously successful operations retain their charges. No public reservation API permits callers to refund or reset usage. This internal commit/rollback mechanism satisfies shared reservations without an externally mutable counter interface.

Use bounded multi-pass scanning instead of allocating an unbounded fragment list. Returned storage must pass `max_size` preflight; `bad_alloc` becomes `allocation_failure`, `length_error`/unrepresentable sizes become `resource_limit`. No partial result is returned. Complete wrappers retain existing sticky and final-zero-padding/trailing-data behavior.

## Accepted frames and error priority

Accept legal short, two-octet and fragmented forms for both length units when caller limits permit; default bitmap limits normally exclude fragments. There is no separate implementation ceiling hidden behind configurable limits. Reject overlong two-octet lengths below 128, long normally-small bitmap lengths at or below 64, zero total widths/payloads, invalid fragment multipliers, non-maximal fragmentation and an all-absent extension bitmap. A zero terminal fragment after positive contents remains accepted. Opaque payload bytes are never inspected, normalized or zeroed.

Priority for each primitive:

1. Existing sticky error; otherwise finished/moved-from invalid state.
2. Scan determinant/segment availability with checked arithmetic. Missing metadata/payload/terminal determinant is `truncated_input` at the logical input limit. A fully readable invalid fragment selector has no definable segment length and is immediately `constraint_violation` at that determinant's first bit. Arithmetic overflow is `resource_limit` at primitive start.
3. Once the frame boundary is established, check cumulative wire and relevant retention budgets; overflow/excess is `resource_limit` at primitive start. Thus truncated oversized frames report truncation; readable oversized frames report limits.
4. Validate alignment zeros, reporting the first nonzero padding bit. Then validate remaining framing rules: illegal determinant forms/non-maximal fragments report `constraint_violation` at the offending determinant; invalid whole width/all-absent bitmap reports it at primitive start.
5. Allocate/copy; allocation errors report primitive start. Commit once all checks succeed.

The first error is preserved across later primitives and complete callbacks, including callbacks that ignore it or subsequently throw. Failure and accessor behavior remain those of S3/N1/N2. Length parsing does not call committing primitive methods during staging.

## Acceptance checks

Persistent tests use an independent bit builder and literal determinants, covering all starting residues; bitmap widths around 64/128/16384 and sparse/trailing-zero/all-present bits; payload lengths around 127/128/16384/65536; all fragment multipliers, multi-fragment concatenation and exact-multiple terminators. Explicit invalid frames exercise canonicality, every short prefix, nonzero alignment, late-fragment failure and error priority.

Tests cover exact/one-less input/wire/three retention budgets, cross-call cumulative use, successful earlier charges followed by rollback, source destruction and deep copies, allocation-failure injection at actual allocation points, sticky/lifecycle behavior and encode rejection. New tests remain active under NDEBUG. Strict C++20, focused sanitizers, ordinary runtime/typed regressions and distribution checks precede independent implementation review and commit. No benchmark, external whole-message qualification or full frozen-schema parse is required for these primitives.
