# RRC-P7 finite wire qualification

This developer-only campaign consumes authenticated, unmodified external ASN.1
sources and freshly generated P6 headers. It does not install an SDK. Build and
configure the repository first; install the pinned `requirements.txt` in the
qualification environment. Every work directory must be fresh.

```sh
python3 tools/rrc-uper/check.py --work /tmp/p7-focused
python3 tools/rrc-uper/generate.py --asn1-root /path/to/schema \
  --probe /tmp/p7-focused/probe --work /tmp/p7-matrix \
  --output /tmp/p7-matrix.json
python3 tools/rrc-wire-qualification/audit_reference.py \
  --generated /tmp/p7-focused --work /tmp/p7-reference \
  --output /tmp/p7-reference.json
python3 tools/rrc-wire-qualification/qualify.py --asn1-root /path/to/schema \
  --probe /tmp/p7-focused/probe --generated /tmp/p7-matrix \
  --work /tmp/p7-wire --output /tmp/p7-wire.json --jobs 4
python3 tools/rrc-wire-qualification/qualify.py --asn1-root /path/to/schema \
  --probe /tmp/p7-focused/probe --generated /tmp/p7-matrix \
  --work /tmp/p7-wire-asan --output /tmp/p7-wire-asan.json \
  --sanitize --only-compound --jobs 1
ASAN_OPTIONS=detect_leaks=0 python3 tools/rrc-uper/check.py \
  --work /tmp/p7-focused-asan --sanitize
```

`--asn1-root` contains `rrc/asn1/...`, not just the `rrc` directory. The generator
matrix must reproduce all 64 sealed P6 header hashes. The full campaign compiles
the six original modules in a separate native pycrate compiler process. Native
semantics and wire are built without reading C++ codec bodies or production IR.
The bridge reads public model declarations and source-name metadata solely to
construct the corresponding C++ value. Native encode -> C++ decode compares the
complete owned model, cursor and bit count; C++ encode -> native decode compares
normalized native semantics and exact bytes/bits. No oracle implementation is
patched. Source fingerprints and external authentication are checked again at
completion. Each root has a separate strict C++20 translation unit.

The published full profile has 64 real roots (59 selected plus five DEFAULT
seeds), 143 real cases and 59 separately owned compound cases. Real cases cover
mandatory low/contrast values, DEFAULT presence/suppression, mandatory-baseline
channel CHOICE branches and three frozen bare seeds. This does not exhaust every
OPTIONAL, SIZE boundary, nested branch or field combination. All seven generated
channel roots receive independent cases; five other channels remain blocked.
SIB1 remains blocked; MIB, SIB2 and MTC frozen bytes remain unchanged.

The compound fixture covers root/extension indexes, ENUM, DEFAULT, addition
groups, lengths 127/128, collection and octet fragmentation through 16384/16385,
collection length 65536, offsets 0..7, future SEQUENCE projection and unknown
CHOICE retention (including offset 3). Negative checks exercise truncation,
appended octets, padding, input/depth/work/value/collection/bitmap/unknown quotas
and transaction rollback. For wires <=256 bits every truncation point is checked;
large wires use explicitly sampled points. Sanitizer evidence is compound-only,
plus the separate P6 focused allocation/rollback harness, not all real roots.

`reference-boundary.json` resolves the P6 reversed explicit-tag gate against
X.680/X.691 and records actual freshly generated C++ output. Both pinned native
references encode that original synthetic CHOICE in declaration order; it is
excluded from native wire PASS. Real root closures are separately audited for
reference-compatible tag order. The pycrate group DEFAULT input requires explicit
`fallback=2`; its encoder suppresses that default and decoder restores it. This
input limitation is disclosed rather than patched away.

See `docs/rrc-cpp-uper-p7-wire-qualification-closeout.md`, `verification.json` and
`review.md`. No full RRC coverage, installed SDK, benchmark or merge is claimed.
