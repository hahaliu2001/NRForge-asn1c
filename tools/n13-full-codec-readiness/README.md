# N13 full codec readiness scan

Read-only frozen-source inspection; successful syntax compilation is not wire qualification. See [the result and shared-capability plan](../../docs/ngap-cpp-aper-n13-full-codec-readiness.md) and [all 131 rows](readiness.json).

Build `tools/asn1typed_codec_coverage` from the current tree in an isolated configured build. Retain the six original modules at the source authority specified by `tools/n11-envelope-qualification/source-manifest.json` and `tools/qualification/ngap-rel18.modules`. This scanner uses the N12 guard and a bounded recognizer of those frozen procedure declarations, not a general ASN.1 parser.

```sh
python3 tools/n13-full-codec-readiness/scan.py \
  --repo /path/to/NRForge-asn1c \
  --asn1-root /path/to/frozen-source-root \
  --probe /path/to/build/tools/asn1typed_codec_coverage \
  --work /path/to/new-empty-scan-directory \
  --output /path/to/readiness.json
```

`--work` must not exist. Each invocation performs one Parser/Fixer pass for all messages. Generated headers, translation units, compiler/probe logs and the raw ordinary inventory stay in that work directory. Only generation-success triples reach compilation. `--cxx` defaults to g++; it accepts a shell-tokenized compiler command without executing a shell. Output is published after source and tool-input stability checks. Build the probe from the recorded source; the report hashes the binary but does not independently prove its build provenance.

Family 0/1/2 means BODY types/mapping/codec. Target-descriptor results sample only the initiating-message API. Feature inventory excludes bound-instance internals and extraction-masked dependencies. Reported clusters use the first failing family; shared error text need not mean one semantic defect. Strict `-fsyntax-only` compilation does not link or execute a codec. Only UEContextReleaseCommand carries the historical, bounded N11 wire qualification label.
