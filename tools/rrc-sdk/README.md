# RRC C++ source SDK 0.1.0

Build/configure the repository and its typed/parser/fixer/common libraries first,
using the frozen authority instructions in `tools/rrc-readiness/README.md`.
A C++20 toolchain, CMake >=3.20 and Python are needed for source delivery. No schema
is vendored. `--asn1-root` contains `rrc/asn1/...`.

```sh
python3 tools/rrc-sdk/build.py --asn1-root /path/to/schema \
  --work /new/rrc-sdk --prefix /new/rrc-install --jobs 2
python3 tools/rrc-sdk-consumer/prepare.py --asn1-root /path/to/schema \
  --generated /new/rrc-sdk/generated --work /new/rrc-consumer-input
python3 tools/rrc-sdk/verify_seal.py --repo "$PWD" \
  --sdk-work /new/rrc-sdk --work /new/rrc-seal-negatives
```

Use pycrate 0.7.11 for consumer preparation, the P7 pinned semantic bridge and the
accepted P7 corpus. Work directories must be new, and install prefixes new/empty.
The generator first runs fresh focused/controller and authenticated matrix checks;
all 64 raw header hashes and the entire supported/blocked P6 ledger must match.
Source and actual linked producer artifact fingerprints are captured before
production and checked after generation and installation. Schema authentication
is repeated. No producer or codec patch is made by the SDK packager.

The SDK installs `libnrforge_rrc.a`, 64 public root headers, runtime/catalog/version
headers, manifest, provenance and relocatable CMake exports. The complete requested
manifest has 85 identities, 64 supported and 21 blocked. Public namespaces preserve
module/root identity with hyphens replaced by underscores. No header is generated
for a blocked root. Separate root includes keep consumer compilation bounded.

```cmake
find_package(NRForgeRRC 0.1.0 EXACT CONFIG REQUIRED)
target_link_libraries(my_consumer PRIVATE NRForge::rrc)
```

```cpp
#include <nrforge/rrc/roots/NR_RRC_Definitions/MIB.hpp>
namespace mib = nrforge::rrc::NR_RRC_Definitions::MIB;
// decoded = mib::decode(bytes); encoded = mib::encode(decoded.value().value);
// Both APIs return Result; check it before accessing value().
// decode accepts Limits, optional logical_bits and Completion.
// CompleteEncoding reports semantic_bits, encoded_bits and terminal padding.
```

The accepted bit-aware runtime remains `nrforge::uper`; root models and SDK identity
use `nrforge::rrc`. Every public include participates in an archive/header fingerprint
link guard. A real executable resolves all 64 objects and both public wrappers
before an archive earns its receipt; install rechecks the actual immutable lock
and receipt. Consistency fingerprints are not signatures or stable binary ABI.

For four-protocol isolation, first build/install the NGAP and F1AP SDKs using their
existing `tools/*-sdk/build.py` instructions, and supply the accepted E1AP C++ SDK.
The large mixed consumer requires LLVM `ld.lld` on `PATH` (see the consumer README).
Then run:

```sh
python3 tools/rrc-sdk-consumer/isolate.py --repo "$PWD" --schema /path/to/schema \
  --sdk-work /new/rrc-sdk --prefix /new/rrc-install \
  --consumer-input /new/rrc-consumer-input \
  --ngap-prefix /path/ngap-install --ngap-work /path/ngap-sdk \
  --f1ap-prefix /path/f1ap-install --f1ap-work /path/f1ap-sdk \
  --e1ap-prefix /path/e1ap-install --e1ap-work /path/e1ap-sdk \
  --work /new/rrc-relocated --jobs 2
```

Isolation must run after builds finish. It temporarily renames the source, schema,
all original SDK build/install paths and prepared consumer inputs, restoring them
in `finally` even on failure. The workspace must be outside these inputs. All 64
root executables (143 real cases) use public APIs, owned value comparisons, exact
bytes/semantic lengths and three completion modes. One further executable includes
and uses all four installed protocol APIs, validates the 131/158/72 APER registries
and an RRC MIB, with a shared translation unit. Compiler dependencies are audited
against installed/system files. Configuration expected-fingerprint and actual
header/archive mismatch tests must reject the paired inputs.

`verification-summary.json` and `review.md` record the accepted campaign. Full RRC
coverage remains false: seven of 12 channels, 59 selected roots, five extra DEFAULT
roots and three bare seeds; SIB1 remains blocked. Finite P7 value and reference
limitations remain. Python SDK is P9. No benchmark or merge.
