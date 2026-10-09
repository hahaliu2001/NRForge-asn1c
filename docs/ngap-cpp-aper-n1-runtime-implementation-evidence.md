# N1 Runtime Implementation Evidence

Date: 2026-10-09. Branch: feature/ngap-cpp-aper-first-message.
Base: 476bbb8fe8629fc15e36860c4e3bee5dcdf26b3f.
Contract: ngap-cpp-aper-n1-constrained-integer-contract.md; N1-01–N1-05 approved.

## Scope

Adds read_constrained_uint and write_constrained_uint, forwarded by Field views,
for zero-based, non-extensible full 8/16/32/40-bit domains only. The 16-bit path
delegates to the existing aligned-u16 primitive. Larger domains use an unaligned
payload-octet-count prefix, zero alignment and minimal unsigned big-endian payload.
Each primitive publishes cursor and budget changes atomically. Live/sticky state
and width checks precede range checks or shifts. Invalid 40-bit selectors are
rejected before payload availability and budget checks.

Production changes: libaper/runtime.hpp and runtime.cpp. Persistent regression
tests: libaper/check_runtime.cpp. No generator, mapping or NGAP codec changes.

## Verification

| Check | Result |
|---|---|
| libaper make check | 1/1 PASS |
| libasn1typed make check | 9/9 PASS |
| g++ C++20, O2, NDEBUG, Werror, pedantic-errors, conversion/sign-conversion warnings | PASS |
| ASan + UBSan, no sanitizer recovery, leak detection disabled | PASS |
| Independent agent source/contract review and focused UBSan execution | PASS; no blocking findings |
| Actual runtime versus asn1tools 0.167.0 APER, both directions | 66,272 cases; zero failures |
| git diff --check | PASS |

External comparison exhausts U8/U16 at residue zero, samples 128 values each for
U32/U40 with seed 691, and checks boundaries at residues 1–7. It compares complete
bytes and lengths, decodes external bytes with the actual C++ runtime and runtime
bytes with the external decoder. These external drivers were scratch experiments,
not additions to the persistent repository suite.

Persistent C++ tests independently construct expected bits and exercise every
residue, truncation point and alignment bit; selector/minimality rejection;
exact/insufficient budgets; sticky errors; finished/moved states; and actual
allocation failure.

LeakSanitizer was attempted with detect_leaks=1 but could not read /proc/2/task
and reported its ptrace limitation. No usable leak conclusion: this is not PASS.

## Delivery boundary

Independent implementation review: PASS. Implementation and evidence are
unstaged and uncommitted at delivery; no implementation push. This is focused
N1 verification, not qualification of a real NGAP message or complete NGAP-PDU.
No benchmark or full schema qualification was run.
