# N2 Runtime Implementation Evidence

Date: 2026-10-09. Branch: feature/ngap-cpp-aper-first-message.
Base/approved contract: `80998dde4e0dd4cd00c41241dcae872b9f4b0767`.
Authority: ngap-cpp-aper-n2-enumerated-contract.md, N2-01 through N2-07.

## Implementation boundary

Adds EnumeratedIndex and BitReader/BitWriter read/write_enumerated, forwarded
through Field views. Root counts are 1..255; extension indexes preserve the full
uint64 domain, including unknown additions. The index is a wire index, never an
ASN.1 assigned numeric value. Each flag/prefix/alignment/length/payload operation
preflights and publishes atomically, with existing sticky/lifecycle rules.
Zero-bit singleton fields still validate arguments and state.

Production changes are limited to libaper/runtime.hpp and runtime.cpp; persistent
regression tests are in check_runtime.cpp. No Owned IR, generator/mapping,
ENUMERATED schema representation, IOC dispatch or NGAP message code changes.

## Verification

| Check | Result |
| --- | --- |
| libaper make check, final focused suite | 1/1 PASS |
| libasn1typed make check | 9/9 PASS |
| g++ C++20, O2, NDEBUG, Werror, pedantic-errors, conversion/sign-conversion warnings | PASS |
| Entire runtime check under ASan + UBSan, no recovery, leak detection disabled | PASS |
| Actual runtime versus external APER, both directions | 7,327 cases; zero failures |
| Independent implementation review | PASS; no blocking findings |
| git diff --check and new document whitespace | PASS |

External comparison details: asn1tools 0.167.0 corroborates 5,167 root cases,
covering every root index for counts 1,2,3,4,6,7,45,255, both extensibility states
and all cursor residues. Its zero-residue non-extensible singleton is excluded
because it emits empty bytes; the runtime's S3 complete encoding is one zero
octet, tested persistently. pycrate 0.7.11 corroborates 2,160 extension cases:
indexes 0..256, all larger payload-size transitions, and UINT64_MAX at all eight
residues. Its native `_ext_<index>` unknown-value representation supports these
large indexes. Each comparison checks full bytes and length, decodes external
bytes with the actual C++ runtime, and decodes runtime bytes with the external
schema codec. No normalization or patching of external wire output was used.

The approved contract's UINT64_MAX study evidence was model-only; implementation
verification now adds native pycrate double-direction evidence. asn1tools is not
used as the long-extension oracle because of its documented alignment defect.
External comparison drivers are scratch experiments, not persistent suite files.

Persistent tests independently construct expected bits, check all supported
study root counts and root indexes, extension boundaries, all residues, logical
truncation points, each alignment bit, malformed lengths and non-minimal forms,
combined-error priorities, spare roots, exact/one-less budgets, actual allocation
failure, singleton zero limits, sticky errors and finished/moved states. They use
REQUIRE/abort rather than assertions disabled by NDEBUG.

LeakSanitizer was attempted with detect_leaks=1 but could not read /proc/2/task
and terminated with its ptrace limitation. No usable leak result; not a PASS.

## Delivery

Implementation and this evidence remain unstaged and uncommitted at delivery.
Only the approved N2 contract was committed/uploaded. No benchmark, real-message
qualification or subsequent capability implementation was performed.
