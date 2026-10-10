# Installed NGAP SDK consumer

`ng_setup.cpp` is an outside-checkout consumer example of the three NG Setup
outcomes. It includes only `ngap.hpp` and three public `messages/*.hpp` headers,
constructs each `messages::NgSetup*::Body` afresh, calls `make_ngap_pdu`, compares
complete encoded bytes and length with independent frozen native goldens, then
checks every constructed decoded field, OPTIONAL state and selected wrapper.
Assertions use `REQUIRE` and remain enabled under `NDEBUG`.

These fixtures contain the ASN.1 schema's mandatory IE rows and minimal values.
They establish SDK integration, ownership and exact APER interoperability for
these finite cases. They are not live AMF/gNB procedure or deployment examples.

## Use an installed package

Copy `ng_setup.cpp` and `CMakeLists.txt` into an otherwise independent directory:

```sh
cmake -S consumer -B consumer-build -DCMAKE_PREFIX_PATH=/path/to/installed-sdk
cmake --build consumer-build
ctest --test-dir consumer-build --output-on-failure
```

The target is `NRForge::ngap`, discovered through
`find_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED)`. No source-private runtime,
mapping or codec headers and no qualification adapter objects are required.

## Verification, including relocation

```sh
python3 tools/ngap-sdk-consumer/verify.py \
  --prefix /path/to/installed-sdk \
  --work /path/to/new-consumer-work \
  --output /path/to/consumer-evidence.json
```

The work directory must not exist. The verifier copies the installation to a
new prefix, copies only the consumer sources, configures with package registries
disabled and a clean compiler-search environment, builds and runs all three
outcomes, and audits actual compiler dependency files. It refuses references to
the original checkout/prefix, mapping or codec headers. A mismatched expected SDK
fingerprint must fail at configure time. The SDK's header fingerprint link guard
also rejects a mismatched header/library pair.

## Reproduce the example's independent goldens

Preparation alone uses qualification tools and generated mapping metadata to
translate native semantics into spelling-safe typed construction. Consumer
compilation does not use those tools or headers.

```sh
PYTHONPATH=/path/to/pinned-pycrate-0.7.11 python3 \
  tools/ngap-sdk-consumer/prepare.py \
  --generated-root /path/to/complete-generated-sdk \
  --asn1-root /path/to/frozen-ngap \
  --work /path/to/new-native-work
```

The exact six TS 38.413 V18.10.0 modules are hash-checked against the repository
source manifest. A fresh pycrate 0.7.11 compiler/oracle encodes and decodes every
fixture. Complete wire and semantic SHA-256 hashes must match the historical
accepted full-PDU profile before `ng_setup.cpp` or `native-fixtures.json` is
written. The frozen JSON is evidence rather than input to production codecs.
