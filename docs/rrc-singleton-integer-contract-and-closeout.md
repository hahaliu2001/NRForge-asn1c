# RRC-P11 closed singleton INTEGER contract and closeout

Baseline: `99784032a83ab55c3009800bdb31606eb6031000`.
Branch: `feature/f1ap-cpp-aper`. P11 follows the P10 blocker study and batch plan.

## Contract

A closed, nonextensible `INTEGER (v)` becomes the existing owned integer
domain `[v,v]`. The extractor accepts only a structurally valid solitary
`ACT_EL_VALUE` integer leaf. It creates a local, read-only range view and
reuses the established range extraction paths. Parser/fixer nodes remain
unchanged; no new IR field, renderer rule or runtime API is introduced.

Signed values through `INT64_MIN` and `INT64_MAX`, and nonnegative values
through `UINT64_MAX`, retain exact storage. Values outside supported storage,
malformed constraint leaves, extensible singleton roots, and inline constrained
collection elements remain rejected. Existing unions and known extension
addition handling are unchanged. The previous closed-singleton negative
fixture is intentionally replaced with an extensible-singleton negative fixture.

The singleton contributes zero semantic bits. Existing completion policy still
applies: octet completion substitutes one zero octet for an empty encoding;
contained-bit completion substitutes one zero bit; exact completion stays empty.
These substitutions are distinguished from semantic and padding bits.
Wrong values fail with `constraint_violation`; decoders materialize the unique
value. OPTIONAL, CHOICE and collection length determinants still contribute
their own bits. DEFAULT omission and present DEFAULT decoding retain the
existing runtime policy.

## Validation and observed scope

Native extraction tests cover owned lifetime after tree destruction, original
declared/combined AST preservation, signed/unsigned boundaries, ordinary root
aliases, direct/named fields, CHOICE, malformed leaves and atomic failed IR.
Eleven synthetic roots render and four unsupported roots reject. Strict C++20
generated-code tests cover seven scalar roots under all three completion modes,
OPTIONAL, CHOICE, DEFAULT, named-element lists and wrong-value/size rejection.

The authenticated six-module, 85-identity rescan gives **65 render PASS,
15 extraction blocked and 5 render blocked**. All 64 previously accepted
headers remain byte-identical. The four P10 first-failure transitions are:

| Root | P11 result | Next observed boundary |
| --- | --- | --- |
| `SystemInformation` | Extract/render and strict C++20 syntax compile PASS | New wire qualification deferred |
| `DL-CCCH-Message` | Extraction blocked | Constrained inline element in `sl-PSFCH-ToPUCCH-r16` |
| `RRCSetup` | Extraction blocked | Same constrained inline element |
| `RRCReconfiguration` | Extraction blocked | `MRDC-SecondaryCellGroupConfig.nr-SCG` containing context |

This is observed progress, not a prediction of later message unlocks. The
P8/P9 C++ and Python SDKs, their receipts and their **64-root delivery scope**
remain unchanged. P11 performs no new real-message wire qualification.

Shared APER regression re-extracts/renders all **361 messages** and compares
historically recorded BODY and available target-envelope headers. NGAP's
historical evidence contains BODY hashes only; current NGAP envelopes are
generated, without a historical byte comparison. Native suites cover
libasn1typed (46), libuper (3), libaper (10), libngap (3) and libasn1parser (1).
Detailed input, header, binary and log hashes are in
`tools/rrc-singleton/readiness.json` and `verification.json`; independent
acceptance is recorded in `review.md`.

## Next batch

RRC-P12 defines and implements owned constraints for primitive inline
collection elements, with rejection/lifetime and determinant coverage before a
fresh root rescan. Parameterized CHOICE tags, opaque CONTAINING contexts and
constructed aliases remain separate batches. Expanded wire qualification and
SDK delivery follow those capability batches. No benchmark, merge or schema
modification is part of P11.
