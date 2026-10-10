# Bounded private key / raw payload native comparison

Generate the fixture with `check_asn1typed_private_render`, compile
`check_asn1typed_private_generated.cpp` with runtime.cpp and the generated
headers, then run `python3 qualify.py BINARY --output results.json`.
The pinned dependencies are in requirements.txt.

240 combinations cover 4 local numeric keys, 4 global OBJECT IDENTIFIER keys,
3 criticalities and 10 nonempty payload lengths through 16383. The generated
BODY encoder bytes match native APER and its decoder verifies the exact keyed
raw contents. This model represents an unknown open payload by OCTET STRING
with those complete encoded octets. It does not qualify vendor payload semantics
or a complete NGAP-PDU. The generated fixture is renamed from the frozen schema.

The native asn1tools 0.167.0 OID decoder mishandles the combined first
subidentifier when it exceeds 119 (2.999.3 becomes 26.39.3). Its encoding agrees
with X.690 and with pycrate. Therefore 120 global-key results decode the actual
APER OID bytes independently using pycrate 0.7.11 and check exact arc tuples,
including a second arc larger than uint64. The report records this limitation;
it does not silently accept the incorrect native semantic result.
