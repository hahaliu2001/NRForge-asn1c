# N11 complete target envelope qualification

Status: PASS within the bounded profile; N11-P1/P2/P3 complete. Closeout date: 2026-10-10 (America/New_York).

The generated C++20 codec now handles the complete initiating UEContextReleaseCommand NGAP-PDU, not only its body. The owned descriptor supplies procedure code, root tag/PER order, received criticality and payload association. The authoritative six-module extraction identifies procedure 41 and retains the extensible 81-row procedure table. Other procedures, outcomes and unknown outer extensions remain owned receive-only opaque data; malformed known target payloads fail instead of falling back.

## Evidence

The fresh guarded run passed the independently approved exact profile: 2,652 checks, zero unresolved production disagreements. It verified all six frozen module identities, regenerated the six headers deterministically, checked unchanged N10 BODY hashes, strictly compiled and self-tested the driver, and compared every case signature with the checked-in profile. There is no byte normalization or automatic profile acceptance.

| Relation | Checks |
| --- | ---: |
| Complete generated PDU decode | 1,311 |
| Typed byte equality and native semantic backdecode | 693 |
| Opaque encode refusal | 618 |
| Independent physical-error literals | 28 |
| Independent suffix decode/refusal | 2 |

The exact minimal PDU is `002900100000020072000400010002000f400140` (20 octets). Shared budgets, sticky errors, allocation failure, owned input lifetime, padding, truncation and trailing data are tested. Hand-modified owned metadata variants additionally verify target code 73, reversed tag order and a closed procedure table; these are not authoritative source evidence.

The 1,125 target-body cases distinguish 1,053 exact native full-PDU selfdecodes from 72 inherited fragmented-child native decoder exceptions. The latter have independent outer framing, unchanged BODY byte equality and standalone child evidence. Another 72 intentionally raw known-other payloads provide framing-only evidence, not native typed agreement. Twelve unsupported but native-schema-decodable examples remain opaque. Native SEQUENCE suffix API anomalies and independent receiver literals are recorded separately.

## Verification and review

- Independent P1 and P2 reviews passed; the P3 profile was independently approved before the fresh exact-profile rerun.
- Typed IR regression suite: 21/21; unchanged runtime suite: 4/4; developer tools: 1/1. The final focused generated test also passed after the diagnostic regression update.
- Strict generated C++20 compilation and runtime self-tests passed, including the acronym naming and alternate metadata variants.
- Focused P1/P2 C generator and P3 generated codec ASan/UBSan checks passed with leak detection disabled. The C checks linked existing parser/fixer libraries; they do not claim complete instrumentation of those libraries.
- LeakSanitizer was attempted but failed under the execution environment’s process/ptrace restrictions; no LSan PASS is claimed.
- Scratch negative checks rejected a changed profile fingerprint and a changed frozen source module. Fresh distribution and whitespace checks cover the delivered files.

Reproduction commands, exact category distinctions and profile hash are in [the qualification README](../tools/n11-envelope-qualification/README.md). Machine evidence is in [qualification-summary.json](../tools/n11-envelope-qualification/qualification-summary.json) and [accepted-profile.json](../tools/n11-envelope-qualification/accepted-profile.json). Earlier contracts and implementation evidence are in [N11 contract](ngap-cpp-aper-n11-envelope-contract.md), [owned evidence](ngap-cpp-aper-n11-envelope-evidence.md) and [generation evidence](ngap-cpp-aper-n11-envelope-generation.md).

This establishes one bounded complete message milestone. It does not establish all NGAP procedures, application mandatory IE policy, vendor interoperability, general PER qualification or performance. Frozen source and runtime contracts are unchanged.
