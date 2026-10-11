# Unified typed NGAP-PDU generation

Build `tools/asn1typed_ngap_dispatch` after configuring the repository and building its existing parser/fixer/typed dependencies. Run:

```sh
mkdir generated-ngap
./tools/asn1typed_ngap_dispatch modules.txt /path/to/frozen-ngap messages.txt generated-ngap
python3 tools/ngap-dispatch/seal_sdk.py --repo "$PWD" --generated generated-ngap
cmake -S generated-ngap -B generated-build -DNRFORGE_SOURCE_ROOT=/absolute/path/to/NRForge-asn1c
cmake --build generated-build -j1
cmake --install generated-build --prefix /path/to/new-sdk-install
```

`modules.txt` names the six frozen ASN.1 module files; `messages.txt` lists every declared target BODY identity. The output directory must already exist and be empty. Duplicate, malformed, incomplete, extra or ambiguous identities fail. The generator derives closure from validated owned procedure/object-set evidence and does not assume a fixed message count. It destroys Parser/Fixer trees before rendering and compares repeated generated outputs. A failure can leave partial files; the manifest is published last and success requires exit status zero. Do not reuse a failed directory.

`ngap.hpp` exposes the lightweight unified API. Include a message-specific header for concrete BODY construction/access. The generated manifest maps original identities to stable namespaces and public headers:

```cpp
#include "ngap.hpp"
#include "messages/NgSetupRequest.hpp"

// Use the manifest's actual qualified namespace and its Body alias.
// Populate required IEs before encoding.
using Body = nrforge::ngap::messages::NgSetupRequest::Body;
Body body;
auto made = nrforge::ngap::make_ngap_pdu(std::move(body));
if(made) {
    auto encoded = nrforge::ngap::encode_ngap_pdu(made.value());
    // Check Result before accessing value; invalid fields can reject encode.
}
```

All adapters and `registry.cpp` must be linked exactly once. Mapping/codec headers stay private to their adapter TUs. The CMake target publishes generated/public runtime include directories and C++20 requirements. The seal identifies exact source and generated content; CMake refuses modified inputs and installs a relocatable `NRForge::ngap` target. Applications discover it with `find_package(NRForgeNGAP 0.1.0 EXACT CONFIG REQUIRED)`. Public installed headers enforce their fingerprint-specific library link guard. See `tools/ngap-sdk/README.md` for the complete build and `tools/ngap-sdk-consumer/README.md` for the independently verified installed consumer.

Decoded PDUs preserve received criticality; `body_if<Body>()` safely checks the concrete type. Root identity is immutable. Unknown root slots and unknown outer extensions are owned receive-only values and refuse encode. Malformed known messages remain errors. This API performs wire dispatch, not NGAP procedure/application validation. See the repository unified dispatch contract and qualification tool for finite interoperability evidence.

## F1AP shared dispatch profile (F1-P3)

Use the same controller with a final `--f1ap` argument, the frozen F1AP module
list and all declared F1AP message names. It emits `f1ap.hpp`, separate
`nrforge::f1ap::messages` BODY identities and a complete F1AP registry. The
generated CMake target `nrforge_f1ap` builds a repository-local dispatch library;
it does not install, seal or qualify an F1AP SDK (F1-P5).

```sh
mkdir generated-f1ap
./tools/asn1typed_ngap_dispatch tools/f1ap-readiness/f1ap-rel18.modules /path/to/frozen-f1ap/src messages.txt generated-f1ap --f1ap
cmake -S generated-f1ap -B generated-f1ap-build -DNRFORGE_SOURCE_ROOT=/absolute/path/to/NRForge-asn1c
cmake --build generated-f1ap-build -j1
```

`make_f1ap_pdu`, `encode_f1ap_pdu` and `decode_f1ap_pdu` expose the same owned
dispatch contract under distinct F1AP types. Both protocol headers can coexist;
there is no alias between NGAP and F1AP PDU, role or type-erased BODY identities.
Shared source implements the existing transaction and ownership behavior with
owned-evidence framing: F1AP has four roots and no outer extension bit. Its
`choice-extension` root is unsupported and fails before reading an outcome
header; it is never confused with an NGAP outer extension or opaque procedure.
Unknown root procedures/absent outcome slots remain receive-only opaque values
only when the procedure object set permits them. Known malformed BODY values
do not fall back to opaque data. All-message linkage and focused vectors are
integration gates, not the independent batch wire qualification of F1-P4.

## E1AP shared dispatch profile (E1-P3)

Pass `--e1ap` with the frozen E1AP module list and all72 message names. The
controller emits `e1ap.hpp`, independent `nrforge::e1ap::messages` BODY types,
a complete registry and development CMake target `nrforge_e1ap`. The public
entry points are `make_e1ap_pdu`, `encode_e1ap_pdu`, `decode_e1ap_pdu`. E1AP
uses three outer roots plus an extension marker, as proven by owned schema
evidence. Unknown procedures/absent slots and outer extensions are owned
receive-only values; malformed known messages remain errors. See
`tools/e1ap-readiness/README.md` for build/integration reproduction and
`docs/e1ap-cpp-aper-pdu-integration-contract-and-closeout.md` for scope.
This development build does not deliver an installable E1AP SDK.
