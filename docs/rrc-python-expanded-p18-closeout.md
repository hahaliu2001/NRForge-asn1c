# RRC P18 expanded Python SDK closeout

`nrforge-rrc` 0.2.0 exposes the finite P17 85-root owned-model SDK (80 selected
roots and five DEFAULT witnesses, including 12 channels). It does not claim
full RRC coverage. Historical P9 0.1.0 package and APER sources are preserved.

The initial and final source-archive wheel builds both completed successfully;
final wheel installation passed RRC 12, NGAP 12, F1AP 9 and E1AP 10 tests in a
fresh isolated four-protocol environment. RRC exercised 248 real cases over 85
roots in all three completion modes, three supplemental native cases, 19 DEFAULT
omissions, 5510 malformed/quota checks, owned lifetimes, concurrent calls and all
24 import permutations. Five generator tests passed. Evidence logs are retained.

ELF inspection finds the Python entry point, no direct nrforge namespace
exports, no RPATH/RUNPATH and no libnrforge dynamic dependency. Weak std template
symbols containing nrforge types remain; complete ELF symbol hiding is not
claimed. Successful four-protocol runtime tests are the coexistence evidence.

After conversation truncation, the former scratch repository and workflow
records were unavailable. The original final wheel, sdist, staging receipt,
generated bindings, SDK, installed venv and execution logs survived in /tmp.
The 12 package sources were restored exactly from the built sdist. Workflow
scripts and this closeout were reconstructed on the remote P17 baseline.
Verification binds this new snapshot; the unavailable previous snapshot
1d085e3e2edf75f7c683aaae3536d58ef82e528c82028eac7462ec7b90a20dfa
is recorded only for recovery provenance and is not reused as approval.

The original APER wheel archives disappeared with the former scratch schema;
all three accepted wheel archives were recovered from saved deliveries. Their
archive hashes match accepted Git receipts, and all substantive installed
members match those wheel bytes (pip rewrites dist-info/RECORD).
The old restoration journal documents the successful prior isolation run,
not the existence of deleted scratch paths today. No wheel rebuild or rerun of
previously passing consumer suites is needed to verify unchanged package bytes.
Fresh independent review of the reconstructed scripts and current artifact
chain is required before publication. Its authoritative verdict is review.md.
No benchmarks or merge were performed.
