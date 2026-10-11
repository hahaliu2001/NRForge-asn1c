# Installed public E1AP consumer

```cmake
find_package(NRForgeE1AP 0.1.0 EXACT CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE NRForge::e1ap)
```

Include `<nrforge/e1ap/e1ap.hpp>` and qualified message headers. Construct a
public message `Body`, pass it to `nrforge::e1ap::make_e1ap_pdu`, encode with
`encode_e1ap_pdu`, decode with `decode_e1ap_pdu`, and access its checked typed
`body_if<Body>()`. The committed `e1_setup.cpp` supplies concrete populated
examples for all three outcomes in both E1 Setup directions.

`prepare.py` independently compiles the exact frozen schema with pycrate 0.7.11
and compares six fixture bytes/semantic hashes with accepted E1-P4 evidence.
Mapping files supply wrapper type identities only. The emitted C++ consumer
uses installed public headers exclusively. `prepare_all.py` produces a separate
72-slot functional consumer. `verify.py` relocates the installation, builds and
executes both consumers, audits dependencies/exports and tests configuration
and direct-public-header fingerprint mismatches. `isolate.py` additionally
hides the original checkout/schema/build/install paths and restores them on
failure. See `../e1ap-sdk/README.md` for reproduction.

All-slot values include 71 empty containers and one opaque PrivateMessage
entry. Those establish construction/dispatch functionality; populated Setup
fixtures establish a finite native relation, not application procedure policy.

Isolation also constructs two real NGAP/F1AP development-core archives from
compiled objects. E1AP public headers must reject those archives at the identity
link guard. Two include/link orders exercise all three public core profiles
with the full installed E1AP registry, while NGAP/F1AP cores use explicit rejected empty-registry fixtures. This detects header/runtime/symbol conflicts;
it does not claim historical complete SDK coinstallation or wire requalification.
