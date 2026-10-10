# NGAP C++ SDK packaging closeout

Status: PASS, independently reviewed; 2026-10-10.

SDK 0.1.0 provides `find_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED)` and `NRForge::ngap`. The complete static library was built, linked, checked and installed with CMake 4.4.4 and g++ 13.3.0. All 131 messages / 81 procedures are registered; all 135 production objects are valid and every registration definition occurs once. A default-build registry executable must link and run successfully before the archive receipt is published.

The public installed headers carry a fingerprint-specific link guard. Configure/build/install verify sealed inputs and the archive receipt. Altered inputs, missing/extra generated files, an expected fingerprint mismatch and mixed header/library identities are rejected. Direct PDU and message header entry points are guarded too. Exported CMake paths are relocatable.

An outside-repository consumer used only a relocated installation. During its actual configure, build and run, the original repository, generated output, schema and installation paths were temporarily unavailable; all were restored afterwards. NG Setup Request (41 octets), Response (43) and Failure (13) matched complete native pycrate 0.7.11 / historical golden vectors and decoded field checks. These minimal mandatory-IE values are schema fixtures, not deployment configuration.

The 656 previously qualified production generated files remain byte-identical. Existing checks passed: libaper 10/10, libngap 1/1, libasn1typed 40/40 after a clean rebuild. Initial stale local PIC objects and an empty generated adapter object were detected and rebuilt; the latter motivated the mandatory complete-registry link gate. The receipt-writing CMake cache-variable issue was fixed and independently checked.

Independent review reconciled installation/archive hashes, all registration symbols, native vectors, consumer dependencies and negative link logs: PASS, no blocking findings. Exact identities, file hashes and consumer evidence are in `tools/ngap-sdk/verification-summary.json`; reproduction commands are in `tools/ngap-sdk/README.md` and `tools/ngap-sdk-consumer/README.md`.

## Limits

Strict-warning acceptance is for the actual O0 C++20 build. An initial GCC O3 attempt produced maybe-uninitialized diagnostics in generated nested variant/optional code; optimized warning-clean compilation and performance are not qualified. This is a source SDK, without a cross-toolchain binary ABI promise.

The local pipeline stages were actually run individually; `build.py` was reviewed and syntax checked rather than replaying the expensive build. CI is supplied but no remote run is counted as PASS. It requires `NRFORGE_SCHEMA_READ_TOKEN` with read-only access to private NRForge-RAN at the pinned schema commit. The historical full-PDU profile is preserved, not rerun by this packaging milestone. No new sanitizer, benchmark or live RAN/CU-CP qualification is claimed.

This milestone ends at SDK delivery; application integration is a separate task.
