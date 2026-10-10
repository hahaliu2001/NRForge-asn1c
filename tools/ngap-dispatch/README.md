# Unified typed NGAP-PDU generation

Build `tools/asn1typed_ngap_dispatch` after configuring the repository and building its existing parser/fixer/typed dependencies. Run:

```sh
mkdir generated-ngap
./tools/asn1typed_ngap_dispatch modules.txt /path/to/frozen-ngap messages.txt generated-ngap
cmake -S generated-ngap -B generated-build -DNRFORGE_SOURCE_ROOT=/absolute/path/to/NRForge-asn1c
cmake --build generated-build -j1
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

All adapters and `registry.cpp` must be linked exactly once. Mapping/codec headers stay private to their adapter TUs. The CMake target publishes generated/public runtime include directories and C++20 requirements. Link the generated `nrforge_ngap` library into the application. Generated code and runtime must come from the same repository revision.

Decoded PDUs preserve received criticality; `body_if<Body>()` safely checks the concrete type. Root identity is immutable. Unknown root slots and unknown outer extensions are owned receive-only values and refuse encode. Malformed known messages remain errors. This API performs wire dispatch, not NGAP procedure/application validation. See the repository unified dispatch contract and qualification tool for finite interoperability evidence.
