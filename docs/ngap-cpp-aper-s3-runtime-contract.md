# S3 — Minimal C++20 APER Runtime Contract

## Status and authority

This is the Owner-approved S3 API and contract for the synthetic slice,
approved on 2026-10-08. It is design documentation only; it does not implement
or qualify a codec. The baseline inspected for this revision is branch
`feature/ngap-cpp-aper-s1-types`, HEAD
`fe30dbdb47aaeca7cc28781ffefdc4b092fffc9f`.
The repository records accepted D1/D2 decisions in
`ngap-cpp-aper-approved-decisions.md`: shared C++20 runtime boundary, structured
result based codec API, and separate generated type and codec work. S2's
approved mapping study establishes basic aligned PER, the 16-bit aligned Count
payload, and zero alignment/final padding as the encoding form. S2 explicitly
leaves decoder padding validation and trailing-data behavior undecided.
****Authority gap:**** the approved-decisions record identifies a D3 Runtime
Interface Contract Study and a separate Runtime Reuse Study, but neither full
study/report nor its accepted detailed contract is present in the inspected
repository. D1-R4 records only that reuse/wrapping/replacement of the legacy C
runtime needs a separate study. No D3 details are reconstructed here. The
S3 decisions below freeze a small independent C++20 API and its limits,
exception policy, and decoder behavior. The D3 and Runtime Reuse source gap
remains; approval of this contract does not reconstruct those missing studies.

## Scope

The runtime primitives are protocol- and synthetic-type-neutral:

- MSB-first bit reader and writer;
- one-bit read/write used by BOOLEAN and a one-bit CHOICE selector;
- octet alignment: writer inserts zero bits; reader checks required bits are
  zero;
- octet-aligned unsigned 16-bit big-endian offset for values 0 through 65535;
- outermost complete-value finish/finalize, including final zero padding.
There is no Selection or Packet codec, generic constrained INTEGER, length
determinant, open type, extension, IOC, or arbitrary-precision integer here.
S4 integrates these primitives with the existing generated mapping traits.
Nested field operations never finalize or append complete-value padding.

## Placement and ownership proposal

Use `nrforge::aper` in `libaper/runtime.hpp`, with implementation in
`libaper/runtime.cpp` and build registration in `libaper/Makefile.am` (or the
repository's corresponding library build files). The runtime is C++20 and has
no dependency on C descriptors, C runtime structures, or C type layout.
This location is a proposal, not an existing repository convention verified
by S1/S2.
Reader input is `std::span<const std::byte>` and is borrowed: the caller keeps
the referenced bytes alive and unchanged for the reader's lifetime. The
reader owns its cursor and holds a reference to the per-call DecodeContext;
it never owns the input. The writer holds a reference to EncodeContext. Each
context must outlive its reader/writer. Cursor-bearing readers and writers are
non-copyable; moving transfers their state and leaves the source unusable.
Writer owns its `std::vector<std::byte>` output. A successful finish moves the
complete vector to the caller. No span into a writer's growable buffer escapes.
One `DecodeContext` or `EncodeContext` is created at each public complete-value
call. Nested field helpers receive the same context by reference, so wire-bit
and octet accounting is shared across all descendants. The cursor is owned by
the reader/writer and is not separately copied into nested codecs. Contexts
are not shared between calls or threads.

## Declaration sketch

The following is a reviewable shape, not a promise of final ABI. Omitted
includes are `<cstddef>`, `<cstdint>`, `<limits>`, `<new>`, `<span>`,
`<utility>`, and `<vector>`.

```cpp
namespace nrforge::aper {
enum class ErrorCode {
    invalid_argument,
    constraint_violation,
    truncated_input,
    nonzero_padding,
    trailing_data,
    resource_limit,
    allocation_failure,
    invalid_state,
};
struct Error {
    ErrorCode code;
    std::size_t bit_offset; // absolute from first input/output bit
};
template<class T> class [[nodiscard]] Result {
public:
    static Result success(T value);
    static Result failure(Error error);
    bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    T& value() &; const T& value() const &; T&& value() &&;
    const Error& error() const &;
};
template<> class [[nodiscard]] Result<void> {
public:
    static Result success() noexcept;
    static Result failure(Error error) noexcept;
    bool has_value() const noexcept;
    explicit operator bool() const noexcept;
    const Error& error() const &;
};
struct Limits {
    std::size_t max_input_octets  = 1u << 20;
    std::size_t max_output_octets = 1u << 20;
    std::size_t max_wire_bits     = 8u << 20;
};
struct DecodeContext { /* Limits and per-call consumed wire-bit count */ };
struct EncodeContext { /* Limits and per-call emitted wire-bit count */ };
struct CompleteEncoding {
    std::vector<std::byte> octets;
    std::size_t last_field_end_bit;
    std::uint8_t final_padding_bits;
    bool empty_encoding_substitution;
    std::size_t complete_encoding_bits;
    std::size_t octet_count;
};
class BitReader {
public:
    static Result<BitReader> make(std::span<const std::byte> input,
                                  DecodeContext&);
    Result<bool> read_bit();
    Result<void> align_to_octet_zero();
    Result<std::uint16_t> read_aligned_u16_be();
    std::size_t cursor_bit() const noexcept;
};
class BitWriter {
public:
    explicit BitWriter(EncodeContext&);
    Result<void> write_bit(bool);
    Result<void> align_to_octet_zero();
    Result<void> write_aligned_u16_be(std::uint64_t value);
    Result<CompleteEncoding> finish();
    std::size_t cursor_bit() const noexcept;
};
template<class T, class DecodeFields>
Result<T> decode_complete(std::span<const std::byte> input,
                          const Limits&, DecodeFields&&);
template<class T, class EncodeFields>
Result<CompleteEncoding> encode_complete(const T&, const Limits&,
                                          EncodeFields&&);
}
```

`Result<T>` is a discriminated union/variant
that contains exactly one of T or Error; `Result<void>` contains success state
or Error. Accessing the wrong arm is a programming error (`invalid_state` at
API boundaries, or a documented assertion for local accessors). It does not
inherit from or emulate C++23 `std::expected`.
`decode_complete` creates the context and reader, invokes a field callback,
then validates the remaining complete-value padding and exact input
consumption. It returns T only after all checks succeed. `encode_complete`
creates the context and writer, invokes a field callback, calls `finish()`
once, and returns the owned complete encoding only on success. These wrappers
express the outermost boundary; field callbacks cannot call finish through
their reader/writer contract. Callbacks receive internal FieldReader/FieldWriter views that expose only
field primitives and cannot finalize. The wrappers check both the callback
Result and the context first-error state before publishing success; ignoring
a primitive failure cannot produce a successful complete result.
`read_aligned_u16_be` and `write_aligned_u16_be` align first, then read/write
the 16 bits most-significant bit first. Alignment plus the payload is one
atomic operation: preflight all alignment/payload availability, padding,
arithmetic, output extent and budgets before committing any cursor or counter. The writer accepts a wider integer so
the full S1 uint64_t storage domain is representable at the call boundary.
Values above 65535 are checked before any narrowing and rejected as
`constraint_violation` before changing the cursor or output. The reader returns
`uint16_t`, covering the entire permitted offset domain.

## Units, boundaries, and arithmetic

All cursor positions and `Error::bit_offset` values are absolute bit offsets
from the beginning of the complete input/output octet sequence, with the
first bit at offset zero. Octet counts and span lengths are in octets. No
public member is named the ambiguous `bit_length`.

| Measurement | Meaning |
| --- | --- |
| `last_field_end_bit` | Cursor immediately after the last semantic field bit, including any intermediate alignment before fields, but excluding outermost final padding. |
| `final_padding_bits` | Number of zero bits appended by outermost finish, 0 through 7. |
| `empty_encoding_substitution` | True only when a zero-bit field encoding is replaced by the required single zero octet. |
| `complete_encoding_bits` | 8 when empty_encoding_substitution is true; otherwise last_field_end_bit + final_padding_bits. Always a multiple of 8. |
| `octet_count` | Number of owned output octets; equals `complete_encoding_bits / 8`. |
| reader `cursor_bit()` | Next input bit to consume; includes consumed intermediate alignment, excludes unconsumed final padding. |
| context `wire_bits` | Bits processed by primitive operations, including intermediate and final padding bits that are checked or emitted. It is a work/accounting metric, not the semantic endpoint. |

Before `octets * 8`, require `octets <= SIZE_MAX / 8`. Before addition,
require `a <= SIZE_MAX - b`. To round up a bit count, compute remainder and
checked-add `0..7`; do not calculate `bits + 7` unchecked. Derive octet count
as quotient plus a checked remainder flag rather than overflow-prone
`(bits + 7) / 8`. Any unrepresentable size is `resource_limit` at the
operation's starting cursor. Reject impossible input span sizes before
computing a bit limit.
The public complete decoder always receives a complete octet span and uses
its full checked bit length. An internal logical-bit limit in [0, input_bits]
may be used only for focused primitive tests; it does not change complete
input length or its budget charge. Bounded subreaders, prefix decoding and
streaming decoding are deferred and are not public S3 APIs.

## Padding and complete-value policy

S2 records X.691 11.1.4's zero fill before octet-aligned fields and at the end
of an outermost complete aligned encoding. The standard encoding form is
zero padding. The proposed project decoder policy is to reject any nonzero
required intermediate alignment bit or final padding bit as
`nonzero_padding`.
The proposed complete decode policy is exactly one complete value per input
span. After a field decoder succeeds, inspect the remaining bits needed to
complete that final octet as zero, then require no further octets. Extra full
octets produce `trailing_data`. For an encoding whose final field already ends
on an octet boundary, no final padding is inspected; any additional octet is
trailing data. There is no permissive, prefix, or streaming mode in this
slice. The rejection policy is a project choice; S2 did not approve it.
Zero-bit field encodings: X.691 (02/2021), 11.1.4 requires an empty
outermost aligned field encoding to be replaced by one all-zero octet. A
healthy zero-bit writer therefore finishes as 00, not an empty vector. Its
last_field_end_bit and final_padding_bits are both 0,
empty_encoding_substitution is true, complete_encoding_bits is 8, and
octet_count is 1. The substitution costs 8 wire bits and 1 output octet.

When a field callback consumes zero bits, complete decode requires exactly
one zero octet: empty input is truncated_input at bit 0, a nonzero octet is
nonzero_padding at its first nonzero bit, and further octets are trailing_data
at bit 8. Checking the substitution costs 8 wire bits. This complete-value
rule adds no NULL codec or synthetic type. Nonempty encodings use ordinary
0..7 final padding bits and set empty_encoding_substitution to false.

Reference: [ITU-T X.691 (02/2021), clause 11.1.4](https://www.itu.int/rec/T-REC-X.691-202102-I/en).

## Error contract

Errors are selected by stable `ErrorCode`; callers must not parse diagnostic
strings. The offset is an absolute bit offset in the input/output sequence.
The proposed convention is operation-specific and deterministic:

| Code | Applies at | Reported offset | Result and state |
| --- | --- | --- | --- |
| `invalid_argument` | Null/invalid API state or logical limit outside `[0, input_bits]`; invalid public limits if rejected at construction | Operation start, normally 0 | No value; context is failed. No dereference or arithmetic using invalid input. |
| `constraint_violation` | Writer's 16-bit offset receives value above 65535 | Start of that field operation, before alignment | No value; primitive is atomic, cursor/output unchanged, context becomes failed. |
| `truncated_input` | A read needs bits beyond logical limit or supplied complete input | First missing bit, equal to available logical bit count | No value; primitive is atomic, cursor unchanged, context becomes failed. |
| `nonzero_padding` | Required intermediate or final zero bit is one | First nonzero padding bit | No value; cursor/output not published; context becomes failed. |
| `trailing_data` | Complete decode finds bytes after final padding / semantic value | First trailing bit, at the first extra octet boundary after final padding | No value/object; reader context becomes failed. |
| `resource_limit` | Octet or wire-bit budget would be exceeded; checked size arithmetic cannot be represented | Operation start, before mutation/debit | No value; primitive is atomic, context becomes failed. |
| `allocation_failure` | Writer buffer growth or owned result construction fails | Operation start for the growth/finalize operation | No complete encoding; context becomes failed. Catch `std::bad_alloc` at public Result boundaries. |
| `invalid_state` | Use after successful finish, moved-from use, or repeated finish | Current cursor | No value; no further mutation. |
| Stored first error | Any operation after failure | Original first-error offset | Replay the original code and offset; no further mutation or debit. |

Malformed semantic values are reported at the start of the field operation;
truncated reads report the missing-data start; padding reports the first bad
bit. If a compound callback propagates a primitive failure, it preserves that
original code and offset. `Error` intentionally omits an allocation-prone
message. An optional diagnostic string may be added outside the minimum
contract, but it cannot replace the code.
Public Result boundaries catch std::bad_alloc and return allocation_failure,
storing the first failure at the operation start. Writer preflight checks
vector::max_size() and representability before growth; exceeding either is
resource_limit, so std::length_error is not a normal resource-error path.
Internal field callbacks must not throw exceptions other than allocation
failure; throwing another exception is a programming-contract violation,
not a wire error. The minimum API avoids strings and std::function allocation.

## Primitive atomicity and failed state

Recommend ****A plus sticky failure (limited B)****: every primitive preflights
argument, available input/output size, arithmetic, and budget before changing
cursor/buffer/accounting; therefore the primitive is atomic. On any returned
error, the context then records the first failure and becomes sticky-failed.
This prevents a caller from ignoring an error and continuing a compound
decode/encode on inconsistent semantic state.
After failure, status/error queries remain available. Read, write, align,
and finish replay the stored first Error unchanged; they never mutate further.
Primitive failure does not debit budget. Writer growth must provide the strong
exception guarantee, leaving cursor, logical output and counters unchanged.

finish() is legal exactly once on a healthy writer. It atomically checks
output/wire budgets and capacity limits, adds ordinary final zero padding or
the empty-field all-zero-octet substitution, constructs the owned result, marks
the writer finished, and returns it. It cannot expose partial output. Calling
it after successful finish is invalid_state. Calling it after failure replays
the first Error. Reader final validation belongs only to decode_complete.

S4 must build decoded values in a local temporary owned object and return it
only after all nested decoding, padding checks, and trailing-data checks
succeed. On any error, destroy the temporary and return only `Error`; no
partially decoded object is observable.

## Per-call budget

For this fixed primitive slice, retain three distinct limits:

| Limit | Unit and charge | Proposed default | Zero limit | Failure rule |
| --- | --- | --- | --- | --- |
| `max_input_octets` | Entire public input span length, including final padding and any trailing bytes | 1 MiB | Accept only an empty input span; any non-empty span fails before decode | `resource_limit` at bit 0, before field decode. |
| `max_output_octets` | Logical output octet length, including alignment, final padding and empty substitution | 1 MiB | No complete encoding can succeed (minimum one octet) | Preflight each operation's required extent and final rounded size; fail before mutation. |
| `max_wire_bits` | Bits inspected/emitted by primitives, including alignment and final padding | 8 Mi bits | Permit only operations that process zero bits | Before each primitive, checked-add its full bit cost and fail atomically if over budget. |

max_output_octets bounds logical output length, not allocated capacity or
allocator memory consumption. It is not a physical memory-allocation cap.

These limits are intentionally distinct: input span size, owned output size,
and parser/encoder bit work are not interchangeable. Buffer capacity is not a
budget and must never be used to infer one. Defaults are proposed policy
values, not repository-approved limits; public calls may provide smaller or
larger explicit values subject to representable sizes.
Charge rules: read/write payload bits and checked/emitted padding all count
toward `max_wire_bits`; outermost final padding and the empty-field substitution also count. The input-octet
charge is made once from the complete supplied span before decode; it includes
padding and trailing bytes. Output octets are charged against the eventual
rounded complete size; intermediate alignment contributes to that size. A
failed primitive does not debit any counter. A successful primitive debits
once, after all checks and mutation can succeed. Nested operations share the
same context and cannot reset or replenish it.
Check order: validate arguments and arithmetic; validate cursor/input bounds;
check semantic constraint; compute required output extent; check applicable
octet and wire-bit budgets; perform operation; commit cursor and counters.
For malformed padding, check availability and wire-bit budget before reading
the candidate padding bits; on a nonzero bit, report its first position and
fail without committing cursor/counters. The full public input-octet limit is
checked before creating the reader.
There is no recursion, variable-length container, or unknown payload in S3,
so no depth, element-count, or unknown-payload budget is introduced. If later
work adds them, those limits must live in and be charged through this same
per-call context shared by nested codecs.

## S3 implementation boundary

An implementation task under this approved contract should add only this runtime
surface, its minimal build registration, and focused primitive tests in that
separately authorized task. It should not alter S1 renderer/types, S2 mapping
traits, ASN.1 IR/extraction, C APER runtime, or NGAP dispatch. Mapping-trait
integration and typed `Selection`/`Packet` codecs belong to S4. This document
round itself makes no code or test changes.

## Verification plan for implementation

The later implementation must independently verify:

- MSB-first bit order, including reads/writes crossing octet boundaries;
- BOOLEAN and one-bit selector operations;
- aligned 16-bit big-endian values 0, 1, 255, 256, and 65535;
- Encode rejection of 65536, 2^32 and UINT64_MAX before narrowing, with
  unchanged cursor/output/accounting;
- alignment from cursor residues 0 through 7;
- rejection of nonzero intermediate and final padding;
- truncation at each bit position of one-bit and aligned 16-bit operations,
  including missing alignment and payload bits;
- Exact complete-value acceptance and trailing-byte rejection;
- Zero-bit field encoding produces 00 with the special metrics and costs;
  zero-field decode accepts 00, rejects empty/nonzero/trailing inputs;
- Aligned-u16 failures after otherwise available alignment remain atomic;
- each budget exactly sufficient and one unit too small;
- Repeated finish is invalid_state; every operation after failure replays
  the first code/offset; callbacks that ignore errors cannot publish success;
- Context lifetime/reference use, non-copyable cursors and field views without
  finalize; allocation failure and max_size preflight;
- checked multiplication, addition, and round-up overflow boundaries.
Use hand-derived vectors and an independent implementation or independently
reviewed expected bytes as evidence; encoder/decoder roundtrip alone is not
sufficient. This S3 study does not write or run those tests, build a codec, or
perform codec qualification.

## Owner-approved decisions

Owner approval recorded on **2026-10-08**.

| ID | Accepted contract | Status |
| --- | --- | --- |
| S3-01 | Adopt a type-neutral `nrforge::aper` C++20 runtime in `libaper/runtime.hpp` and `.cpp`, independent of C runtime layouts. | Owner Approved / Accepted |
| S3-02 | Use borrowed `span<const byte>` input, owned vector output, reader/writer-owned cursors, and one explicit per-call context referenced by non-copyable cursor objects and shared nested field views; contexts outlive cursor objects. | Owner Approved / Accepted |
| S3-03 | Use the discriminated `Result<T>` / `Result<void>` shape and stable `ErrorCode` values listed here; bit offsets are absolute from input/output bit zero with operation-specific positions in the error table. | Owner Approved / Accepted |
| S3-04 | Keep field primitives separate from outermost complete APIs; nested operations never finalize. Complete decode requires one full octet span and rejects trailing octets. | Owner Approved / Accepted |
| S3-05 | Check intermediate alignment and final padding for zero. Replace zero-bit outermost field encodings with one zero octet; check the same rule on decode without adding a NULL codec. | Owner Approved / Accepted |
| S3-06 | Use named length metrics for semantic field end, final padding, complete bits, octet count and an explicit empty-substitution flag; check every size conversion/addition/round-up for overflow. | Owner Approved / Accepted |
| S3-07 | Make each primitive atomic and transition the context to sticky failed on its first error; replay the first code/offset on every operation after failure, wrappers check sticky state, finish only once, no partial output or decoded object publication. | Owner Approved / Accepted |
| S3-08 | Use three per-call limits: input octets, output octets, and processed wire bits; proposed defaults 1 MiB, 1 MiB, and 8 Mi bits. Padding and empty substitution are charged; output limits bound logical length, not capacity; failed operations do not debit; nested codecs share counters. | Owner Approved / Accepted |
| S3-09 | Catch allocation failures at public Result boundaries and return `allocation_failure`; preflight max_size as resource_limit; internal callbacks may not throw non-allocation exceptions. | Owner Approved / Accepted |
| S3-10 | Defer mapping-trait integration and typed codec implementation to S4; no generic constrained INTEGER or broader APER framework in S3. | Owner Approved / Accepted |



This revision records the Owner approval and changes documentation only; no
implementation or codec qualification is claimed.
