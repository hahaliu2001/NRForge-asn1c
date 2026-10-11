# RRC-P12 independent review

Status: **PASS**, no remaining blockers.
Reviewer: independent agent `/root/e1_sdk_review`.
Baseline: `82ff13ff3172640887c6c847a258e1b8a2a07d13`;
branch: `feature/f1ap-cpp-aper`.

Approval binds canonical sorted compact JSON of `source_sha256` and
`verification_sha256`, without a trailing newline. This unique review receipt
is excluded from the snapshot and may be added before commit/push. No bound
source may change afterward. No merge is approved or performed.

- Exact canonical snapshot SHA256:
  `99f02dfd106f80e9165d1c769d92e99856020fef1d9c28a50977e6d108ff270f`.
- Verification summary SHA256:
  `f9c1a7dadadcee136243c575cf715eefe5035c6e337d4499d15ad8f489ead4c1`.
- Readiness report SHA256:
  `42613cd14208a73a78f9d5be9bda564a1bee9b4c77173f3294a4354677d8111d`.

Before production edits, independent model review confirmed that existing list
SIZE, named element_type and primitive body constraints suffice. The reviewer
approved skipping a separate P12-A IR implementation batch. This was a model
contract decision, distinct from the final implementation acceptance below.

Core review confirms that only direct constrained INTEGER/BIT STRING expressions
are promoted to anonymous named primitive bodies. Named use restrictions,
named bits, actual parameters and unsupported mixed constraints retain their
rejection boundaries. Complete effective constraints are validated by the
existing primitive extractor. List and element constraints are independently
owned. The legacy anonymous diagnostic view takes its identity from the newly
added owned type, avoiding a stale incoming reference after array reallocation,
and never rewrites the borrowed AST. No new IR, renderer or runtime interface.

The reviewer directly compiled current extractor and element test sources with
strict GNU11 warnings and ASAN/UBSAN, then executed ownership, namespace,
bounds, 37 allocation-failure rollback positions and quota checks successfully.
Independent strict C++20 recompilation/execution of ten generated roots under
three completion modes passed hand-calculated determinant bytes/bits, bounds,
truncation and quota checks. LSan was not run or claimed: process task enumeration
is unavailable in this environment. Current extractor/test instrumentation is
explicitly distinguished from rebuilding all parser/fixer libraries with sanitizers.

All twelve bound source/report hashes, the exact snapshot, producer/archive,
binary and log guards match. All 2,627 baseline blobs outside the five modified
existing files are preserved. Native logs report 64 passes and zero failures/errors.
Existing-file changes are .gitignore for the new test executable, top-level
Makefile.am distribution entries, libasn1typed/Makefile.am test registration,
asn1typed_extract.c and the intentionally updated singleton-list negative fixture
in check_asn1typed_extract.c.

Independent checks compared actual freshly generated APER files against history:
F1AP 158 messages / 948 headers, NGAP 131 / 393, E1AP 72 / 432, totaling
361 messages / 1,773 byte-identical recorded headers. Parse/fix, owned lifetime,
BODY/envelope extraction and generation pass. NGAP historical comparison is
BODY-only; current envelopes are generated without historical hashes.

All 65 P11-supported physical RRC header hashes remain identical. The new
85-root matrix is 67 render PASS, eight extraction blocked and ten render blocked.
Five former extraction failures advance to CHOICE tag evidence blockers. Two
roots newly render and pass strict syntax compilation; no real-message wire
qualification is claimed. INTEGER union element ownership passes while its
existing UPER renderer profile remains blocked. Nested collection tests use a
constructed wrapper; the existing parser direct-nesting limitation is not repaired.
Delivered C++/Python SDK and accepted qualification receipts remain at 64 roots.

No benchmark, schema change or SDK rebuild was performed. Approval covers this
exact capability batch plus the unique review receipt, with commit/push to the
existing branch and no merge.
