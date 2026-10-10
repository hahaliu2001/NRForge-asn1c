# Unified public NGAP registry qualification

This campaign replays the 3432 independently accepted native semantic cases for all 131 message identities through one actual linked generated registry and the public `nrforge::ngap` API. It does not alter the historical `full-pdu-qualification/accepted-profile.json`.

The generator must first produce a complete directory containing `manifest.json`, `registry.cpp`, public `messages/<Name>.hpp`, the BODY types/mapping/codec headers, and `adapters/<Name>.cpp`. The manifest must identify each message's actual generated header paths, C++ BODY type, role, procedure code and declared criticality.

```sh
PYTHONPATH=/path/to/pinned-qualification-deps python3 tools/unified-ngap-qualification/qualify.py \
  --repo . --asn1-root /path/to/frozen-source \
  --generated-root /path/to/generated-unified-ngap \
  --work /path/to/new-empty-work-directory \
  --output /path/to/unified-candidate-report.json --jobs 1
```

The reference is pinned to pycrate 0.7.11. The six frozen module blobs are verified before and after execution. The manifest must prove parse/fix success, Parser destruction before generation, global/per-message determinism, and complete registry closure. Native descriptors independently verify all 81 procedures, declared criticalities, role presence slots, canonical root PER order and object-set extensibility against both manifest schema and the actual compiled registry declarations. Manifest paths must remain inside the snapshotted generated directory. The native stage constructs semantic values independently, including real encoded OCTET STRING CONTAINING transfer values; generated storage retains those transfer octets without claiming embedded-protocol qualification.

Each identity compiles one test TU that includes its production adapter exactly once. This avoids compiling its large BODY graph twice. A lightweight main links all 131 adapter/test objects, the actual generated registry, the public API implementation and the APER runtime into one executable. Registry metadata is checked against the independently accepted role/code/message/criticality evidence. Assertions remain active with `NDEBUG` through explicit abort-based checks.

For every case, the C++ bridge independently constructs a fresh typed BODY from native semantic values, makes a public PDU, decodes the independently native-produced full PDU, checks typed identity and every selected BODY field, presence, list count, enum semantic value, CHOICE wrapper, IOC payload, bit width and contents, and then encodes the freshly constructed PDU. It compares full size and every octet, and returns its actual bytes to Python. Python independently parses the root framing and explicitly native-decodes those returned bytes to compare the original semantic tree. Native wire and semantic hashes must match every historical accepted case exactly.

One baseline per message also tests root mutations, truncation/padding/trailing diagnostics, budgets, ownership after input mutation and deep copy/move. Global unknown-root policy and registry misuse are tested independently in the public API suite, not silently added to these finite native case counts.

The report fingerprints production and qualification sources, every supplied generated file, each emitted test TU, the main TU, the executable and its actual output. Changed inputs abort publication; a failed rerun removes a stale target report. A passing run publishes a candidate requiring independent review. It is finite accepted-value interoperability evidence, not exhaustive value-space, live vendor, benchmark, conditional application-rule or NAS/RRC qualification.

After the campaign, replay the complete-registry receive-only policy against the same production objects:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic-errors -Wconversion -Wsign-conversion -DNDEBUG \
  -Ilibaper -Ilibngap -I/path/to/generated-unified-ngap \
  tools/ngap-dispatch/check_public.cpp \
  /path/to/work/runtime.o /path/to/work/pdu.o /path/to/work/registry.o \
  /path/to/work/[0-9][0-9][0-9].o -o /path/to/public-policy-check
/path/to/public-policy-check
```

This checks all three unknown-code root roles, all 112 absent outcome slots of known procedures, unknown outer extension ownership, criticality/header preservation, retained-payload budget refusal and receive-only encode refusal. It is additional API-policy evidence, not additional native semantic profile cases. `make -C libngap check` separately covers reversed role ordering, incomplete registry rejection, model/value identity mismatch, copy/move lifetime and actual allocation failures.
