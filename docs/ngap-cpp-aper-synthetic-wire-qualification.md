# Synthetic APER Independent Wire Qualification

Date: 2026-10-09. Baseline: `e0b6f2d97af1c4695e8686a8a3d4e5b1ae7ff1a7`.

## Scope and verdict

**PASS within the evidence boundaries below.** This qualifies the two existing
synthetic Count/Selection/Packet fixtures against an external implementation and
an independent bit layout model. It does not qualify general ASN.1, NGAP, every
accepted generator shape, allocation failure, or the entire runtime API.
No production code or approved contract is changed.

| Evidence | Result and boundary |
| --- | --- |
| Original AUTOMATIC TAGS fixture | External APER encode/decode agreement and independent bit model: PASS |
| Reversed explicit-tag fixture | Canonical tag order, source-order storage, complete bytes and values: PASS against independent model |
| Reversed wire interoperability | PASS using the untouched original fixture as the external comparator for the same semantic wire layout |
| External library compiling the untouched reversed fixture | Disagrees with canonical tag order; NOT claimed as external native-schema agreement |
| Truncation, padding, trailing bytes, budgets, overflow | PASS against the approved strict S3 policy; external decoder rejection is not the oracle |

The two fixtures retain identical constraints and SEQUENCE order. Their CHOICE
source orders differ. Source-order storage must remain flag/count in the original
and count/flag in the reversed fixture, while the canonical wire order remains
flag/count. The check validates the decoded storage ordinal as well as payload.

## Oracle and method

The reference is `asn1tools==0.167.0`, codec `per` (aligned PER), with exact pinned
dependencies in `libasn1typed/qualification/requirements.txt`. It compiles the
existing schema files directly; neither library source nor schemas are patched.
It shares no NRForge parser, fixer, Owned IR, naming, renderer, or runtime code.

`qualify_wire.py` independently constructs bits and field spans without importing
generated traits. Its small model is deliberately specific to these schemas.
Normative basis: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items),
10.2 and 23.2 (canonical outer tag order), 11.5.7.3 (aligned two-octet integer),
12 (BOOLEAN), 19 (presence bitmap and sequence order), and 11.1 (complete padding).
The stricter failure priority and resource budgets come from the approved
[S3 contract](ngap-cpp-aper-s3-runtime-contract.md), not from external permissiveness.

Once per fixture, a real Parser → Fixer → Owned IR driver generates the three
C++ headers after deleting the parser tree. The existing render driver also
checks deterministic codec output. The qualification adapter compiles these
headers with the actual runtime.

For each valid value:

1. The model's complete bytes must equal the external encoder's complete bytes.
2. Generated C++ encodes the value. Every byte, complete length and all five
   CompleteEncoding metrics are compared with the independent expectation.
3. Generated C++ decodes the externally supplied bytes, checks all fields,
   OPTIONAL absent/false/true, payload and storage ordinal.
4. The actual generated C++ bytes are passed to the external decoder and its
   semantic value is checked. This is cross-decode, not local roundtrip evidence.

All Python checks use explicit exceptions, not removable `assert` statements;
the C++ adapter also has no assert-based checks. Report files are deleted before
a rerun, so a failed run cannot leave an earlier PASS at the same destination.

## External CHOICE ordering limitation

With the untouched reversed schema, asn1tools preserves CHOICE source order:

| Selection value | Canonical / NRForge bytes | Native reversed asn1tools bytes |
| --- | --- | --- |
| flag = true | `40` | `c0` |
| count = 255 | `8000ff` | `0000ff` |

In its installed `codecs/per.py`, CHOICE compilation calls `compile_members`
without tag sorting, and `Choice.create_maps` enumerates those members. The suite
explicitly reproduces and records this discrepancy. If the pinned oracle's
behavior changes, that check fails and requires re-review of the reference.

An exploratory pycrate 0.7.11 probe also produced these native reversed bytes;
that probe is not part of the committed qualification gate. No external library
is treated as authoritative for the reversed source-order → canonical-order step.
That step is covered by the independent tag-rank model and the normative rule.
There is no silently reordered comparator schema or suppression of byte mismatches.

## Corpus and recorded evidence

The full run covers both namespaces:

| Category | Count |
| --- | ---: |
| Count: exhaustive 0..65535 | 131,072 |
| Selection: two BOOLEAN values and seven integer boundaries | 18 |
| Packet: seven count boundaries × nine selections × three presence states | 378 |
| Packet: deterministic samples, seed 6912021 | 512 |
| Total valid values and external cross-decodes | 131,980 |
| Strict negative requests | 10,548 |
| Exact-budget successful requests | 1,640 |
| Total adapter requests / checked response lines | 144,168 / 276,148 |

Integer boundaries are 0, 1, 255, 256, 32767, 32768 and 65535. For boundary values,
negative cases include every short octet prefix, three trailing suffixes, each
individual alignment/final-padding bit and all padding bits together. Input,
output and wire budgets are tested at the exact requirement and one unit less,
in both directions where applicable. Overflow cases are 65536, 2^32 and
UINT64_MAX at standalone and nested field positions. Every failure checks its
error code and bit offset and requires failure rather than a published value.

The [machine-readable full result](ngap-cpp-aper-synthetic-wire-qualification-results.json)
records the fixture/harness hashes, corpus hash, baseline, dependency versions,
compiler, counts and external discrepancy. Corpus SHA256:
`2bd134b043629b4f9807f4b2880a6626d5834d84966a54ce326b6324c84d37cd`.
Quick mode retains the strict boundary corpus but reduces exhaustive Count to
boundaries: 922 valid values. It is not a substitute for the full run.

## Reproduction

From a checkout of the baseline plus this patch, with Autotools, flex, bison,
a C compiler, a C++20 compiler and Python 3 available:

```sh
autoreconf -fi
./configure
make -j4 -C libasn1common
make -j4 -C libasn1parser
make -j4 -C libasn1fix
make -j4 -C libasn1typed check_asn1typed_codec_render
python3 -m venv /tmp/nrforge-wire-venv
/tmp/nrforge-wire-venv/bin/pip install -r libasn1typed/qualification/requirements.txt
/tmp/nrforge-wire-venv/bin/python libasn1typed/qualification/qualify_wire.py --report /tmp/nrforge-wire-full.json
make -j4 -C libaper check
make -j4 -C libasn1typed check
```

The explicit dependency build order avoids the pre-existing top-level SUBDIRS
order issue. `--build-dir` accepts an existing configured build tree containing
the fixtures and renderer driver. The qualifier's own generated headers and
binary live in a temporary directory. It does not commit, push or change schemas.
External qualification remains opt-in; normal `make check` gains no Python
dependency. All three new qualification sources are in EXTRA_DIST.

## Validation and independent review

- Full qualification: PASS with g++ 13.3, strict C++20 including conversion warnings.
- Existing libasn1typed checks: 9/9 PASS; libaper checks: 1/1 PASS.
- Generated C++ adapter/runtime with ASan + UBSan, no-recover, Python `-O`:
  quick corpus PASS with leak detection disabled. LSan was actually attempted
  but failed to inspect `/proc/.../task` under ptrace; no usable LSan verdict is
  claimed. The C renderer was not rebuilt with sanitizers for this patch.
- Independent agent review: PASS / Ready to Commit, with no blocking findings.
  It independently reran the full corpus and reproduced its hash and counts;
  after final harness changes it reran quick mode. A scratch-only mutation of
  the encoder's first output nibble was rejected at response 0 without a PASS
  report. Scope, external limitation, both cross-decode directions, errors,
  counters and report lifecycle were reviewed. Its generation-frequency wording
  observation is corrected above. Claude was not required or used for this gate.
- No benchmark, real NGAP qualification or wider schema qualification was run.
