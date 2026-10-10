# ASN.1 Typed developer tools

The tools accept an ordered module list. Each nonblank, noncomment line is a
source path; relative paths are resolved against `--asn1-root` when supplied.
Without a root, paths are resolved from the current working directory. No
module discovery is performed.

Exit status is 0 for success, 2 for invalid command line, 3 for module-list or
parse/load failure, 4 for fixer failure, and 5 for extraction or lookup failure.

The optional NGAP Release 18 qualification list is
[`qualification/ngap-rel18.modules`](qualification/ngap-rel18.modules). With
the sibling NRForge-RAN checkout, run from this repository:

```sh
tools/asn1typed_real_probe --asn1-root ../NRForge-RAN/src \
  --module-list tools/qualification/ngap-rel18.modules \
  --root-module NGAP-PDU-Contents --message NGSetupRequest

tools/asn1typed_tree_inspect --asn1-root ../NRForge-RAN/src \
  --module-list tools/qualification/ngap-rel18.modules \
  --type NGAP-IEs.GNB-ID --member gNB-ID
```

The inspector reports fixed-tree evidence. These parser/fixer pointers and
indices are diagnostic views, not durable Typed identities.

`asn1typed_ioc_probe` is the opt-in physical IOC generation readiness probe.
It deletes the Parser tree after extraction, reports owned registry summaries,
then preflights all three output families before writing headers. The default
namespace is `ioc_probe`; `--namespace` selects another validated namespace.
Optional `--verify-determinism` renders each family twice from the same unchanged
owned graph after tree deletion and refuses output if the complete text differs.

```sh
tools/asn1typed_ioc_probe --asn1-root ../NRForge-RAN/src \
  --module-list tools/qualification/ngap-rel18.modules \
  --root-module NGAP-PDU-Contents --message UEContextReleaseCommand \
  --output-prefix /existing/output/directory/command --namespace command_probe
```

The output names are `command_types.hpp`, `command_mapping.hpp` and
`command_codec.hpp`. Include runtime, types, mapping, then codec. Generation
failure exits 6 without creating files; output I/O failure exits 7 and can leave
earlier files. Existing output files are replaced. Success proves generation
readiness only, not message-body or complete NGAP-PDU wire qualification.

The [N10 body qualification runner](n10-body-qualification/README.md) consumes
this probe with `--verify-determinism`, verifies the six frozen source identities,
strict-compiles an ID-selected integration adapter, and compares the actual
UEContextReleaseCommand body with a freshly compiled native reference. Its
scope excludes the NGAP-PDU envelope.
