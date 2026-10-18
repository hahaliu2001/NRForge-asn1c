# Selected NGAP Transfer tooling

The only maintained delivery is [the unified ASN1 SDK](../unified-sdk/README.md).
Version 0.3.0 selects all 26 explicit CONTAINING roots and the existing 131 outer
messages. prepare.py, make_vectors.py, installed_check.py, python_vectors.py and
resource_guard.py are reused by that producer. build_delivery.py is retired and
fails before executing anything. The old four-root-only wheel project is retired.

The accepted 250-check native campaign remains historical finite evidence at
460c790; see ../m6003-transfer-qualification/RESULTS.md. It does not qualify the
new roots, new installed SDK or new wheel. Native execution, consolidated
independent review and Owner acceptance of the expanded source remain pending.
