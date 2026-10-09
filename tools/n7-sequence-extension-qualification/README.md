# N7-P3 reproducible native APER cross-check

Sources: driver.cpp and qualify.py. Install pinned dependencies from requirements.txt into an external Python environment. Receiver headers come from real Parser/Fixer/Owned IR generation of untouched sequence-extension-generation-n7.asn1, namespace extensiontest. Separate sender schema is sequence-extension-sender-n7.asn1 and contains declared additions. No oracle bytes were normalized or modified. The runner compares every matched case ID and every compact mismatch signature against accepted-profile.json; equal aggregate counts alone cannot pass. Changed profiles fail and write observations for review. The zero-unresolved result is published only after this exact comparison.

## Run

Use the configured/built repo or a VPATH build containing the focused render driver. The runner does not rebuild the repository and keeps generated oracle code, receiver headers and binaries in a temporary directory.

```sh
python -m pip install -r tools/n7-sequence-extension-qualification/requirements.txt
python tools/n7-sequence-extension-qualification/qualify.py --repo /absolute/source/repo --build /absolute/build --output /outside/repo/n7-native-result.json
```

Alternatively pass `--generated /absolute/header/directory` containing types.hpp, mapping.hpp and codec.hpp generated with namespace extensiontest. `--cxx 'clang++'` selects another compiler. The JSON records compact mismatch metadata (case, type, length, SHA256, reported error), not raw large vectors. The checked-in summary is the observed qualification profile. Any changed profile fails the run and requires investigation; expected oracle failures are never silently counted as matches.


- 328 attempted pycrate 0.7.11 native sender decode cases: 116 agree, 212 disagreements recorded as compact metadata in the output JSON. Unmodified generated root-only encode → pycrate sender decode: 252/252 agree.
- 325 attempted asn1tools 0.167.0 aligned PER sender decode cases: 323 agree, 2 disagreements recorded as compact metadata in the output JSON. Unmodified generated root-only encode → asn1tools sender decode: 252/252 agree.
- Combined native-oracle coverage: 326 distinct receiver cases agree with at least one untouched native oracle. 252 root-only encoding cases agree with both oracles. This is qualified-subset evidence, **not** an all-case zero-difference claim.

Cases cover 32-bit INTEGER values, optional absent/false/true, nested Inner/Outer/CHOICE/non-extensible parents, empty extensible roots, sidecar-name collisions, sparse three-position addition bitmap including absent trailing positions, exact opaque BOOLEAN/INTEGER/OCTET STRING payload bytes, 127/128-octet length boundary and 16K/32K sender content with open-type fragmentation. Payload comparison uses native standalone component APER bytes (without outer open framing).

Oracle disagreement diagnostics:

1. Pycrate root-end non-octet alignment: sender Inner{value=0,flag=false,extraFlag=false} produces c0000280000100. Independently counted root-end17 + normally-small7 + bitmap3 requires5 alignment bits before length1, giving c00002800100. Asn1tools emits the latter; receiver accepts it. Pycrate's pre-generation of extension payload changes its shared bit offset before writing the bitmap and inserts an extra zero octet. Generated receiver rejects the zero total open length as constraint_violation@27.
2. Asn1tools fragmented open payload: extraBytes contains16384 octets. Inner wire is16392 octets; pycrate wire is16393. Asn1tools omits the remainder determinant before the final two payload bytes: tail ...feff00 vs normative ...fe02ff00. Receiver rejects the former and accepts unmodified pycrate output. The32768-content case has the same defect.
3. Long bitmap native limitations: pycrate width65/255 encodes count-minus-one as an unconstrained INTEGER; asn1tools width65 emits the flag plus length without required octet alignment and does not support widths>127. ITU X.691(02/2021), clauses11.9.3.4–11.9.3.6 confirm that n>64 uses n as an unconstrained length, octet aligned for APER. Existing P2 framing agrees with these clauses. Width64 native case agrees; widths65/255 remain normative/manual-test qualified, not native-oracle qualified.

No whole NGAP qualification, benchmark or opaque re-encoding claim. The runtime/producer were not modified to fit invalid oracle outputs.

Normative source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), clauses 11.9.3.4–11.9.3.8.4.
