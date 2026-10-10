# F1-P1: frozen F1AP batch readiness

This read-only scan reuses the NGAP physical-message extractor and IOC/body renderers. It does not change codec semantics or qualify F1AP wire interoperability.

The profile fixes TS 38.473 V18.10.0 (Release 18) to NRForge-RAN commit `d6e514a33ec3695925c24aa292900514b371b074`. Supply that repository's `src` directory as the source root. The six modules are checked by ordered paths, byte lengths, Git blob SHA-1 and SHA-256 before and after execution. Schemas are external inputs, not vendored or modified by this task.

From the asn1c repository root:

```sh
make -C tools asn1typed_codec_coverage
python tools/f1ap-readiness/test_scan.py --asn1-root /path/to/NRForge-RAN/src
python tools/f1ap-readiness/scan.py \
  --asn1-root /path/to/NRForge-RAN/src \
  --probe tools/asn1typed_codec_coverage \
  --work /tmp/new-f1ap-scan-directory \
  --output /tmp/f1ap-readiness.json
```

The work directory must not already exist. A working compiler is required (`--cxx` selects it). Compile commands use C++20 with strict warnings, syntax-only, and no runtime execution. The probe builds one combined Parser/Fixer tree, extracts every declared procedure outcome and then destroys the tree before generating owned types, mappings and body codecs. The scanner independently reconciles procedure root-set membership, message declarations, procedure codes and role counts (94 procedures; 158 messages: 94 initiating, 36 successful and 28 unsuccessful).

The shared probe retains its original four/five-argument NGAP interface. F1AP uses the explicit eight-argument interface:

```text
probe MODULE_LIST ASN1_ROOT MESSAGE_LIST HEADER_DIR BODY_MODULE DESCRIPTION_MODULE PDU_TYPE
```

`readiness.json` records all 158 message results, exact first-failure clusters, source identities, compiler version/options and scan-input hashes. Large raw owned inventories, generated headers and compiler logs are in the work directory and can be reproduced from those inputs. Hashes identify the particular probe executable; rebuilding it may change that hash without changing semantics.

Interpret each gate separately: parse/fix; physical extraction; three body-generation families; strict body compilation; envelope descriptor extraction. A successful envelope extraction does not imply that complete-PDU generation was attempted. Compilation does not test linkage, bytes, required IE policy, extension handling, unknown IEs, or interoperability. An earlier failure masks later dependencies. Cluster sizes describe observed affected messages, not predicted unlock counts. Ordinary type inventories exclude bound-instance internal bodies. No benchmark or external codec qualification is run.
