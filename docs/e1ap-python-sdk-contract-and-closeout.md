# E1-P6 — Python SDK delivery contract

Baseline: `4d7e80b9e13c73867463d428632e6eed7fe0da25` (accepted E1-P5).
Distribution `nrforge-e1ap` 0.1.0, import `nrforge_e1ap`, backs all 72 messages
and 40 procedures with the installed sealed C++ E1AP SDK. The initial wheel
qualification target is CPython 3.12 Linux x86-64.

The API exports `identity`, `messages`, `schema`, `encode`, `decode`,
`CodecError`. Dictionary/list conversion uses exact built-in shapes, explicit
variant wrapper labels, bytes OCTET STRING, explicit BIT STRING bit counts,
and optional omission/None. Required missing keys, unknown keys, unsigned
negatives, bool-as-integer and overflow fail before narrowing. Unsigned
64-bit usage counters retain the entire uint64 range. Optional false remains
distinct from omission/None. Codec and conversion budgets are independent;
the GIL is released only around owned C++ encode/decode.

The outer E1AP CHOICE has three extensible roots. Unknown outer extensions,
opaque procedures, IE payloads and sequence additions preserve owned received
bytes under resource limits. Retention does not authorize unknown-value
re-encoding. Python `CodecError` preserves the runtime first code/bit offset.
Each module has its own exception identity and statically linked sealed SDK.
Hidden extension visibility and archive-local linker symbols prevent SDK
interposition across NGAP, F1AP and E1AP.

Shared NGAP binding generation/conversion/native sources use a closed explicit
protocol profile. NGAP remains the default distribution; CMake refuses profile
mismatch. E1AP recognizes exactly 150 installed public headers, verifies their
canonical installed include rewrite, original hashes, version and SDK
fingerprint, then generates all 72 message conversions. Unsupported public
shapes fail closed. pybind11 3.0.1 and scikit-build-core 0.11.6 remain pinned.

The Python tests inherit 1,341 hash-checked finite E1-P4 full-PDU vectors for
all 72 messages and 40 procedures, including counter boundary values. Six
CU-UP/CU-CP E1Setup request/response/failure fixtures are inherited from E1-P5.
These bytes establish Python integration with the accepted codec; they are
not a new independent wire qualification or exhaustive value claim.

Build and external consumer commands are in `tools/e1ap-python/README.md`.
`prepare.py` creates a self-contained staged source distribution with a
SHA-256 inventory receipt. `run_installed.py` copies tests outside the checkout,
hides the checkout and explicitly supplied schema/SDK/build inputs, runs
`python -I`, audits installed module paths/oracle absence, and restores all
hidden inputs in `finally`. E1AP tests cover registry/schema, finite bytes,
real uint64 fields, six Setup outcomes, independently constructed failure,
shape/range checks, unknown receive-only cases, optional omission, errors,
resource/conversion budgets, concurrency and all six three-protocol import
orders. Existing NGAP and F1AP installed tests remain regression gates.

No application mandatory-IE policy, stable ABI, other-platform,
free-threaded/subinterpreter, live RAN/vendor interoperability, exhaustive
all-value codec coverage or performance benchmark is included.

## Qualification and delivery closeout

The final wheel built with GCC 13.3.0, two compile jobs and lld 18.1.3 with
`--threads=1`, under the shared strict C++20 warning/error options. Post-link
import saw all 72 messages and the installation-time public seal check passed.
The E1-P5 C++ archive SHA-256 is unchanged. An earlier extracted-sdist attempt
compiled all objects but failed because GCC rejected `-fuse-ld=lld-18`; only the
successful persistent staged-source wheel build is accepted evidence.

A clean virtual environment installed the E1AP wheel without dependencies,
alongside the previously delivered F1-P6 F1AP wheel and original NGAP Python
wheel. These historical artifacts are hash-checked against their own repository
receipts; neither historical wheel was rebuilt during E1-P6. The current shared
binding generator separately reproduced 135 NGAP and 162 F1AP generated files
byte-for-byte against the E1-P5 baseline on sealed public inputs.

The external isolated run passed **10 E1AP**, **9 F1AP**, and **12 NGAP** methods.
E1AP includes all 1,341 inherited finite vectors, six Setup outcomes, all six
UL/DL uint64 paths (0, 1, INT64_MAX, 2^63, UINT64_MAX and bool/negative/overflow
rejection), 18 inherited outer-extension cases and all six module import orders.
Checkout, frozen schema, SDK prefix, C++ build work, both source staging trees
and generated binding work were renamed away during this run and restored.
Installed module paths and absence of pycrate are recorded. All installed
package files match their wheel ZIP member hashes; E1AP ELF has no RPATH/RUNPATH
or SDK shared-library dependency.

E1AP and both historical protocol seal suites passed two methods each. The
wrong distribution/profile configuration failed before message compilation.
The final source distribution's 13-file staging inventory and hashes passed;
a successful second full clean build from its extracted final archive was not
run. Actual wheel/sdist hashes, byte sizes, SDK header receipt, isolated results,
retained wheel identities and source hashes live in
`tools/e1ap-python/verification-summary.json`; generator regression evidence is
in `tools/e1ap-python/generator-regression.json`. Independent acceptance is
recorded in `tools/e1ap-python/review.md`.

E1-P1 through E1-P6 now cover the frozen-source study, unsigned64 support,
outer PDU dispatch, finite wire qualification and C++/Python SDK delivery.
RRC remains a separate source/encoding/profile study; it is not part of this
closeout.
