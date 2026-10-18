# NGAP 0.2.0 candidate producer delivery

Current authority: [coverage and implementation plan](../../docs/cpp-sdk-coverage-and-implementation-plan.md).
The 2026-10-18 Owner decision supersedes the previous cross-repository delivery
chain: ASN1 owns C++ SDK and Python wheel production, verification and publication;
RAN and 5GC consume immutable pinned artifacts. Do not run the old 5GC full-delivery
entry point. Producer orchestration migration and consolidated review are pending.
This planning update does not make existing scripts safe for publication.


The historical four-stage SDK/binding/RAN release/5GC wheel delivery authorization
has been superseded by the ownership decision above. Direct pushes to this
existing public asn1c fork remain authorized; RAN and 5GC remain private. No mainline
merge, history rewrite, old release replacement or deletion is authorized.

The SDK retains the 131-message outer NGAP PDU API. Four independent roots are
installed at `nrforge/ngap/transfers/<root>/root.hpp`, with separate namespaces
`nrforge::ngap::transfers::<root>`. `<root>` is setup_request, setup_response,
release_command or release_response. The sealed provenance lists the exact
generated C++ type and encode/decode entry names. Root headers, mapping and
codec participate in the same fingerprint and public-header hash verification.
They call the existing APER runtime; this delivery adds no second PER codec.

Python 0.2.0 adds `transfers()`, `transfer_schema(root)`,
`encode_transfer(root, body, *, limits=None, conversion_limits=None)` and
`decode_transfer(root, data, *, limits=None, conversion_limits=None)`.
Root names are the exact ASN.1 PDUSessionResource...Transfer identifiers.
Values use the existing dictionaries/lists/bytes/integers/enum labels, preserve
received order/duplicates/criticality/unknown payload and owned optional fields.
The general SDK does not apply the 5GC R1 receive-policy projection. Unknown
payloads/additions remain receive-only where the native codec refuses encoding.

Transfer input/output cannot exceed 4096 octets, even when a caller requests a
larger limit. Callers can tighten native/conversion budgets. Error codes and
bit offsets are preserved by CodecError; production receive mapping is separate.
Strict zero padding/trailing-byte checks follow the approved native policy.

`build_delivery.py` exports committed source using the reviewed snapshot rules,
exports the frozen six ASN.1 modules via git-show without changing a RAN checkout,
generates/seals/builds/installs SDK 0.2.0, generates independent oracle vectors,
builds a new wheel and tests fresh installed consumers. Pycrate is isolated to
the tool venv and independently compiles the six fixed modules. Installed tests
compare all projected typed fields and oracle wire bytes. Old outer 131 vectors
and twelve existing consumer tests remain in the regression (version is 0.2.0).
The native 250-check campaign already accepted at 460c790 is not rerun or relabeled.

The previous 5GC full-delivery entry point is withdrawn. ASN1-owned publication
orchestration must be implemented and reviewed before execution. The
resource guard is mandatory: cgroup v2 MemoryMax=6 GiB, MemorySwapMax=0,
TasksMax=128, CPUQuota=150%; compilation is serial. The guard verifies effective
limits before any build. A missing user systemd manager/delegation stops safely;
there is no sudo or unbounded fallback. Limits constrain the build descendants,
not unrelated host workloads, and do not guarantee VM/network stability.

Every run uses a new work directory. Logs/failed outputs are retained; do not
rerun into an existing directory. No automatic retries, cache reuse, cleanup,
existing tag mutation or release overwrites. Native qualification and actual
publication remain NOT_RUN until terminal evidence reports their outcomes.

Author verification: three lightweight delivery checks and five 5GC release
guard checks pass; Python syntax checked. These are not C++/wheel execution.
Final consolidated independent review and Owner acceptance remain pending.
