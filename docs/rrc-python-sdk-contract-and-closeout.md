# RRC-P9 Python SDK delivery contract

Baseline: `4a7124a4e5314ad14a000abe790a7394f3aefb0c`, branch
`feature/f1ap-cpp-aper`. Distribution `nrforge-rrc` 0.1.0, import `nrforge_rrc`.
The initial wheel target is CPython 3.12 Linux x86-64. The package statically links
the installed sealed P8 C++ SDK, with hidden archive symbols. It ships no schema,
native oracle, generator dependency or fallback to a source checkout at runtime.

`identity()`, `roots()`, `manifest()` and `schema(module, root)` describe the same
finite P8 profile: 64 supported roots and 21 blocked identities, seven channels,
59 selected roots plus five DEFAULT seeds. A blocked or unknown identity cannot
be passed to a codec. RRC remains an ordinary root API without an invented PDU or
procedure dispatcher. Full RRC wire coverage remains false; SIB1 is blocked.

`encode(module, root, value, *, completion="octets", limits=None,
conversion_limits=None)` returns `octets`, `semantic_bits`, `encoded_bits`,
`padding_bits` and `empty_substitution`. `decode(module, root, data, *,
logical_bits=None, completion="octets", limits=None, conversion_limits=None)`
returns an independently owned `value` and `semantic_bits`. Completion also accepts
`contained_bits` and `exact`; meanings and empty substitution follow P8 unchanged.
`CodecError.code` and `.bit_offset` retain the first C++ codec error.

Conversion uses ordinary Python dict/list/bytes/bool/int/None. SEQUENCE field names
are original ASN.1 names (extension groups use their public `@groupN` identity).
Primitive wrappers are exposed as their scalar, SEQUENCE OF as lists, known
ENUMERATED as source labels, unknown enum extensions as `{"extension_index": n}`.
CHOICE is `{"type": source_alternative, "value": value}`; unknown extensions use
`{"type": "@unknown", "value": {"index": n, "payload": bytes}}` where supported
by the C++ model. Missing required fields and unknown keys fail. Optional omission
or None means absence; DEFAULT omission preserves the initialized effective C++
default and decoding materializes effective values. False is distinct from absence.
The accepted profile contains no optional NULL member; such a future model fails
binding generation until its presence representation is separately qualified.
BIT STRING uses `{"octets": bytes, "bit_count": n}`. Storage overflow, negatives
for unsigned values and bool-as-integer fail before narrowing; ASN.1 constraints
remain the C++ codec's responsibility.

Codec `Limits` and independent Python conversion depth/node/byte/collection quotas
are keyword-only. Input Python values and data are converted/copied with the GIL
held; it is released solely around owned C++ encode/decode, then reacquired before
creating Python results. No returned value borrows Python input, codec temporaries,
schema objects or C++ staging. Existing unknown-value and extension policies remain
those of P8, without new acceptance or retention claims.

Bindings are generated solely from authenticated installed public models. The
generator checks the complete public file inventory, exact header and metadata
hashes, SDK version/fingerprint and finite identity closure. Unrecognized model
declarations fail closed. Configuration, every build and installation recheck the
public seal; the C++ header/archive identity guard also participates in linking.
pybind11 3.0.1 and scikit-build-core 0.11.6 remain pinned. RRC sources are separate
from the delivered shared APER binding sources.

Qualification reuses the immutable P7 143 real cases with independently prepared
native semantic values, exact bytes/bits and full owned value comparisons. It is
Python integration of the accepted finite codec, not new wire qualification or
all-field/all-value coverage. Wheel/sdist delivery, extracted-sdist build, isolated
installed consumption, owner lifetime, errors/quotas, all four-protocol import
orders and retained APER tests are acceptance gates. Source/schema/SDK/build inputs
must be unavailable during installed tests. No benchmark or merge is authorized.

Delivery gates pass on CPython 3.12 Linux x86_64, GCC 13.3.0 and lld 18.1.3.
The wheel is built from the newly extracted self-contained sdist against a fresh
sealed P8 installation. All 64 bindings and the public conversion schema are
sealed, with repeat generation and five generator tests passing. Actual CMake
build/install reject generated C++ mutation and pass after exact restoration.

Installed isolation hides both checkouts, the frozen schema, installed SDK,
SDK build, source staging, extracted sdist and Python builds. Only installed
packages provide codecs. RRC 12, NGAP 12, F1AP 9 and E1AP 10 tests pass; all
24 import orders preserve separate catalogs and exceptions. RRC verifies all
143 P7 cases in three completion modes, 19 DEFAULT omissions and 998 malformed
or quota checks, plus ownership, concurrency, conversion and native boundary
cases for optional false, future ENUM and unknown CHOICE. The three supplemental
cases test Python integration and do not extend the immutable P7 wire profile.
libuper 3, libaper 10, libngap 3, libasn1typed 46 and libasn1parser 1 pass.

Accepted APER wheels and all baseline source blobs remain unchanged, except
Makefile.am distribution entries. The native extension embeds the sealed SDK
archive, exports no NRForge codec symbols and needs no SDK at runtime. There is
no full RRC claim: SIB1 and the other 20 blocked roots remain unavailable; minimal
finite fixtures do not provide all-field/all-value coverage. Existing extension
retention policy and source revision refer to the inherited P8 producer.
The exact artifacts, identities, hashes and independent review snapshot are in
`tools/rrc-python/verification-summary.json` and `tools/rrc-python/review.md`.
No merge or benchmark is performed.
