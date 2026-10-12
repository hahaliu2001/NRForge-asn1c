# RRC-P16 expanded finite UPER wire qualification

Baseline: `fc12b37d8d7527c5f2591c16a9728ec367e02114` on
`feature/f1ap-cpp-aper`. This batch adds developer qualification tools and evidence;
production code, frozen schemas, historical receipts and the 64-root SDK remain
unchanged. SDK expansion is deferred to P17/P18.

The expanded finite profile consumes all 85 current headers: 80 selected roots
plus five DEFAULT seed roots, including all 12 channels. Independently compiled
unmodified pycrate0.7.11 descriptors produce expected semantics and UPER bytes.
The bridge reads only public C++ model declarations and source-name metadata.
Every case requires exact wire bytes/bits, complete owned model equality and
exact decode cursor, followed by native decode of C++ encoding. Descriptor tag
order is audited across real closures. SIB1's frozen 117-bit seed joins unchanged
MIB/SIB2/MTC seeds. All 143 original P7 real cases and all 59 compound cases are
preserved by complete case data, not count alone.

Seven optional witnesses cover singleton SRB-Identity-v1700, constrained integer
list elements, fixed 32-bit list elements, SetupRelease.setup, an opaque self
carrier, MobilityHistoryReport-r16's inherited SIZE 1..16 and the direct NULL
CHOICE extension in LocationMeasurementIndication. Native descriptor paths are
recorded in the full qualification receipt; BIT STRING bounds both equal 32.
This is finite selected-value evidence, not every OPTIONAL or SIZE endpoint.

Native Contents decoding is projected back to opaque carrier bytes. Inputs use
native-produced canonical contained payloads, and native reencoding of projected
semantics must reproduce the complete original outer wire. No oracle code is
patched; malformed inner data and inner-codec qualification are outside scope.
The P7 reversed explicit-tag reference boundary remains unchanged and excluded.

Initial parallel and default-GC serial campaigns encountered compiler OOM and
are not PASS evidence. Acceptance uses a fresh serial campaign with explicit GCC
`ggc-min-expand=5` and `ggc-min-heapsize=16384` compiler collection parameters.
The isolated DL-DCCH GC compile succeeded. The complete campaign reuses that
SHA-bound compilation after rederiving its emitted test source and native
snapshot, then reruns the executable and independently verifies every receipt.
Remaining roots compile within the complete campaign; reused roots are explicit
in qualification.json. Cache keys cover runtime header/producer inputs as well
as executable/source/oracle/corpus/flags/toolchain.
Strict C++20 flags retain conversion and
sign-conversion errors. Negative checks exercise truncation, padding/trailing
bytes, input/depth/work/value/collection/bitmap/unknown quotas and rollback.
The sanitizer campaign covers the unchanged compound profile only; it does not
claim sanitizer coverage of every real root. LSan is disabled for environment
task-enumeration limits. No performance benchmark or merge.

Authoritative gzip snapshots retain the complete native generated modules and
corpus. Reports bind these snapshots, C++ sources and run/compile logs; final
verification additionally binds pinned oracle implementation files, fresh probe
and headers, and all retained baseline Git blobs. Developer-only scope permits
reuse of immutable historical APER evidence without a new full APER campaign.
See the full and sanitized qualification receipts, verification.json and the
exact-snapshot independent review in tools/rrc-wire-expanded.

Final completed results: 248 real cases plus 59 compound cases (307 total),
9,142 negative checks; all 85 roots pass, including 12 channels. Compound-only
ASan/UBSan passes 59 cases and 3,551 negative checks. Fresh native regression
suites pass libasn1typed 52/52 and libuper 3/3. No blocked root remains within
this finite selected 85-root profile. These outcomes do not imply full RRC
coverage or expanded SDK delivery.
