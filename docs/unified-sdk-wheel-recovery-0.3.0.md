# Unified SDK 0.3.0 wheel recovery

`tools/unified-sdk/recover_wheel.py` is the executable recovery entry point
for the preserved stage-13 failure. It does not execute historical
`command.json` files, resume `produce.py`, rebuild the four-protocol SDK, or
run benchmarks. It verifies the old candidate, failure receipt, bundle,
`sdk_files`, and the complete equality of `sdk` and `relocated-sdk` before
building.

The recovery command is constructed from fixed arguments. Each run creates a
new empty wheel-output directory and a new Python-extension build directory.
The resource guard must succeed, and that output directory must contain exactly
the expected wheel created by that invocation. The command argv and its SHA256
are recorded. Installation and both installed-wheel checks run in a new venv;
the wheel digest is checked again after all tests.

The WSL recovery scope uses `MemoryMax=10737418240` (10 GiB),
`MemorySwapMax=0`, `TasksMax=128`, and `CPUQuota=150%`. The build keeps
`CMAKE_BUILD_PARALLEL_LEVEL=1` and lld `--threads=1`. Evidence records the
effective limits plus the cgroup's `memory.peak` and `memory.events`; the
recovery rejects a different memory limit and never raises it automatically.

Qualification inputs are byte-checked against the specified source snapshot.
Only the recovery implementation and its tests may be registered as explicit
before/after patches; executed qualification tools are required to match
exactly. The old SDK is reused as authenticated input, while the Python
extension is compiled in the fresh guarded build directory.

The scope is finite: outer NGAP has 131 messages and three independent NG
Setup cases; Transfer has 143 vectors. Historical F1AP/E1AP evidence is
checked for its recorded finite counts, and E1AP is not claimed as full
protocol coverage. No live RAN, exhaustive value-domain, benchmark, or
cross-platform qualification is implied.

Run recovery from the commit that contains this tool and its tests. Package a
candidate with `release.py package` only after recovery succeeds. Publication,
retirement, push, merge, review PASS, and Owner ACCEPTED remain separate
explicit actions and are not performed by recovery.
