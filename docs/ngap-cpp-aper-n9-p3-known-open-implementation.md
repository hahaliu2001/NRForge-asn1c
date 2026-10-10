# N9-P3 — Known open-type runtime evidence

2026-10-09. Implemented under continuous execution authority. Independent design and implementation review: **PASS**. This milestone is runtime infrastructure, not IOC dispatch or full NGAP qualification.

## Delivered contract

`read_known_open_type<T>` and `write_known_open_type` provide synchronous child Field views over a complete open-type payload, using the original per-call context. Physical framing is charged once; collection, extension and explicit unknown-retention budgets remain shared. Child completion does not finish the enclosing call. A failed scope rolls back its cursor/output and budget charges while preserving the first sticky physical error. Nested reservations count concurrently and are released on every path.

The new staging/depth limits are appended to Limits. Decoder child errors map through fragment determinant gaps, including nested frames, to original wire coordinates. Encoder staged errors use the outermost physical known-field anchor. `FieldWriter::record_failure` supplies symmetric explicit sticky rejection. Existing unknown-open operations and ordinary lifecycle contracts are retained.

## Validation and review

- An isolated configured build ran `make -C libaper check`: **4/4 PASS**, including all three prior runtime suites and the new known-open suite.
- The focused suite compiled and ran with strict C++20, optimization, NDEBUG and active REQUIRE checks; independent reviewer reproduced that result.
- Literal vectors cover all eight starting residues, empty complete substitution, lengths 1/127/128/16383/16384/16385/32768/49152/65536/65537, fragment terminators, nested complete values and mapped physical error positions.
- Exact known BOOLEAN wire budget is 16 bits; nested known BOOLEAN is 24 physical bits, not child-plus-frame double accounting. Independent review caught a direct `read_bit` counter increment; it was replaced with the shared charging helper and protected by these regressions before acceptance.
- Tests exercise concurrent staging/depth limits and reuse, semantic-budget rollback, parent-alias guards, ignored callback errors, sticky exceptions, and actual decode-staging/encode-staging/outer-output allocation failures.
- Fully instrumented runtime and focused test ASan/UBSan run passed with `detect_leaks=0`. No successful LeakSanitizer result is claimed for this environment.

This evidence is focused correctness and compatibility validation. It does not qualify arbitrary callbacks, external codecs, whole NGAP messages, performance, or memory use beyond the documented logical staging quota. No benchmark was run.
