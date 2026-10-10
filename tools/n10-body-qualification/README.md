# N10 — actual UEContextReleaseCommand body qualification

This integration uses the actual physical owned graph and generated codec for
**UEContextReleaseCommand BODY only**. It does not encode an InitiatingMessage
open field or complete NGAP-PDU envelope, enforce mandatory-IE/criticality
application policy, or claim general NGAP qualification.

## Reproduce

Install `requirements.txt` into an isolated Python environment. Build the generic
`tools/asn1typed_ioc_probe` with the normal repository build. Supply the untouched
NRForge-RAN source root containing the six paths in
`tools/qualification/ngap-rel18.modules`:

```sh
python tools/n10-body-qualification/qualify.py \
  --repo /absolute/path/to/NRForge-asn1c \
  --asn1-root /absolute/path/to/NRForge-RAN/src \
  --probe /absolute/path/to/asn1typed_ioc_probe \
  --work /absolute/path/to/scratch/n10-body \
  --output /absolute/path/to/scratch/n10-summary.json
```

The runner verifies all six ordered Git blob and SHA-256 identities **before**
generation or native compilation. `source-manifest.json` records the exact
TS 38.413 V18.10.0 authority at NRForge-RAN commit
`d6e514a33ec3695925c24aa292900514b371b074`. No module, declaration,
constraint, empty object set or source byte is filtered or rewritten.

It invokes the actual generic probe with `--verify-determinism`, root
NGAP-PDU-Contents/UEContextReleaseCommand and namespace `n10::body`. The probe
releases the Parser/Fixer tree before rendering. Fresh generated headers stay
in scratch. The runner then compiles `driver.cpp`, `body_adapter.hpp` and the
real runtime with strict C++20, conversion/sign warnings, `-Werror` and
`-DNDEBUG`, and executes the driver's active-check `self-test` before comparing
native cases. It does not trust a supplied precompiled target driver or unrelated
headers. `--cxx` selects the compiler.

The primary oracle is **pycrate 0.7.11**, freshly compiled from **all six exact
modules** in each run (1223 types, 650 sets and 698 values). No installed NGAP
schema or different release is substituted. Byte streams are passed unchanged.

## Accepted evidence

There are **375 native BODY cases** and **757 matched checks**:

| Operation | Checks | Result |
|---|---:|---|
| Native complete body encoding to actual generated decoder | 375 | 375 full models match |
| Generated typed body encoding, exact native bytes and native back-decode | 231 | 231 match |
| Opaque retained values refused by generated encoder | 144 | 144 match |
| Independent literal body sidecar decode/refusal | 2 | 2 match |
| Independent literal rejection code and offset | 5 | 5 match |

Matrix cases include all **81 schema-known Cause enum values**, the first unknown
extension index of each of the five enums and indexes 63/64/255/256, all three
UE-NGAP-IDs branches and all six Cause branches; the opaque choice-extension
branches are receive-only and have explicit encode-refusal checks. Integer cases cover octet-width
thresholds and the AMF 40-bit / RAN 32-bit maxima. The ordered model preserves
received criticality mismatches, duplicates, missing rows, empty containers,
reordered IEs, nested optional extension containers, unknown numeric IDs and
unknown payload bytes. Unknown scalar ENUMERATED indexes remain re-encodable.
Opaque IOC and SEQUENCE sidecars are explicitly refused by the encoder.

Opaque cases use IDs 0/65535, all three received criticalities and payload lengths
1/3/127/128/16384/65536, in four locations: main IE, UE choice extension, Cause
choice extension and optional pair extension container. The driver destroys the
physical input after decoding, checks copied/moved owned values, mutates one
copy's unknown storage and confirms independence. Known wrappers are associated
by numeric ID through generated mapping metadata, not variant or row ordinal.

Literal checks reject inner Cause padding at bit 127, inner trailing octets at
128, known-open alignment at 42, complete-body trailing octets at 128 and a
truncated known Cause payload at 120. These are receiver contract checks, not
agreement with pycrate's comparatively lax known-open padding/trailing policy.
The independent body sidecar literal has bitmap width 2, index 1, payload `80`;
its wire is `80000002800180`.

`accepted-profile.json` freezes every case ID, operation, octet count and wire
SHA-256, the complete source manifest, freshly generated header hashes, literal
errors and exact oracle-limit observations. Any unexpected behavior fails before
profile comparison. A changed exact profile also fails, even when total counts
remain the same. The runner never updates the accepted profile. Only after all
checks succeed does it publish zero unresolved **production** disagreements.

## Narrow native-oracle limitations

The accepted profile preserves **26 observations**, without changing oracle
input, bytes, values or output:

- Two SEQUENCE suffix observations: native input `_ext_0` with payload `80`
  fails, while native input `_ext_2` emits `80000002800180` and decodes under
  the key `_ext_1`. The exact supplied key, observed key/error, bytes and hash
  are stored. This naming behavior is not normalized into semantic agreement.
  Receiver sidecar behavior is checked by an independent literal instead.
- Twenty-four full-body native self-decode exceptions occur for 16K/64K opaque
  payloads inside UE choice extensions or pair extension containers. The native
  **sender bytes are not altered**. Each case independently checks the body
  header, container count, first identifier 114, criticality/alignment and
  canonical aligned short/long/C1–C4/terminal determinants. It concatenates the
  complete outer known frame, proves the child bytes identical to standalone
  native UE-NGAP-IDs encoding, and successfully native-decodes that child to the
  original value. Exact case/wire/child lengths and hashes and native exceptions
  are stored. The actual generated decoder matches the full original model in
  every case. The oracle limitation is its fragmented known child handling in
  the full-body decoder, not permission to repair sender output.

The framing interpretation follows [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en),
clauses 11.2 and 11.9. No secondary whole-body native oracle is claimed.
Runtime transaction, shared-budget, sticky-error and allocation behavior are
also checked by the persistent driver and existing runtime suites. This bounded
body qualification does not replace those tests, qualify the envelope, benchmark
performance or certify application-level protocol success.
