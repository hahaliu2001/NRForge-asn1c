# N12 read-only codec coverage sample

This is a proposed 14-message planning sample derived from the Owner's Tier-A
procedure scope, not a recovered historical frozen message list. It separates
physical extraction, BODY output generation, initiating target-envelope evidence,
and independently qualified complete PDU status. A zero process exit means the
batch completed, not that all sampled messages passed.

Build the developer tool after normal Autotools configuration:

```sh
make -C tools asn1typed_codec_coverage
python tools/n12-codec-coverage/run.py --repo . \
  --asn1-root /path/to/frozen-source-root \
  --probe tools/asn1typed_codec_coverage --output /tmp/coverage.json
```

The runner checks the exact ordered six-module list and both Git blob and SHA-256
identities against N11's manifest before and after the probe. The probe parses
and fixes once, extracts all independent message graphs and initiating-target
descriptors, deletes the Parser tree, then calls all three current physical IOC
BODY renderer entrypoints. No headers are published, no generated C++ is compiled,
and no native sender/decoder or wire qualification is performed here. Descriptors
for successful/unsuccessful target outcomes are outside N11's current target API;
a missing initiating association is not proof of malformed ASN.1.

The checked-in coverage.json was captured by the equivalent manually guarded
batch invocation, followed by this runner's report/provenance check:

```sh
python tools/n12-codec-coverage/run.py --repo . \
  --asn1-root /path/to/frozen-source-root \
  --check-report tools/n12-codec-coverage/coverage.json
```

Baseline commit and probe source/binary fingerprints are recorded. The source
is tools/asn1typed_codec_coverage.c; it follows the existing developer-tool
layout rather than requiring new Automake subdirectory object rules. The ordinary
inventory includes named declaration constraints plus field/alternative use-site
constraints; it excludes bound-instance body inventories. Successful extraction
includes registry payload closure, and generation preflights the complete graph,
but first-error diagnostics and this partial inventory are not an exhaustive
missing-capability analysis. Counts are physical-mode counts, not historical
ordinary Typed-extraction counts. Failed extraction publishes no partial graph.

Numeric inventory enums are the definitions in libasn1typed/asn1typed.h:
kind primitive=0/sequence=1/sequence-of=2/enum=3/choice=4; primitive invalid=0,
BOOLEAN=1/INTEGER=2/UTF8=3/Printable=4/Visible=5/OCTET=6/BIT=7. Renderer families
0/1/2 are types/mapping/codec. Every sampled successful physical extraction has
three recorded renderer results, including failures. No report auto-accepts a
new wire profile or changes source/schema/production code.

The planning conclusions and proposed implementation order are in
[the N12 plan](../../docs/ngap-cpp-aper-n12-tier-a-coverage-plan.md).
