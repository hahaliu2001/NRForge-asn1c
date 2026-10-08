# NGAP Full-Schema Typed IR Extraction Closeout

## Scope

This closeout records message-level Typed IR extraction qualification for
the complete NGAP message inventory against the frozen 3GPP TS 38.413
V18.10.0 (2026-06), Release 18 ASN.1 baseline. It records full-schema
extraction coverage; it does not select NRForge-RAN V1/Tier-A messages.

## Protocol Source

- **Specification:** 3GPP TS 38.413 V18.10.0 (2026-06), Release 18
- **ASN.1 source:** `../NRForge-RAN/src/ng/asn1/`
- **Source baseline commit:** `d6e514a33ec3695925c24aa292900514b371b074`
- **Module list:** [`tools/qualification/ngap-rel18.modules`](../tools/qualification/ngap-rel18.modules)
- **Probe root module:** `NGAP-PDU-Contents`
- **ASN.1 top-level type:** `NGAP-PDU`, defined in
  `NGAP-PDU-Descriptions`
- **Modules, in module-list order:**
  1. `NGAP-CommonDataTypes.asn`
  2. `NGAP-Constants.asn`
  3. `NGAP-IEs.asn`
  4. `NGAP-Containers.asn`
  5. `NGAP-PDU-Contents.asn`
  6. `NGAP-PDU-Descriptions.asn`

## Qualification Summary

The Golden and Batch 1–8 qualification message lists and accepted review
reports were supplied through the project review workflow. Those inputs
report PASS for the initial qualification and independent reviews for each
batch; they are not committed as per-run records in the inspected
repository.

| Set | Messages | Result |
|---|---:|---|
| Golden | 1 | PASS |
| Batch 1 | 8 | PASS |
| Batch 2 | 5 | PASS |
| Batch 3 | 7 | PASS |
| Batch 4 | 16 | PASS |
| Batch 5 | 20 | PASS |
| Batch 6 | 20 | PASS |
| Batch 7 | 29 | PASS |
| Batch 8 | 25 | PASS |
| **Total** | **131** | **PASS** |

## Coverage Reconciliation

The independent inventory from ASN.1 elementary procedure definitions
contains **81 elementary procedures** and **131 explicit, unique messages**.
Against Golden and Batches 1–8, reconciliation found **131 qualification
entries**, **131 unique names**, **0 missing messages**, **0 extra
messages**, and **0 duplicate messages**. No procedure or outcome
inconsistencies were observed.

This reconciliation checks message-set completeness and the mappings stated
by the ASN.1 definitions. It does not verify historical probe execution or
replace the accepted qualification and review results.

## Semantic Capabilities

Non-exhaustive summary of generic Typed IR capabilities exercised or added
during NGAP development:

- Parameterized information-object-class and object-set binding.
- Owned Typed IR references and bound instances.
- INTEGER value ranges and SIZE constraints, including inline ownership.
- Extensibility metadata and extensible empty IOC support.
- CHOICE alternative inline INTEGER ranges.

These capabilities are recorded as development and qualification scope, not
as proof of exhaustive semantic correctness. The semantic capability matrix
does not yet have dedicated entries for extensible empty IOC sets or CHOICE
alternative inline INTEGER ranges. Its CHOICE inline-constraint entry covers
SIZE, and its generic rule requires complete representation of accepted
constraints.

## Evidence and Limitations

- Qualification message lists and accepted review reports for Golden and
  Batches 1–8 were supplied through the project review workflow; they are not
  committed as per-run records in the inspected repository.
- Message-set reconciliation was read-only and did not rerun probes.
- Aggregate owned-type and bound-instance counts are qualification evidence,
  not a complete field-by-field semantic proof.

## Explicitly Out of Scope

- Full-schema field-by-field semantic audit.
- Complete C++ or Python generation.
- Generated-code compilation across the full schema.
- APER encode/decode and interoperability qualification.
- NRForge-RAN message selection and runtime behavior.
- F1AP, E1AP, and RRC qualification.

## Final Status

**NGAP Full-Schema Typed IR Extraction: Complete / Accepted**

This status means **message-level extraction coverage**. It does not mean
complete NGAP Codec support.
