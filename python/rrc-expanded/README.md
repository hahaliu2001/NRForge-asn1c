# RRC Python SDK 0.2.0

Distribution `nrforge-rrc`, import `nrforge_rrc`. This finite profile covers the
85 P17 supported roots (80 selected roots and five DEFAULT seeds);
all 12 selected channels are supported. The initial
qualified platform is CPython 3.12 Linux x86-64. Full RRC coverage remains false.

```python
import nrforge_rrc as rrc
module, root = "NR-RRC-Definitions", "MIB"
decoded = rrc.decode(module, root, bytes.fromhex("020008"))
assert decoded["semantic_bits"] == 23
encoded = rrc.encode(module, root, decoded["value"])
assert encoded["octets"] == bytes.fromhex("020008")
print(rrc.identity(), rrc.schema(module, root))
```

Encode returns octets, semantic/encoded bits, padding and empty substitution.
Decode accepts `logical_bits`; both APIs accept `completion="octets"`,
`"contained_bits"` or `"exact"`, independent `limits` and `conversion_limits`.
The latter controls max_depth/max_nodes/max_bytes/max_collection_elements.
CodecError preserves C++ code and bit_offset. Complete values are owned.

SEQUENCE dictionaries use original field names, CHOICE uses
`{"type": alternative, "value": value}`, known ENUMERATED uses source labels,
SEQUENCE OF uses lists, OCTET STRING uses bytes, BIT STRING uses
`{"octets": bytes, "bit_count": n}`. Optional omission/None is absence; DEFAULT
omission uses the effective C++ default. `schema` describes exact finite shapes.
The current profile has no optional NULL member; future optional NULL models are
refused pending separate presence qualification.
Primitive wrappers are flattened. False is distinct from absence; bool is rejected
as integer. Conversion errors precede narrowing; ASN.1 limits stay with the codec.

Build against a sealed installed RRC C++ SDK, using pybind11 3.0.1 and
scikit-build-core 0.11.6:

```sh
python3 tools/rrc-python-expanded/prepare.py --output /absolute/new/rrc-python
CC=gcc CXX=g++ PATH=/path/to/lld/bin:$PATH python3 -m build --no-isolation /absolute/new/rrc-python \
  -Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=/absolute/rrc-install \
  -Ccmake.define.CMAKE_MODULE_LINKER_FLAGS="-fuse-ld=lld -Wl,--threads=1"
python3 -m pip install /absolute/new/rrc-python/dist/nrforge_rrc-*.whl
```

Source distribution builds need Python/C++20/CMake >=3.20 and the installed C++
SDK. Installed wheel consumers need only Python; no SDK, schema, oracle or generator
is loaded at runtime. Every build/install checks SDK and generated binding receipts.
Each extension has hidden archive symbols for coexistence with all three APER SDKs.
The finite P16/reference limitations remain. No benchmark, stable ABI, other-platform
or free-threaded/subinterpreter qualification is claimed.
