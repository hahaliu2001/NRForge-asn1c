# F1-P4 independent acceptance review

Verdict: **PASS — approve the exact finite candidate for promotion**.
Reviewer: independent agent `p4_independent_review`; date: 2026-10-10.
Baseline: `3c78d511561af8a846ca67e29b75dd88fbdbedde`.

The approved candidate is exactly 15,760,703 bytes, SHA-256
`b957abe4cbd0c80bf109dc77dbb5e2f37257959ac6b65040003e2d875d615f64`.
Approval applies only to that candidate and its recorded finite values. The
reviewer did not edit production codecs, frozen schema, candidate, stage,
commit or push. Promotion must preserve the candidate byte-for-byte as the
accepted logical payload; its internal candidate status stays historical. An
external summary records this approval.

### Lossless storage-format review

The GitHub connection's request limit prevents transporting the original
pretty-printed candidate once JSON-escaped. The committed
`accepted-profile.json` is therefore a 1,167,860-byte gzip/base64 JSON envelope,
not a reserialized or reduced qualification profile. The reviewer independently
decoded base64 with strict validation, decompressed gzip, compared every decoded
byte to the original candidate, and verified the original 15,760,703-byte size
and approved SHA-256 above. All 57 captured source fingerprints still match.
Storage-envelope SHA-256 is
`621e7814a91d747a751acd8618a21318ced6cc700ca193148efe0ea2e9173062`.

The README restores and verifies the exact original before `--accepted-profile`;
the unchanged runner consumes that expanded original, not the envelope. The
external summary distinguishes storage and logical hashes. **PASS** for this
lossless packaging only: no case, byte, semantic, trace or source fingerprint
was changed, and the qualification verdict is unchanged.

## Independent execution and evidence reconciliation

The reviewer independently reran the actual complete-registry executable,
not just its generator or a same-codec roundtrip. Return code was zero; its
559,000-byte output exactly matched the original response hash. Every one of
4,728 distinct responses was checked against the recorded complete native
vector, BODY bytes, full semantic value and source identity. Each response was
also natively backdecoded with the freshly compiled frozen-source oracle and
compared to the original semantics. No exception or byte mismatch was ignored.
The empty private object set emits native unknown-table diagnostics, retained
as warnings rather than silently transformed into known vendor semantics.

| Independently verified relation | Result |
|---|---:|
| Native / frozen text / actual registry closure | 158 outcomes, 94 procedures |
| Initiating / successful / unsuccessful outcomes | 94 / 36 / 28 |
| Complete generated/native byte and semantic cases | 4,728 PASS |
| Selected top-level declared IE row occurrences | 975 / 975; 0 missing |
| SRBID extension-union values / cases | 4 and 5 / 330 |
| Distinct selected CHOICE labels | 113 |
| Main physical, ownership and budget checks | 8,781 PASS |
| Actual unknown codes / absent outcome slots | 486 / 124 PASS |
| Finite populated complete-vector sizes | 13–330 octets |

All 57 source fingerprints and 794 generated-file fingerprints matched the
current inputs. All 158 test TU/object hashes matched; every object had ELF
magic and its strict compile log was empty. The archive, original P3 linked
integration receipt, main TU, oracle, executable and actual response hashes
were independently verified. Frozen F1AP module size, SHA-256 and git-blob
identities remained exact. There was no production compiler/runtime/registry
fix; historical readiness evidence was not relabelled as a new scan.

For each identity, the independently derived error matrix was reconciled to
the actual reported physical count: all prefix truncations, F1 alignment bits
2–7 and 18–23, fourth selector, invalid criticality, open determinants, empty
open and trailing data, plus eight ownership/resource/malformed-known checks.
The sum is exactly 8,781. Fourth-root rejection is at bit 2, before any ordinary
procedure header; it is not an NGAP extension-bit interpretation. Unknown and
absent slots remain owned receive-only opaque values and refuse encode.

| Exact evidence | SHA-256 |
|---|---|
| Main executable | `db4ea4aba13855dfd3b14bda12a6dbe93d1a6ff831a87a854eeb8ea8a46e0f93` |
| Independently reproduced actual stdout | `b4fe3e85d16916f649cb0213aca598d7def5ab5d0ec3055d3c95d5428465f997` |
| Actual stderr (empty) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| Frozen-source native oracle | `ac1b32f792b8655528b5b4d297768791374305f76c1e899c423db0eff28ba91c` |
| Full production archive | `9adc4035e09161473ea504f5b59b649220401f5b9536b4a21b416bffd7f2a4f9` |
| P3 linked archive receipt | `2c0a024ed2d63e62821b94af5d5ba7a1addcf90050bf6995c50626e7d711d8d5` |
| NGAP regression record | `e97e3cab4f2f24df2ba31181c357c0b1beae52f0b28abc5bbbd2104ce6480b5a` |

The runner's full-wire/semantic bridge is fail-closed: it constructs expected
BODY values from native semantic leaves, asserts typed kind and concrete BODY
identity before checking decoded fields, and verifies entire encoded vectors.
All native profile failures remain explicit failures. Archive attestation
requires an exact match for every relevant generated/public/runtime input.
Accepted replay excludes only object hashes from the identity comparison;
complete bytes, semantic/BODY hashes, selected traces, test TU hashes and
coverage remain exact. Object/executable hashes remain execution provenance.

## NGAP preservation

The reviewer verified all 55 fresh functional test return codes, test-input
and log hashes (including recovered test-only helper binaries), with no
failure or skip. All sealed current production/source and six frozen NGAP
schema fingerprints matched. Fourteen raw inventories, 42 BODY headers and
659 dispatch files were independently compared byte-for-byte to the P3
baseline. The 656 outputs represented by the historical unified qualified
profile also matched exactly. Historical accepted profile files are unchanged.
No new NGAP native wire campaign, SDK expansion or benchmark is claimed.

The initial missing-helper diagnostic is excluded from acceptance. The final
55-test rerun used real recovered/relinked helper binaries and complete logs;
test-only cleanup did not mutate the production archive. Earlier optional
resource-interrupted link attempts are not PASS evidence. The final full
campaign's completed link, execution and independent rerun are the evidence.

## Separate independent framing diagnostic

Before final candidate approval, a small public-only C++ decoder was linked
against the same production archive. The reviewer separately executed 90 valid
unknown-code-9 roots across three roles, three criticalities and payload sizes
1, 127, 128, 16,383, 16,384, 32,768, 65,535, 65,536, 65,537 and 131,072 octets.
Another 2,529 mutations matched the independent model's exact error code and
physical offset. All **2,619** checks were independently rerun successfully.
They are not added to the 4,728 semantic cases or 8,781 main checks. Fragmented
opaque receive coverage is not fragmented populated typed-BODY qualification.

Supplemental reproducibility: save the following as `framing.cpp`, compile with
strict C++20 using `-Ilibaper -Ilibngap -I<generated>` and link the attested
`libnrforge_f1ap.a`. The driver uses only the public F1AP entry point.

```cpp
#include "f1ap.hpp"
#include <iostream>
#include <string>
#include <vector>
int main() {
    std::string hex;
    while (std::cin >> hex) {
        std::vector<std::byte> input;
        if (hex != "empty") {
            for (std::size_t i = 0; i < hex.size(); i += 2)
                input.push_back(static_cast<std::byte>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        }
        const auto result = nrforge::f1ap::decode_f1ap_pdu(input);
        if (result) std::cout << "PASS\n";
        else std::cout << static_cast<int>(result.error().code) << ' ' << result.error().bit_offset << '\n';
    }
}
```

Generate ordered inputs with `reference_framing.py`: for each role, then each
policy, then each size in the list above, call
`encode_root(role, 9, policy, bytes(i % 251 for i in range(size)))`. Submit that
valid vector followed by `root_error_cases(wire, False)`. Serialize one lowercase
hex vector per line, using `empty` for zero octets and a final newline. Valid
responses must be `PASS`; invalid responses must exactly equal the expected
numeric error and offset (`constraint_violation=1`, `truncated_input=2`,
`nonzero_padding=3`, `trailing_data=4`). The independent model also has a
standalone self-test: `python tools/f1ap-wire-qualification/reference_framing.py`.

Supplemental decoder source SHA-256:
`828595dd3c0f9853eec66d0ff637a7c8be6954214b13e337b19970ddef4d8be1`;
actual executable SHA-256:
`a221b2716b12335cb40b13dbb315eb5ad265a0724b04b6c69e7fa5ad4d6b7f43`;
ordered request SHA-256:
`777d1e71f76af5cee9c5ea8ee5c0ad61730aa42d990ccc36c97fce5fd59eb724`;
actual stdout SHA-256:
`14d5572c26a457e347a925ff01320bd9cfda0c56c2f302b64cef8a90440b9c6c`;
stderr is empty. These executable hashes are provenance for the actual local
build, not a requirement that a different compiler reproduce identical ELF.

## Scope and closeout review

The contract, README, closeout and draft external summary accurately distinguish
finite independent wire evidence from compilation/integration readiness. The
4,329 omission entries are repeated selected-case traces, not distinct nested
fields or complete coverage. Unreached sizes/fragmentation of typed payloads,
extension selections, nested combinations, vendor/private interpretation,
contained RRC/NAS, mandatory-IE/application conditions and state machines remain
unqualified. Fourth-root payload support remains explicitly unsupported.
F1-P5/F1-P6 SDK delivery and E1AP/RRC are not included in this acceptance.

No blocking finding remains. Approve exact logical profile promotion in the
independently verified lossless storage envelope and normal commit/
push after the main agent's final consistency and whitespace checks. Do not
modify the approved candidate or silently expand this qualification claim.
