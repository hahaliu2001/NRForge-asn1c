# Shared capability batch replay

This directory closes the seven-capability batch authorized after N18, including
the N19 extensible INTEGER prerequisite. Component reports distinguish actual
native schema comparisons, explicit equivalent/surrogate references and
independent bit models. They are not complete NGAP-PDU qualification.

Build the current source, including parser/fixer dependencies, then run
`make -C libaper check` and `make -C libasn1typed check`. The pre-existing top-level
dependency-order issue is not changed by this batch. Install the pinned optional
Python dependencies from the component qualification requirements files; they
are not required by `make check`.

Replay these tools against the same final source:

- `tools/shared-integer-qualification/qualify.py --output tools/shared-integer-qualification/summary.json`
- `tools/batch-size-qualification/qualify.py --output tools/batch-size-qualification/qualification-summary.json`
- `tools/batch-fragment-bits-qualification/qualify.py --output tools/batch-fragment-bits-qualification/qualification-summary.json`
- `tools/shared-character-qualification/qualify.py`
- `tools/shared-null-qualification/verify.py --output tools/shared-null-qualification/native-summary.json`
- Generate and compile the private fixture driver, then run
  `tools/private-key-qualification/qualify.py DRIVER --output tools/private-key-qualification/results.json`.
- Set `FRAGMENT_VECTOR_DIR` to a new directory when running the generated
  collection test, then run `tools/fragmented-collection-qualification/compare.py
  VECTOR_DIRECTORY --output tools/fragmented-collection-qualification/results.json`.

Rebuild `tools/asn1typed_codec_coverage`. Run the frozen-source scanner with a new
work directory and the unmodified source root specified by the N11 manifest:

```sh
python3 tools/n13-full-codec-readiness/scan.py \
  --repo . --asn1-root /path/to/frozen-ngap \
  --probe tools/asn1typed_codec_coverage \
  --work /path/to/new-scan-directory \
  --output tools/shared-capability-qualification/readiness.json
python3 tools/shared-capability-qualification/verify.py \
  --readiness tools/shared-capability-qualification/readiness.json \
  --output tools/shared-capability-qualification/summary.json
```

The final verifier rejects stale component/source fingerprints, any failed
physical extraction, BODY generation or strict compilation, and any change to
the 69 previously accepted header triples. It does not execute every message
codec or replace wire interoperability qualification. Source files must remain
unchanged throughout the scanner invocation.
