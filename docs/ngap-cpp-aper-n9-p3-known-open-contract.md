# N9-P3 — Scoped known open-type runtime contract

2026-10-09. Accepted under continuous execution authority after independent design review PASS.

## API and scope

Add these templated operations to BitReader/BitWriter and matching FieldReader/FieldWriter forwarding views:

```cpp
template<class T, class DecodeFields>
Result<T> read_known_open_type(DecodeFields&& decode_fields);
// callback: Result<T>(FieldReader& child)
template<class EncodeFields>
Result<void> write_known_open_type(EncodeFields&& encode_fields);
// callback: Result<void>(FieldWriter& child)
```

Callbacks execute synchronously, use the supplied child view and do not retain it, move/destroy the enclosing reader/writer/context, or invoke another view of the enclosing stream while a scope is active. Enclosing-stream operations and factory creation against an active context return invalid_state without mutating that context; the designated child remains usable. No separate DecodeContext or EncodeContext is constructed. Values T must be nothrow move constructible so result publication cannot throw after transaction commit. Generated scalar/owned-vector/optional/variant aggregates meet this requirement.

Also add header-only Result<void> FieldWriter::record_failure(Error), symmetric to the reader hook. It validates live state first, replays existing sticky failure, returns finished/moved-from invalid_state without context mutation, and otherwise records the supplied failure. Outside a known scope its offset is physical; inside encode staging any supplied local child offset is anchored to the outermost physically positioned known field start. This permits explicit unknown/selector-wrapper rejection without misusing an unrelated primitive.

This is typed open framing only, with no IOC dispatch, schema policy or generator changes. N7 unknown read_open_type_owned behavior and counters remain unchanged. Known reads never charge unknown retention for their own staging; an explicit unknown read inside their child still charges its payload/record budgets normally.

## Complete child and physical accounting

The child starts at local bit zero over the concatenated payload octets. It validates a complete value locally: final zero padding, one zero octet for an empty encoding, and absence of trailing octets. Child completion closes only that child; it never finishes the shared parent context. The parent continues after the complete framed open field.

Every physical received/emitted bit, including determinants, alignment and fragment terminators, charges shared wire usage exactly once. The outermost physical known-open operation preflights and reserves its whole frame. Child ordinary and framing primitives advance local cursors but do not add physical wire usage or logical parent output usage. Nested opens are already bytes inside that outer frame. All collection, extension-bitmap and explicit unknown-retention charges use the same shared context and limits; they are not suppressed or reset for children. Ordinary primitives outside known scopes retain their existing accounting.

Encode completes the child into owned staging before constructing the enclosing determinant sequence. It uses the existing BASIC APER short/two-octet/maximal-C1..C4 framing, with a final zero determinant for exact fragment multiples. It does not strip child complete padding or substitute an empty raw payload. Decode reuses the N7 availability/padding/canonical framing scan and bounded rescans, rather than accepting a different known-wire grammar or retaining an unbounded fragment descriptor array.

## Transaction and reservations

One known-open call, including its callback, is a transaction. Success publishes the value/frame, outer cursor/output, physical wire use and child semantic charges once. Failure records the first sticky error and restores cursor/output and all semantic/wire usage counters to their values at entry. Previously successful work before this call remains charged. Successful primitives inside a failing known scope are rolled back with that enclosing transaction; this explicit scoped behavior differs from a later unrelated helper failing after an ordinary primitive has committed. Nested success remains provisional until the outermost containing known call succeeds. No partial known value is returned, and ignored callback errors or later exceptions cannot publish a value.

Use internal context snapshots and scoped reservations, not new contexts, public counter setters or reservation/refund APIs. An active context admits only its designated child operations, avoiding unrelated alias work being refunded by rollback. Existing public record_failure may still stop the shared transaction. Reservations are released on every success/failure/exception path; sticky failure remains after rollback.

Append these Limits members after the existing seven, preserving original aggregate positions/defaults:

```cpp
std::size_t max_known_open_staging_octets = 1u << 20;
std::size_t max_known_open_depth = 16;
```

Both contexts expose read-only known_open_staging_octets() and known_open_depth(), reporting currently active reservations, not a lifetime total or peak. Staging charges sum across all active scopes: each decoded concatenated buffer and each live encoded child buffer's logical octet size. Nested child buffers and their parent's staged output count simultaneously while both are held. Physical input spans and the outer physical writer output are excluded. This is a logical-octet quota, like max_output_octets; vector capacity/allocator bookkeeping is not promised to equal the charged logical size. Preflight size arithmetic and vector max_size before allocation/growth, and release scope storage/reservations after publication. No hidden fixed payload ceiling beyond configured limits. Depth zero rejects every known-open call; sequential scopes can reuse released quota.

## Errors and coordinates

Public cursor_bit() on a child view remains **local**. Decoder primitive failures translate local offsets to the original received wire once before recording them as sticky. FieldReader::record_failure receives a local child Error, including existing generated catch paths using cursor_bit(); it performs that same translation. Existing sticky errors are already physical and are replayed without translation. A callback-created failure with no prior sticky error is also local; an offset outside the child bit limit becomes invalid_argument at the known field start rather than inventing a physical position.

Mapping is composable: translate a logical payload position through the current frame's determinant gaps into its enclosing stream, then through that stream's parent frame, until original input coordinates are reached. Bounded rescans suffice; no allocated position per payload bit/octet or unbounded descriptor list is required. At an intermediate fragment boundary, a position denotes the following content bit after the next determinant; at the end of the logical payload it denotes immediately after the last payload content, before a terminal zero determinant if present. Nested malformed framing offsets use the same mapping. A staging/depth/allocation failure before callback decoding reports the known field's physical start. A child thrown bad_alloc/length_error reports its current cursor translated to original wire. Sticky first error dominates later callback errors/exceptions.

Encode failures during staging, including nested child primitive/callback failures, report the **outermost physically positioned known-field start**. Inner staged offsets are not reported as known global positions. A framing/output failure after staging uses that same field-start anchor. Earlier sticky errors outside the scope retain their original offsets.

Decode priority: live/sticky/lifecycle; whole framing availability and checked arithmetic (fully readable invalid fragment selector immediately constraint_violation at its determinant, as N7); physical wire budget; depth/staging reservations; alignment zeros; remaining canonical framing rules; allocation/copy; child callback; local complete validation; publication. Truncation during framing reports the physical input limit; child truncation reports its mapped logical limit. Encode priority: live state; depth reservation; child callback and its existing primitive priorities; local completion/staging; checked framed size; physical wire/output budgets; outer allocation and emission; publication. All reservation/output failures are resource_limit; bad_alloc is allocation_failure; length_error/unrepresentable sizes are resource_limit. Other callback exceptions record invalid_state at the known physical field-start anchor if there is no prior sticky error, then roll back and rethrow; an existing first error is preserved. Catching and ignoring that exception therefore cannot later publish an outer value. Existing complete-wrapper exception behavior outside this new API is unchanged.

## Acceptance checks

Independent literal framing and complete-value models cover every starting residue, empty child substitution, BOOLEAN/INTEGER/collection children, nested known opens, lengths around 127/128/16384/65536, all fragment multipliers and exact-multiple terminators. Child malformed complete padding/trailing/truncation and nested-fragment offsets are compared to original wire coordinates. Tests demonstrate no double wire/output charge, shared collection/extension/unknown quotas, exact/one-less concurrent staging and depth, quota release/reuse, and transaction rollback after a later child failure.

Use active REQUIRE checks under NDEBUG, actual allocation failure injection at decode staging/encode staging/outer output points, ignored callback failures/later exceptions (including caught unexpected exceptions with sticky invalid_state), move/finished/sticky and blocked parent-alias behavior. Preserve existing unknown/ordinary wire vectors and lifecycle. Strict C++20, focused ASan/UBSan, fresh appropriate regressions, distribution and independent implementation review precede commit. No whole-message qualification or benchmark is claimed.
