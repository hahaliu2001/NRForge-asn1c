# RRC-P16 expanded finite wire qualification

Developer-only successor to P7; it does not install or expand the SDK. Build the
repository, install the pinned requirements, and use fresh work directories.
The ASN.1 root contains `rrc/asn1/...`; all six frozen original modules are
SHA-authenticated against the source manifest. Generate the 85 headers using
`tools/rrc-uper/probe.c` and the root list in
`tools/rrc-null-open/readiness.json`. Each header must reproduce that matrix hash.

```sh
python3 tools/rrc-uper/check.py --work /tmp/p16-focused
python3 tools/rrc-uper/generate.py --asn1-root /path/to/schema \
  --probe /tmp/p16-focused/probe --work /tmp/p16-headers \
  --output /tmp/p16-generation.json
python3 tools/rrc-wire-expanded/qualify.py --asn1-root /path/to/schema \
  --probe /tmp/p16-focused/probe --generated /tmp/p16-headers \
  --work /tmp/p16-wire --output /tmp/p16-wire.json --jobs 1
python3 tools/rrc-wire-expanded/qualify.py --asn1-root /path/to/schema \
  --probe /tmp/p16-focused/probe --generated /tmp/p16-headers \
  --work /tmp/p16-wire-asan --output /tmp/p16-wire-asan.json \
  --sanitize --only-compound --jobs 1
```

Each root is a strict C++20 translation unit. An optional `--compile-cache`
records completed compilations. Cache keys bind the header, emitted test source,
runtime object, full flags/toolchain, oracle source/descriptor snapshots, corpus
and qualification source hashes. Cache artifacts are rehashed before reuse;
the executable always runs again and all CASE/NEG receipts are reprocessed with
the current native codec. Reports disclose reused roots and original commands. Serial compilation is the default:
large channel closures can exceed an 8 GiB environment even in isolation.
GCC compiler garbage collection uses explicit `ggc-min-expand=5` and
`ggc-min-heapsize=16384` parameters. Strict diagnostics remain enabled.
The campaign builds native values before reading public C++ model declarations.
Native encode -> C++ decode compares the full owned model and exact cursor;
C++ encode -> native decode compares normalized semantics, bytes and bit counts.
Native CHOICE tag-order audits remain active. No codec instructions or IR derive
expected bytes, and no oracle implementation or schema is patched.

The finite real profile has 85 roots: 80 selected plus five DEFAULT seeds,
including all 12 channel roots. Mandatory low/contrast values, mandatory-baseline
channel branches and frozen MIB/SIB2/MTC/SIB1 seeds are supplemented by seven
native descriptor-selected optional witnesses. See `witnesses.py` and the path
records in `qualification.json`. The original P7 143 real cases are compared by
identity and complete case dictionaries; the 59 compound cases are unchanged.
The copied public model bridge, compound ASN.1 fixture and requirements remain
byte-identical to P7.

Pycrate automatically decodes Contents constraints. Carrier inputs therefore
use native-produced canonical inner bytes; descriptor-driven projection restores
opaque bytes after native decode and native reencoding must exactly reproduce
the original outer wire. This qualifies those carrier bytes, not arbitrary
malformed inner data or an inner payload codec. No all-field/all-value, every
OPTIONAL, every SIZE endpoint, nested Cartesian branch or full RRC claim follows.
The P7 reversed explicit-tag oracle boundary remains excluded and unchanged.

Compound negatives cover truncation, padding/trailing data, quotas and rollback.
Real wires at most 256 bits test every truncation point; larger wires use sampled
points. ASan/UBSan coverage is compound-only, not all 85 real roots. LSan is
explicitly disabled because this environment cannot enumerate tasks.

`frozen.py.gz`, `fixture.py.gz` and `corpus.json.gz` in each campaign work directory
are authoritative complete snapshots, bound by SHA256. The report also hashes
C++ sources and compilation/execution logs. Raw large `.py`/`.json` scratch files
are convenience copies; acceptance uses the compressed snapshots. No performance
benchmark is run. SDK delivery remains 64 roots pending P17/P18.
