# RRC-P5 — dedicated UPER primitive runtime

Baseline: `4fa0b0d6832cf22f412f833e3284ed15783fab08` (accepted P4).
Branch: `feature/f1ap-cpp-aper`. The runtime lives in `libuper`, namespace
`nrforge::uper`. It neither includes nor links APER internally, and exports no
APER alias. Autotools builds a separate internal library and focused checks.
Installed/sealed SDK delivery remains P8. No merge or performance benchmark.

## Normative contract

Authority is [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en),
[official freely available PDF](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items).
This finite primitive profile is not a claim to implement all ASN.1/PER types.

| Clause | Implemented contract |
|---|---|
| 11.1.3 | Concatenate fields at their current bit offset; no octet alignment operation |
| 11.1.3.1 | Complete octet encodings append zero terminal padding; empty encodings substitute one zero octet |
| 11.1.3.2 | ASN.1 BIT STRING containment has no terminal wire padding; empty encodings substitute one zero bit |
| 11.2 | Open payload is a nonempty complete octet encoding, framed by octet length segments |
| 11.5.6 | Constrained whole numbers use the minimum offset bit width; full uint64/int64 ranges avoid overflowing cardinality |
| 11.6 | Normally-small nonnegative indexes: six-bit small form, otherwise minimal unsigned semi-constrained octets |
| 11.9 | One-/two-octet lengths; 16K units with multipliers 1–4; another determinant follows every fragment, including a zero remainder |
| 13 | Bounded signed/unsigned integers and extensible finite roots; extension payloads use minimal signed whole-number form |
| 14, 23 | Separate root and extension indexes; extension indexes are normally-small; CHOICE extension open framing is composed explicitly |
| 16, 17 | BIT/OCTET STRING fixed, bounded, unconstrained and extensible size; upper bounds at least 65536 use segmented lengths |
| 19 | Bounded collection count or individual length segment; compound code owns element encoding and fragment continuation |
| 18 | Extension bitmap length: small form encodes count minus one; long form encodes actual count as a general length |

`Completion::exact` is an internal field-list boundary, not an ASN.1 complete
encoding. Reader logical bit limits distinguish exact/contained bit encodings
from storage capacity. All unused storage bits must be zero. Octet finish rejects
extra full octets and nonzero terminal padding. `CompleteEncoding` separately
records semantic bits, encoded bits, terminal padding and empty substitution.
Padding is never counted as a semantic field.

The machine profile supports signed int64 unconstrained integers, signed/unsigned
finite contiguous root intervals and full uint64 extension/unknown indexes.
Oversized integer payloads fail with a resource error; no arbitrary precision is
implied. Decoders reject unused range codes, redundant integer/sign octets,
nonminimal length and normally-small forms, invalid fragment multipliers,
extension selectors that encode an in-root value, zero-length open payloads and
invalid enum/size/completion arguments. BIT STRING storage is MSB first, with
exact ceil(bit_count/8) bytes and zero low tail bits. NamedBitList normalization,
character strings, REAL, OBJECT IDENTIFIER, semi-constrained INTEGER and disjoint
integer sets are outside this initial profile. Source constraints requiring
these shapes must stay blocked until separately supported.

## Ownership, limits and transactionality

Reader borrows input for its lifetime and returns owned values. Writer owns
staging until successful completion. Copies are disabled; moving a reader
invalidates its source. Writer staging aliases passed to OCTET STRING/open
writes are rejected before any mutation or allocation. Ordinary source spans
must remain valid for the call. Sticky first errors prevent later operations
from publishing a completed value; reuse after completion is invalid.

Every public field operation is atomic: failure restores the cursor, cumulative
quotas and staging prefix. Allocation/length exceptions become typed errors.
A transaction is one primitive, not an entire future generated compound.
Nested compound transactions, shared child/open-type depth budgets, callbacks,
SEQUENCE optional/DEFAULT decisions, group dispatch and recursive rendering
belong to P6. Raw open payload validation checks framing and nonempty content,
not inner semantics; known dispatch must validate the inner complete encoding.

Default caps are 1 MiB input/output/owned value/retained unknown bytes, 8 Mi bits
wire work, 65536 collection elements, 1024 bitmap bits and 1024 unknown records.
Owned value, collection, bitmap and retained-unknown quotas are cumulative for
one stream. All retained-unknown records and each fragment's value/unknown
bytes are charged before payload allocation, then rolled back on failure.
Bitmap length is explicitly capped at 16383 bits by this profile; the default
quota is lower. Discarded open values allocate nothing and charge no owned-value
or retained-sidecar quota. Their per-call scan length is bounded by
`max_value_octets`; cumulative input/wire caps bound repeated scans. Length
segments themselves charge collection element count; a future compound caller
must supply the payload and subsequent segment before completing that value.

## Evidence and retained boundaries

`tools/uper-qualification/qualification.json` records the finite independent
oracle matrix and source hashes; `verification-rrc-p5.json` records build,
negative/sanitizer/regression checks. Reproduction is in that directory's README.
Both native oracle versions are pinned, with upstream unsupported/defective
shapes explicitly assigned to the other oracle. Synthetic compound wrappers
exercise these primitives; they are not generated RRC wire qualification.

Focused checks cover offsets 0–7, 39/64-bit ranges, machine extremes,
127/128 and 16383/16384/65536 boundaries, exact-multiple continuation, enum
extension indexes, bitmap 64/65 boundaries, complete/contained/exact finish,
malformed determinants/integers, all bit truncations of a prefixed 129-octet
value, input/output/wire/value/element/bitmap/unknown limits, sticky errors,
allocation failures and staging alias rejection. ASan/UBSan are run with leak
detection disabled because this execution environment cannot inspect the
required process task directories; no leak-check PASS is claimed.

Existing APER source files, APIs and generated artifacts remain unchanged.
APER/UPER coexistence compiles both headers and links both runtimes; a field
starting at bit 3 ends at bit 19 under UPER versus bit 24 under APER. Existing
APER, protocol PDU and typed checks are rerun. P4's frozen six-module sources,
80-root readiness matrix and four bare fixtures are not regenerated or claimed
wire qualified here; their earlier evidence remains unchanged. P6/P7 still own
compound generation, the remaining semantic gaps and complete RRC messages.
