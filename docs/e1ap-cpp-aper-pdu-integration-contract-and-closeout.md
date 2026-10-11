# E1-P3 — E1AP owned PDU integration

Status: **Complete / Accepted** after independent review.

Implementation baseline: `b255db5768c90260b6a5ba1bf98fc2e931a43e56`.
Frozen source: NRForge-RAN `d6e514a33ec3695925c24aa292900514b371b074`,
protocol-local TS 37.483 V18.6.0 Release 18 authority. No schema edits.

## Contract

E1AP has three outer CHOICE roots and an extension marker. Owned descriptor
evidence provides the PER indexes, procedure codes, outcome identities and
criticalities. The complete closure is 40 procedures and 72 slots (40 initiating,
20 successful, 12 unsuccessful). The shared generator accepts `--e1ap`, extracts
and reconciles every slot, deletes the parser before deterministic generation,
and emits `e1ap.hpp`, separate message BODY types, adapters and registry.cpp.
Its CMake target `nrforge_e1ap` builds all adapters and the shared runtime with
C++20. Installation and SDK packaging are E1-P5.

`nrforge::e1ap::make_e1ap_pdu`, `encode_e1ap_pdu`, `decode_e1ap_pdu` instantiate
the existing transaction and owned dispatch implementation. E1AP PDU, Role,
registry and type-erased BODY types are distinct from NGAP/F1AP and can coexist.
The E1AP framing validator rejects four-root/nonextensible shapes; NGAP shares
the same mathematical three-root framing, so framing alone does not identify
a protocol. Concrete types and separate registry bindings enforce API identity.

Typed BODY construction derives immutable code/role from the complete registry;
received criticality is retained and a checked setter changes criticality only.
Copies own their BODY/payload, moves invalidate the source, immutable registry
metadata survives the creator and independent bindings reject each other's PDUs.
Known malformed BODY values remain errors. Unknown procedures/absent slots in
an extensible object set and unknown outer extensions become owned receive-only
values; encode rejects them. Closed procedure sets reject unknown codes.
Complete operations retain runtime padding/trailing/error-offset and resource
budget semantics. Mandatory-IE application policy is outside this layer.

## Acceptance

Acceptance requires full72 BODY/envelope strict readiness, complete registry
linking and finite all-slot public API checks, focused ownership/identity/unknown
framing/malformed/allocation/resource tests, preservation of prior protocol
generator outputs, and independent review. The final complete registry builds with 75 strict C++20 translation units
(72 adapters, registry, PDU wrapper, runtime). All72 public consumers compile,
link and pass typed identity/roundtrip/criticality/truncation/trailing checks.
The three protocol registry test programs and both tools tests pass. Repeat
generation matches all363 non-manifest files; duplicate/missing/extra/wrong
profile guards refuse success. Prior E1-P2 full72/six-gate readiness is reused
with every retained input and frozen source fingerprint unchanged. Full
NGAP131/F1AP158 regeneration matches 658/793 baseline non-manifest files
byte-exact, including logical manifest equality after source-path removal.
Receipts: `dispatch-integration-e1-p3.json`, `generator-regression-e1-p3.json`,
`verification-e1-p3.json`, and `review-e1-p3.md`, in `tools/e1ap-readiness/`. The finite values are 71 empty-container BODY values and one PrivateMessage
with a vendor-opaque local:0/raw00 entry (private container SIZE starts at1).
They establish dispatch integration only;
E1-P4 supplies independent bounded complete-PDU wire qualification. No populated
payload exhaustiveness, live interoperability, SDK delivery or benchmark claim.

## Reproduction and baseline provenance

See `tools/e1ap-readiness/README.md` for the generation, strict CMake build,
all-slot consumer and registry-suite commands. `record_p3.py` checks repeat
generation, missing/extra/duplicate/wrong-profile guards and unchanged prior
readiness inputs; it does not relabel E1-P2's scan as a new run.

The regression controller source was extracted with
`git show b255db5768c90260b6a5ba1bf98fc2e931a43e56:tools/ngap-dispatch/generate.c`.
Its Git blob is `814e3009907b9805d7e50f58127629e624a58580`; SHA-256 is
`dd06ec9801739a55f2a76a1e34c904c35a2e12d4260f288e3dfba57ab776045e`.
It was compiled using the same unchanged libraries as the current controller:

```sh
gcc -DHAVE_CONFIG_H -I. -Itools -Ilibasn1common -Ilibasn1parser \
  -Ilibasn1fix -Ilibasn1typed /path/baseline-generate.c tools/developer_tree.o \
  libasn1typed/.libs/libasn1typed_extract.a libasn1typed/.libs/libasn1typed.a \
  libasn1fix/.libs/libasn1fix.a libasn1parser/.libs/libasn1parser.a \
  libasn1common/.libs/libasn1common.a -o /path/baseline-generate
```

The verified baseline executable SHA-256 is
`31aae68c3969c78a4025c88c131872e4102c4e5a7e1c1db6ee31f932b08aa848`.
It is recorded with current controller/source hashes in the generator receipt.
