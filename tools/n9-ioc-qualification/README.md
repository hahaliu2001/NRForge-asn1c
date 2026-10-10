# N9 IOC native qualification

This tool exercises the actual generated IOC types, mappings and codecs for
`libasn1typed/fixtures/ioc-generation-n9.asn1`. It does not qualify the frozen
NGAP target or a general ASN.1 compiler. There is no byte normalization,
source pruning, receive-criticality rewriting, or unknown-ID fallback for known
payload errors.

## Reproduce

Install the exact versions in `requirements.txt` into an isolated environment.
Build the repository's `check_asn1typed_ioc_render` driver using the normal
project build, then run:

```sh
python tools/n9-ioc-qualification/qualify.py \
  --repo /absolute/path/to/NRForge-asn1c \
  --render-driver /absolute/path/to/check_asn1typed_ioc_render \
  --work /absolute/path/to/scratch/n9-native \
  --output /absolute/path/to/scratch/n9-summary.json
```

Alternatively, `--generated DIRECTORY` accepts the five roots already rendered
by that driver: `main`, `empty`, `closed`, `ext`, and `empty_ext`, each with
`_types.hpp`, `_mapping.hpp`, `_codec.hpp`, and `_adapters.hpp`. Namespaces must
be `foo::nrforge::<root-prefix>`. Use `--cxx clang++` for a second compiler.
The runner compiles `driver.cpp` and the actual runtime in its scratch directory
with strict C++20 warnings. It never invokes a shared repository build.

Both package versions and every case ID, oracle identity, octet count, and wire
SHA-256 must match `accepted-profile.json`. An unexpected behavioral disagreement
fails before profile comparison. A profile change also fails, including a change
that preserves aggregate counts. The runner never updates the accepted profile.
Only after both checks succeed does it publish a summary with zero unresolved
production disagreements. Temporary raw vectors and generated modules remain in
scratch; the repository profile contains no large binary payloads.

## Frozen results

The accepted run contains **258 checks, all matching**:

| Check kind | Checks | Result |
|---|---:|---|
| Native pycrate sender to generated decoder | 97 | 97 matched |
| asn1tools raw-framing sender to generated decoder | 97 | 97 matched |
| Generated known-value encoding compared with both native byte streams | 49 | 49 matched |
| Owned unknown records refused by generated encoder | 12 | 12 matched |
| Strict known-payload literal rejection | 3 | 3 matched |

The two sender groups include 49 known-value cases, 12 declared unknown cases,
and 36 empty/extension/closed-registry policy cases. Known cases cover distinct
BOOLEAN wrappers with the same payload type, INTEGER boundaries, optional
compound fields, nested constrained collections, received criticality mismatches,
reordered duplicates, missing entries and an empty container. Unknown payloads
include a separately declared compound type and OCTET STRING sizes 0, 1, 127,
128, 16383, 16384, 16385, 32768 and 65536. These are native encodings, including
fragmented open-type payloads. The generated decoder exposes unknown numeric ID,
received criticality and complete owned payload bytes. The driver overwrites the
main input buffer after decoding before inspecting owned records; other roots
use a temporary input buffer destroyed before inspection. All 12 main unknown
cases also check explicit encode refusal at bit 24.

Empty extensible value and extension registries retain arbitrary unknown IDs.
A non-extensible registry rejects unknown IDs at bit 34, after the 16-bit
container count, 16-bit identifier and 2-bit criticality. This rejection is an
independent contract expectation; native tools are used to produce the wire,
not to establish registry policy.

## Oracle limits

`sender.asn1` is an independent parameterized IOC sender. Its additional declared
rows 300 and 301 are unknown to the receiver. The sender's symbolic names and
row order are not used as receiver dispatch keys. pycrate's raw OPEN spelling
`_unk_004` is a tool marker, not an identifier: the received numeric selector
comes from the preceding identifier field. All three received criticalities are
preserved, including values different from a known registry row's expected value.
pycrate may print informational messages for empty or missing table rows; this
output is not suppressed or treated as a wire transformation.

asn1tools 0.167.0 cannot compile this parameterized IOC schema (its parameter
preprocessing raises `TypeError`). `raw-framing.asn1` therefore uses an explicitly
raw OCTET STRING field for the same aligned length-and-octet frame and independent
standalone payload encoders. Its results are **raw-framing evidence**, not native
typed IOC dispatch evidence. The full parameterized schema is compiled and used
by pycrate.

pycrate 0.7.11's known OPEN decoder accepts some inner nonzero padding and trailing
octets. This is not evidence that the generated strict decoder should accept
them. Three literal vectors independently require BOOLEAN nonzero padding to
fail at bit 63, and BOOLEAN/INTEGER trailing payload octets to fail at bit 64.
Known IDs never downgrade malformed payloads to unknown records. These are
contract rejection checks, not differential matches against pycrate's lax
behavior.

The framing model follows [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en),
clauses 11.2.1–11.2.2 (complete-encoding octets in open types) and 11.9 (length
framing and fragmentation). Runtime transaction atomicity, nested offset
translation, semantic/staging budgets, allocation-failure injection and sticky
errors are verified separately by the N9 runtime and generated-code suites.
This native run does not replace those tests, perform a benchmark, or establish
whole-target interoperability.
