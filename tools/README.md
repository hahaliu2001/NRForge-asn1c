# ASN.1 Typed developer tools

Both tools accept an ordered module list. Each nonblank, noncomment line is a
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
