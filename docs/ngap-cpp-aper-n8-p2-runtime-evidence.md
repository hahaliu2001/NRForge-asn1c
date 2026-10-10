# N8-P2 — Atomic bounded collection length and shared budgets

2026-10-09. Base `4c4880cfaeed74340bcd38dfc7242451cf070929`.

Implementation follows the independently reviewed [N8 contract](ngap-cpp-aper-n8-bounded-collection-contract.md). Both Bit/Field views expose bounded collection length operations for 0<=lower<=upper<=65535. Fixed sizes emit no determinant; other counts encode count-minus-lower with the cardinality-specific APER alignment/width. Spare offsets are rejected before collection-budget reservation.

`Limits::max_collection_elements` is appended with default 65536. Read-only per-context counters accumulate nested collection counts, including positive fixed zero-bit lengths. Count parsing/writing stages wire/output/allocation and element charges, publishing atomically. Failure replays the first error and leaves this primitive's cursor/counters/output unchanged. Earlier successful charges remain committed; complete wrappers do not publish partial results.

## Verification and review

The persistent NDEBUG test uses REQUIRE/abort. Its independent layouts span 14 exact/ranged domains, eight cursor residues and lower/mid/upper values; it compares full complete bytes and counters. Tests exercise input/wire/output/element budget boundaries, truncation, first nonzero alignment padding, spare codes/error priority, UINT64_MAX rejection before narrowing, fixed-zero/fixed-positive charging, nested cumulative charges, actual allocation failure, moved/finished/failed states and ignored-error/exception wrapper behavior. Six-field aggregate Limits initialization is statically checked for compatibility.

Independent source review and independent strict C++20 `-O2 -DNDEBUG` compile/run: **PASS**, no blocking findings; Ready to Commit YES.

| Executed verification | Result |
|---|---|
| Fresh runtime suite | 3/3 PASS |
| Strict C++20 NDEBUG/Werror, conversion/sign-conversion/shadow warnings | Compile/run PASS |
| Fully instrumented runtime and focused collection test ASan/UBSan | PASS, detect_leaks=0 |
| LeakSanitizer actual attempt | Fatal `/proc` task/ptrace restriction; no LSan PASS |
| Runtime distribution | runtime.hpp/runtime.cpp/check_collections.cpp byte-identical |
| Whitespace | PASS |

The earlier P1 focused SIZE test also passed ASan/UBSan with owned core/naming/extraction/test instrumentation and uninstrumented Parser/Fixer/common archives. No renderer, collection ownership or full NGAP qualification is claimed by this runtime commit.
