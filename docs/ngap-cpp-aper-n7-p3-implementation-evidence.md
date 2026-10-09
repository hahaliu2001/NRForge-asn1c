# N7-P3 — Generated extensible SEQUENCE evidence and N7 closeout

2026-10-09. Base: `d0b9e608f5aacd068ffce2d04f2657ff5f4c5f03`.

The [P3 integration contract](ngap-cpp-aper-n7-p3-generation-contract.md) passed independent design review before implementation. Its objective is generated root-only extensible SEQUENCE decoding with owned unknown preservation, and root-only encoding with sticky refusal of retained extension data.

## Implementation and verification

The opt-in types/mapping/codec family revalidates P1 evidence and generates owned sidecars only for extensible root-only SEQUENCEs. It emits the extension flag before root presence/fields, retains bitmap width and every present addition's index and opaque payload, and refuses any non-default sidecar through the sticky P2 encode rejection primitive. Nested values are moved rather than copied during decoding. A minimal header-only `FieldReader::record_failure` reports helper allocation failures without replacing prior errors or mutating finished/moved-from state.

Sidecar spelling deterministically avoids root member/type collisions, and mapping exposes the exact final member pointer. Independent review identified a runtime namespace collision; new-family preflight now reserves `nrforge::aper` and descendants, with three persistent negative cases. Existing N6 namespace behavior remains unchanged.

| Executed verification | Result |
|---|---|
| Fresh `make -C libasn1typed check` | 15/15 PASS; prior shell logs were removed to force fresh execution |
| `make -C libaper check` | 2/2 PASS |
| Generated C++ and runtime, strict C++20 with NDEBUG, Werror, conversion/sign-conversion/shadow warnings | Compile/run PASS; tests use REQUIRE/abort rather than disabled assertions |
| Renderer strict C11 syntax/warnings | PASS |
| Generated C++ and full runtime ASan/UBSan | PASS with `detect_leaks=0` |
| C driver with Owned IR/core/naming/extraction/renderers instrumented | ASan/UBSan PASS; Parser/Fixer/common archives were uninstrumented |
| LeakSanitizer attempt | Unavailable: `/proc` task/ptrace fatal error; no LSan PASS claimed for these local runs |
| Distribution checks | Eight new test/header/fixture files and six qualification tool/evidence files found byte-identical in typed/runtime/tools distdirs |
| Whitespace | `git diff --check` and new-file checks PASS |

Persistent tests cover real Parser/Fixer extraction followed by Parser destruction, repeat generation of all three outputs, unsupported/stale evidence, namespace/name collisions and generator allocation-failure injection. Generated-code vectors cover root alignment residues 0–7; bitmap widths 3/65/128; opaque payload lengths 1/128/16384/65536; ownership after input destruction; copies/moves; and shared nested bitmap/payload/record budgets with exact and one-short cases. Actual allocation failures cover bitmap/payload/record-container storage. A malformed later open type leaves prior primitive charges committed, rolls back the failing primitive, preserves the first error and publishes no complete object. Tests also exercise sticky retained-data encode refusal, failure-hook lifecycle behavior and a safely constructed valueless CHOICE.

## Existing N6 compatibility

Separate scratch drivers linked the HEAD renderer and current renderer against the same extraction/runtime-independent libraries. All six **old-entrypoint** outputs were byte-identical:

| Fixture | types | mapping | codec |
|---|---:|---:|---:|
| `compound-generation-n6.asn1` | 2931 B | 9869 B | 24541 B |
| `compound-generation-reversed-n6.asn1` | 1305 B | 4399 B | 10781 B |

New-family composite codecs intentionally add move/catch handling; their bytes are not claimed identical to old-family codecs. Existing frozen NGAP schemas were not edited.

## Independent native APER qualification

Reproduction tools and a recorded summary are in `tools/n7-sequence-extension-qualification/`. A separate sender schema declares additions while the receiver remains root-only; native bytes are never normalized or repaired before decoding.

| Native oracle | Sender → receiver attempted | Matches | Disagreements | Generated root encoding → native decode |
|---|---:|---:|---:|---:|
| pycrate 0.7.11 | 328 | 116 | 212 | 252/252 PASS |
| asn1tools 0.167.0 aligned PER | 325 | 323 | 2 | 252/252 PASS |

Across both oracles, **326 distinct receiver cases** agree with at least one untouched native implementation; **252 root-only encoding cases** agree with both. This is qualified-subset evidence, not a blanket zero-difference claim. Cases include 32-bit values, OPTIONAL absent/false/true, nested SEQUENCE/CHOICE/parents, empty roots, naming collisions, sparse bitmaps with trailing absent positions, opaque component bytes, length boundaries and fragmentation.

Disagreements were investigated against [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), particularly 11.9.3.4–11.9.3.8:

- Pycrate inserts an extra octet for some non-octet root endings: `c0000280000100` versus independently counted and asn1tools-confirmed `c00002800100`. The receiver rejects the former rather than treating its zero-length open type as valid.
- Asn1tools omits a remainder determinant for the 16K/32K OCTET STRING sender additions; unchanged pycrate output validates those cases.
- Large bitmap widths 65/255 are not native-oracle-qualified: pycrate uses count-minus-one as INTEGER; asn1tools width 65 omits required APER alignment and widths above 127 are unsupported. P2/P3 independent framing vectors cover the normative length/alignment behavior.

No unresolved production discrepancy was found. No runtime behavior was changed to fit the divergent oracle outputs. Raw diagnostic output was retained in scratch during the run; repository tools regenerate it.

## Independent review and N7 closeout

Independent design, implementation and final tools/evidence reviews: **PASS**, with no remaining blocking findings; Ready to Commit **YES**. The late namespace fix and malformed-second-payload tests were independently checked. A non-blocking review observation requested exact case fingerprints in the native qualification profile rather than aggregate counts alone; the runner's final profile addresses this before upload.

| Frozen task | Result |
|---|---|
| N7-P1, owned structure evidence | Complete, `11961777d080ea571f2192f6b5a59ad662d10378` |
| N7-P2, bounded framing runtime | Complete, `d0b9e608f5aacd068ffce2d04f2657ff5f4c5f03` |
| N7-P3, generated ownership integration | Complete after reviewed upload of this change |

**N7 is complete for its approved initial domain. There is no defined N7-P4/P5.** Declared known additions/groups, opaque re-encoding, collections/object-set/open-type dispatch and complete UEContextReleaseCommand NGAP-PDU qualification remain separate future milestones. This change does not claim whole-message qualification or generic ASN.1 support. No benchmark, full frozen-schema scan or WSLg dependency was introduced.
