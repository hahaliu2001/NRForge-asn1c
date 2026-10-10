# NRForge NGAP Python SDK

The `nrforge-ngap` wheel calls the complete C++ SDK. Runtime does not require pycrate, ASN.1 source files, a compiler or the C++ SDK installation. This first release targets ordinary CPython; no free-threaded, subinterpreter, cross-platform or stable-ABI qualification is claimed.

## Build and install

Use a sealed, PIC-capable installed SDK 0.1.0 from `tools/ngap-sdk`.

```sh
python -m pip install build
python -m build --wheel python \
  -Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=/absolute/installed-sdk
python -m pip install python/dist/nrforge_ngap-*.whl
python python/examples/ng_setup.py
```

The build requires pybind11 3.0.1, scikit-build-core 0.11.6, CMake and a C++20 compiler. All 131 public message/type headers are converted at build time. Unrecognized shapes or changed installed headers are refused. The native library is statically linked into the extension. The initial strict compilation gate uses O0, consistent with the installed SDK's qualification.

## Python operations

```python
import nrforge_ngap as ngap
print(ngap.identity())
print(ngap.messages())
pdu = ngap.decode(wire_bytes)
assert pdu['kind'] == 'typed'
wire_again = ngap.encode(pdu['message'], pdu['body'],
                         criticality=pdu['criticality'])
```

`encode(message, body, *, criticality=None, limits=None, conversion_limits=None)` returns complete APER `bytes`. Message names are registry ASN.1 names, such as `NGSetupRequest`. None criticality selects the declared value. `decode` accepts immutable `bytes` and returns a dictionary containing kind, role, procedure_code, received criticality, message and typed body. Unknown root or outer extension results instead retain payload bytes and, for an extension, its index. The C++ preservation contract does not authorize re-encoding unknown outer PDUs; `encode` accepts registered typed messages only.

`CodecError` inherits ValueError and retains `.code` and `.bit_offset` from the C++ runtime. Shape problems use TypeError or ValueError; integers outside C++ storage range raise OverflowError/ValueError before narrowing. Python allocation failures remain MemoryError. No partial object is returned on conversion or codec failure.

## Data model

This initial API uses exact generated C++ field names, available through `schema(message)` or by decoding a fixture. It does not substitute ASN.1 spellings or use variant position as PER index.

- Struct: exact built-in dict with exact str keys, every mandatory member required. Unknown keys and dict subclasses are rejected.
- OPTIONAL: None or omitted for absent; a concrete value for present. False is not absence.
- INTEGER: int, never bool; checked before narrowing. Full ASN.1 constraints remain enforced by the C++ codec.
- BOOLEAN: bool only. Character string: str. OCTET STRING / opaque payload / encoded OID storage: bytes. NULL: None.
- BIT STRING: `{'octets': bytes, 'bit_count': int}`.
- Collection wrapper: `{'elements': [values...]}`; elements must be an exact built-in list, not a subclass.
- ENUMERATED: final generated known-label string, or `{'extension_index': int}` for an unknown extension of an extensible enum.
- CHOICE / IOC open value: `{'type': 'GeneratedWrapperName', 'value': wrapper_dict}`. A wrapper commonly has `{'value': payload}`; an unknown open wrapper has `{'payload': bytes}`. These explicit labels select storage types, not PER ordinal numbers.
- Sequence extensions: `{'received_bitmap_bit_count': int, 'unknown_additions': [{'addition_index': int, 'payload_octets': bytes}]}`. Supply the zero/empty form even when no extension was received. No unknown information is silently discarded; unsupported re-encoding remains a C++ error.

`limits` accepts the nine C++ Limits field names (see installed runtime.hpp). `conversion_limits` is a separate per-call Python/C++ conversion budget: max_depth (256), max_nodes (1,000,000), max_bytes (16 MiB), max_collection_elements (65,536). String/byte values and variant/enum/message labels are charged to max_bytes; fixed dictionary key labels and fixed result metadata are bounded but not charged. Decode input bytes and encode output bytes are included. This budget controls conversion traversal/copies, not memory already owned by the caller, the Python allocator's overhead, reflection schema generation or the independent C++ codec allocations. Labels are limited to 256 characters. Python dictionaries/lists are copied to owned C++ values under the GIL; only pure codec execution releases the GIL.

## Verification

`tests/run_installed.py --python /path/to/venv/bin/python --output result.json` runs an isolated outside-repository consumer. Tests include 131 accepted minimal native vectors, independent NG Setup fields and construction, error offsets/limits, strict types and unknown data preservation. Finite vectors are not a live RAN procedure or every ASN.1 value qualification. No wire logic is implemented in the Python layer.
