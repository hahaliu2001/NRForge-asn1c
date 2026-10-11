# RRC-P12: constrained primitive collection elements

Baseline: `82ff13ff3172640887c6c847a258e1b8a2a07d13` on
`feature/f1ap-cpp-aper`. Independent model review confirms that existing owned
primitive types referenced by anonymous `@element` paths retain element bounds
without new public IR fields. List SIZE is independent of element constraints.

After configuring/building native libraries and extraction test programs:

```sh
make -C libasn1typed check_asn1typed_extract check_asn1typed_collection_elements
python tools/rrc-collection-elements/check.py \
  --asn1-root /path/to/frozen/NRForge-RAN \
  --work /path/to/new/p12-check --output /path/to/readiness.json
```

The work directory must not exist. The driver captures current producer
sources, archives and test executables; authenticates the six frozen RRC
modules; rebuilds probes; generates eleven synthetic element closures (ten
render PASS, one INTEGER-union renderer limitation) and rejects six unsupported
roots. It runs extraction and strict C++20 codec tests, instruments the changed
extractor/test sources with ASAN/UBSAN, refreshes all 85 selected RRC identities,
compares all 65 P11-supported headers, and syntax-compiles the two newly
rendered roots. LSan is explicitly disabled because the current environment
cannot enumerate `/proc` tasks; this receipt does not claim leak-scan success.

The same run rebuilds the shared APER coverage probe and invokes the historical
comparison helper for all 361 messages and 1,773 recorded headers. The current
P12 invocation is labeled separately from that helper's original P4 batch.
Source and archive guards plus frozen RRC source authentication are checked
again after execution. `verification.json` binds the candidate and native
regression logs; `review.md` records independent acceptance.

Measured readiness: **67 render PASS / 8 extraction blocked / 10 render blocked**.
The ten render blockers include five roots newly advanced past extraction;
no previously supported header changes. `SidelinkUEInformationNR-r16` and
`UEAssistanceInformation` newly render and syntax-compile, but their real-message
wire qualification is deferred. Existing sealed C++/Python SDK delivery remains
**64 roots**. There is no SDK rebuild, schema update or benchmark in P12.

Focused nested coverage uses a constructed element wrapper; the parser's
direct nested SEQUENCE OF constraint assertion is not repaired in this batch.
Constrained named uses, named bits and other primitive element families retain
their existing rejection boundary. Extraction ownership does not extend the
renderer profile for INTEGER unions or other previously unsupported domains.
