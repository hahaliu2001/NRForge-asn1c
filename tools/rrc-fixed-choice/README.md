# RRC-P13-A verification

Run from a configured, built repository with the frozen NRForge-RAN schema:

```sh
python tools/rrc-fixed-choice/check.py --asn1-root /path/to/schema \
  --work /new/work/directory --output tools/rrc-fixed-choice/readiness.json
```

The driver recompiles current probes and focused C/C++ checks, authenticates the
six source files, verifies ten P10 instance contexts, compares all 67 P12 accepted
RRC headers, attempts strict compilation of all seven newly rendered roots, and
runs the shared 361-message/1,773-header APER regression. It does not rebuild the
SDK, run real-message wire qualification or benchmark. Its only tolerated strict
compile failure is LocationMeasurementIndication's two existing NULL-extension
unused-io errors; it records BLOCKED_EXISTING_RENDERER with the diagnostic.

`check_extract.c` is also an Automake native test. The direct ASan/UBSan run
instruments the current extractor and test source. Leak scanning is disabled
because the environment cannot enumerate `/proc` tasks. Native regression totals
and the final exact source snapshot are recorded separately in verification.json.
See the contract/closeout document in docs and review.md for the acceptance gate.
