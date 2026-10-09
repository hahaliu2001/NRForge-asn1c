# N3 — Owned ENUMERATED Evidence Implementation

2026-10-09. Implementation independently reviewed **PASS**, and **Owner Accepted** on 2026-10-09.
Branch: `feature/ngap-cpp-aper-first-message`.
Approved authority commit: `262a5c3d7835357cc4394882581b9fb57849202b`.
Authority: [N3 contract](ngap-cpp-aper-n3-enumerated-evidence-contract.md), N3-01 through N3-07.

## Delivered behavior

Owned enum items retain resolved signed assigned numbers or explicit unavailable/unsupported evidence. Root and addition PER indexes are independently zero-based and never replace assigned numbers. Finalize validates the whole container, computes ranks privately and publishes only the complete mapping; evidence failure retains numbers but clears all indexes. Malformed storage/API arguments return ERROR without mutation. Validate recomputes semantic and index invariants. Successful setters and insertions invalidate the mapping.

Extraction reads successfully fixed ATV_INTEGER values, checks the wider Parser domain before narrowing to intmax_t and finalizes without changing existing extraction acceptance. Source array order is retained. Extension additions need increase relative to previous additions, not the largest root number.

Both inline enum copy paths share metadata-preserving owned copying: a claimed complete mapping is validated before copy; incomplete evidence is copied without publishing indexes. Original/parser destruction does not invalidate copies. No ENUMERATED renderer, runtime API, codec or real-message integration was added.

## Verification actually executed

| Check | Result |
| --- | --- |
| Fresh libasn1typed clean/build/check, then regression after final tests | 10/10 PASS |
| libaper check | 1/1 PASS |
| New real Parser/Fixer fixture, Parser deleted before owned assertions | PASS |
| Negative, sparse, reordered, implicit, singleton, marker-only extension values | PASS |
| INTMAX endpoints and wider Parser value marked unsupported | PASS |
| Complete/incomplete/stale-claim copies through both paths | PASS |
| Finalize pending-allocation failure and allocation-point sweeps of both copy paths | PASS; no partial destination field or published indexes |
| Setter misuse, mutation, invalid storage/membership/numbers/names/indexes, repeat finalize/recovery | PASS |
| Independent read-only review and focused driver | PASS, no blockers |
| Independent 1,000 varying root/addition shape ASan/UBSan probe | PASS |
| Strict core C11 warnings including conversion/sign-conversion/shadow/missing-prototypes | PASS |
| Full focused test ASan-only, leak detection disabled | PASS |
| Hand-built evidence/copy allocation paths ASan+UBSan, leak detection disabled | PASS |
| Full Parser-inclusive ASan+UBSan no-recover | Blocked by existing libasn1parser/asn1p_integer.c:34 signed shift by 127; not recorded as PASS |
| LeakSanitizer attempted with detect_leaks=1 | Environment fatal /proc task access / ptrace restriction; no usable leak conclusion |
| git diff --check and new source/fixture whitespace | PASS |

The test uses REQUIRE/abort, independent of NDEBUG. GNU linker allocation wrapping is test-only. Independent review made no production edits. Sanitizer probes/build artifacts are scratch work, not committed regression sources. No broad sanitizer suppression was used to claim a clean full run. Existing Parser/Fixer referenced-number and negative-first-addition limitations remain unchanged as recorded in the contract.

## Scope and remaining work

N3 covers owned schema evidence only. Generated ENUMERATED mapping/types/codecs, unknown-extension object representation and NGAP integration require subsequent work. No benchmark, external wire differential, full NGAP qualification or performance claim is part of this delivery.

Owner approved committing the implementation, tests and this evidence on 2026-10-09. The approved contract was committed separately. Remote upload is not part of this commit action.
