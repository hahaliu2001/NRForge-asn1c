# N18 — Extensible INTEGER root / extension domain contract study

## Authority, baseline and outcome

Owner selected N18 following the N17 recommendation. This milestone is a **study and reviewed contract**, with reproducible read-only inventory and independent model/native reference evidence; it does not implement a runtime primitive or enable production generation. The autonomous independent review/fix and routine commit/push workflow applies. Baseline `0bda141dea90f9df8baba0d6ecc64598c2330f00`, branch `feature/ngap-cpp-aper-first-message`.

Frozen authority remains TS 38.413 V18.10.0 at NRForge-RAN `d6e514a33ec3695925c24aa292900514b371b074`, six unmodified ASN.1 modules, 131 messages / 81 procedures. Historical readiness remains physical 122 PASS / 9 FAIL, BODY 69 PASS / 53 FAIL / 9 NOT_RUN and strict compile 69 PASS / 0 FAIL / 62 NOT_RUN. N18 makes no readiness increase or new complete-message qualification claim.

Decision: prepare a separately selected implementation milestone for **one finite contiguous extensible INTEGER root and an int64 extension storage domain**. Preserve other domains rather than approximating them. No new arbitrary-precision storage, schema edits, performance benchmark, string extension or collection fragmentation work.

## Observed gaps and Owned IR evidence

`tools/n18-integer-study/domain-inventory.json` is the reproducible inventory of the 20 INTEGER-first and four physical-reference-first failures in the accepted N17 report, including named and inline domains. These are first failures, not 24 predicted codec unlocks. Other dependencies remain masked. The inventory tool parses/fixes the frozen modules, extracts actual owned graphs and prints scalar range and every tail interval; it does not infer a domain from the generic renderer diagnostic alone.

The four reference-first messages encounter already-owned extensible inline INTEGER evidence: periodic/time-of-day intervals in ConnectionEstablishmentIndication and UEInformationTransfer; dl-DataSize in RANPagingRequest; ClockAccuracy alternatives in TimingSynchronisationStatusReport. The last has a third IOC extension-container alternative: accepting its integer payloads alone does not prove the entire graph is renderable.

Owned `asn1typed_integer_value_range_t` stores `has_value_range`, finite `lower_bound` / `upper_bound`, `is_extensible` and owned `tail[]`. Tail intervals represent additional intervals **in the root permitted set**, not extension additions. ExpectedActivityPeriod and ExpectedIdlePeriod have root `1..30 | 40 | 50 | 60 | 80 | 100 | 120 | 150 | 180 | 181`: normalization merges 180 / 181 into `[180,181]`, while gaps remain. They must not be lowered as either `1..30` or `1..181`, nor as dense enum-style ranks.

Source inspection at the baseline establishes these boundaries:

- `extract_integer_value_range` recognizes a single finite range or bounded normalized UNION, optionally followed by a bare extension marker. Explicit extension additions and unbounded bounds are not represented by this path and are refused.
- Named serial/effective INTEGER constraint intersection accepts only non-extensible single intervals; it refuses extensible/tail refinements. Do not reconstruct effective constraints from the named declaration alone.
- Anonymous SEQUENCE/CHOICE use-sites retain their own effective range evidence; the proposed future lowering must verify this evidence and residual metadata independently.
- Selected IOC primitive constraints have no new registry range slot. An absent slot is not proof of an unconstrained extension domain.

The selected graphs contain 20 unique relevant extensible domains: 12 contiguous named roots, two discontinuous named roots, four inline SEQUENCE fields and two inline CHOICE alternatives. This is distinct from the 20 messages whose first diagnostic is INTEGER-related.

No new core Owned IR variable-length representation is needed for the proposed single-root subset. Existing root/tail ownership remains unchanged. Later support for explicit additions or general constraint algebra would require its own evidence/representation task.

## Normative wire distinction

Primary source: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en), clauses 11.4, 11.5, 11.8, 11.9 and 13.1–13.2.6. An extensible INTEGER adds a leading bit. A root value uses the ordinary constrained layout. An extension value uses the actual integer's minimum-octet signed two's-complement representation with an unconstrained octet length determinant; it is not a root-relative offset, an enum addition index or a normally-small number. In APER the extension length and integer contents are octet aligned. Positive values whose high bit would indicate a negative sign need a leading zero octet; negative values retain only necessary sign extension. These rules are normative; the bounded storage and rejection policies below are project decisions.

## Reviewed design decisions for the next implementation

| ID | Contract |
| --- | --- |
| N18-01 | Accept exactly one verified finite contiguous root with a bare extension marker, bounds fitting int64, no tail pointer/count or unrepresented constraint residue. Declaration and effective use-site evidence must agree. Existing production extraction must continue rejecting explicit additions and serial refinements; the scalar marker alone is not proof of those exclusions in arbitrary hand-built IR. Refuse discontinuous roots, explicit additions, unconstrained/semi-constrained declarations and unverified serial refinements. |
| N18-02 | Newly supported extensible INTEGER types use `std::int64_t` for the entire supported root/extension value domain. A positive root does not imply only positive extensions: negative extension values remain representable. This is a bounded implementation domain, not a claim to support ASN.1's arbitrary-precision INTEGER. Historical non-extensible uint64/int64 types and outputs stay unchanged. |
| N18-03 | Encoding selects the root path iff the value lies in the accepted interval. Emit the extension bit once. Root payload reuses N16 interval arithmetic/layout at the cursor after that bit. Even a singleton root has its extension bit. Never discard the marker or use a fixed root-width extension payload. |
| N18-04 | Extension encoding aligns with zero bits, writes a one-octet unconstrained length 1–8, then the minimal signed big-endian value octets. Zero is one value octet; 128 is `00 80`, -128 is `80`, -129 is `FF 7F`. Full signed int64 extrema require eight octets. No subtraction of the root lower bound. |
| N18-05 | Decode checks the extension bit and validates root offsets through N16. An extension payload in the root interval is refused as an inconsistent path. Extension values outside the root are retained exactly within int64, including negative unknown values. Explicit known-addition lists are excluded from this first subset. |
| N18-06 | Decode accepts only shortest unconstrained length and shortest signed payload: zero length, unnecessary sign octets, overlong length form for 1–8 or inconsistent root/extension path are `constraint_violation`; a determinant declaring more than eight octets, including valid fragment markers C1–C4, is `resource_limit` under the documented representation ceiling. Invalid fragment multipliers C0 or C5–FF are determinant syntax errors (`constraint_violation`), checked before the support ceiling. Do not read or allocate a huge payload merely to discover it is unsupported. |
| N18-07 | New tentative cursor/Field primitive pair `read_extensible_int(lower, upper)` / `write_extensible_int(value, lower, upper)` must treat extension bit, root selector or extension length, alignment and payload as **one atomic operation**. Success publishes all cursor/counter/output changes; failure publishes none, including the extension bit. Calling existing public primitives sequentially without a transaction does not satisfy this requirement. |
| N18-08 | Live/sticky checks precede argument/value checks. Healthy reversed bounds produce `invalid_argument` at primitive start; finished/moved-from objects return `invalid_state` without mutation; failed objects replay the original error. Malformed-domain/canonicality/representation/budget failures report primitive-start offset, nonzero alignment reports the first offending bit, and truncation reports the first unavailable bit. For supported lengths, check availability of the complete primitive before payload canonicality/alignment and budget checks, matching N16; validate determinant syntax/support limits as soon as its bytes are available. Test simultaneous failures explicitly. No partial cursor publication is allowed in any order. |
| N18-09 | Input, output and wire budgets include all new header, alignment and payload bits/octets. No collection-element charge or new extension-owned allocation is needed. Complete padding, empty substitution, nested Field views, first-error replay and result publication retain S3/N16 behavior. Root/extension arithmetic and sign decoding must avoid signed overflow, shifting negative operands and implementation-defined out-of-range signed conversion. |
| N18-10 | New ordinary opt-in renderer paths and the separately scoped physical lowering may consume only verified declaration/use-site intervals. Reuse final Naming and owned references; temporary inline lowering remains collision-checked and leaves the original IR unchanged. Existing public renderer boundaries and all 69 prior successful header triples must remain compatible. Integration API names are finalized during the implementation, without silently broadening old ordinary entry points. |

The table records the autonomous reviewed design; it is not a fabricated historical Owner approval record. Production behavior changes only in a subsequently selected implementation milestone.

## Study reference evidence and limits

`reference.py` compares an independent bit model with pinned asn1tools 0.167.0 APER (`per`) encoding and native decoding. Eight roots cover singleton, small-bit, 255/256/257/65536/65537 cardinality boundaries and a larger NGAP-like interval. Every initial bit residue 0–7 is exercised, with a following BOOLEAN to expose the INTEGER end position. Values include root boundaries, adjacent extension values, positive sign transitions, negative sign transitions and INT64_MIN/MAX.

Accepted `reference-summary.json`: **768 cases**, 280 root / 488 extension; all complete byte strings and all native decoded fields agree. Selected vector records explicitly include the following BOOLEAN, so their `field_end_bit` is the end of that following field, not the INTEGER cursor. The report fingerprints its reference script. This is a study model/native agreement, **not** execution of a new NRForge primitive or generated codec. It does not establish malformed input rejection, sticky behavior, atomicity, resource policy, discontinuous-root semantics or complete NGAP interoperability.

No production source/header/runtime or frozen schema is changed. No readiness change is claimed; no new full131 rescan or production regression is included in this study. Readiness numbers are retained from N17. Relevant checks are inventory reproducibility/source hashes, reference reproducibility, independent contract review and distribution/whitespace checks.

## Validation and independent review

Independent review: **PASS / Ready to Commit YES**, with no blocking findings. The reviewer independently reran both reports and obtained byte-identical output. Root also rebuilt the read-only probe and reproduced the inventory. Six frozen ASN.1 hashes, all eight inventory input fingerprints and the reference-script fingerprint reconcile. Seven study-tool artifacts were copied through `make -C tools distdir` and verified byte-identical. Python syntax, probe strict C11 syntax, tracked/untracked whitespace and zero production diff checks pass. No new runtime/generated-code sanitizer or regression result is claimed by this study.

## Next task and acceptance gates

Recommend **N19 — Bounded Extensible INTEGER Implementation**, internally split into runtime primitive/API and generated integration/evidence work so each remains reviewable. Do not begin it automatically at N18 closeout.

1. Implement/review the atomic signed runtime primitive with all root thresholds, residues, signed extrema, malformed length/sign/path cases, truncation points, budgets, allocation failure if any, lifecycle and ignored-error/sticky checks. A native encoder agreement alone is insufficient.
2. Implement/review ordinary plus physical inline INTEGER generation from owned declaration/use-site evidence. Test renamed graphs, SEQUENCE/CHOICE payloads, collision/refusal, Parser destruction, deterministic output and OOM ownership. Preserve old generated outputs; do not erase tail intervals to reach more messages.
3. Run generated-code independent native/model references and a fresh frozen full131 readiness scan; reconcile source fingerprints and original 69 successful header triples. Report actual unlocks and newly exposed blockers, not a predicted coverage count. Complete-PDU qualification remains separate.

N18 stops after the reviewed study and routine commit/push. Extensible SIZE/strings, fragmented collections, NULL, private-IE keys, discontinuous INTEGER roots and new complete-message qualification remain separate work.
