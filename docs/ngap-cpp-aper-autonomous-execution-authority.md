# First NGAP Message — Continuous Execution Authority

Owner authorization: 2026-10-09.
Target: UEContextReleaseCommand complete NGAP-PDU C++ APER encoding/decoding and independent qualification, using the untouched frozen schema.

Within this target the assistant may autonomously study/design and record contracts, implement, run relevant correctness tests, use independent agents for review, close findings, commit and push reviewed changes on the working feature branch. Routine contract and commit actions no longer require repeated Owner approval. Claude remains an optional final cross-review, not a per-task handoff.

Keep tasks small and individually reviewable. Preserve committed compatibility and support boundaries; do not prune schemas or claim unsupported codec qualification. New work within the target may extend explicitly scoped capabilities through reviewed contracts. No unsolicited benchmark, no WSLg dependency, no access to /mnt/wslg.

Escalate only a change to the target, a decision that breaks an established compatibility/support promise, an unresolved blocking finding, or destructive actions. Branch merges, deletions or force pushes are not implicit in routine implementation upload.

Report completed work, concrete validation/review evidence and limitations. Record contract and implementation milestones in the repository. Stop at the current milestone for a clear progress report when appropriate, without requesting routine approval to continue.

## N12 planning extension

Owner authorization: 2026-10-09 (America/Los_Angeles), following completion of N11. The Owner accepted N12 — NGAP Tier-A Codec Coverage & Batch Plan and the stated autonomous review/commit/push workflow. This extension authorizes coverage inspection, reproducible read-only readiness tooling, a reviewed batch plan and its evidence on the current branch. It does not authorize production codec expansion to additional messages or claim their qualification; subsequent implementation milestones remain separately selected. Preserve the same frozen schema, compatibility, no-benchmark and non-destructive branch rules.

## N13 all-message readiness extension

Owner authorization: 2026-10-09 (America/Los_Angeles). N13 supersedes the proposed two-message reuse batch with a full 131-message NGAP codec readiness scan. The Owner accepted one verified six-module Parser/Fixer pass, batch physical extraction and BODY generation, strict compilation of generation-success cases, observed shared-gap grouping, independent review and routine commit/push. No production codec expansion, new runtime semantic, schema pruning or new wire qualification is authorized by this scan. Future work is organized by shared capability clusters and batch acceptance rather than individual-message design handoffs.

## N14 shared OCTET STRING capability extension

Owner authorization: 2026-10-09 (America/Los_Angeles), “同意，做N14”. This selects the N13 recommended OCTET STRING/reference-lowering capability batch, including bounded contract study, implementation, focused correctness/independent reference checks, independent review/fix closure, full 131-message readiness rescan and routine commit/push. It authorizes no schema change, BIT STRING/NULL/private-IE architecture expansion, arbitrary constraint erasure, contained-protocol interpretation, performance benchmark or general NGAP interoperability claim. Complete-message qualification remains separate from primitive acceptance and BODY readiness. Stop after this shared-capability milestone; do not begin N15 automatically.

## N15 selected capability milestone

Owner authorization: 2026-10-09 (America/Los_Angeles), “做N15”. Implement bounded owned BIT STRING and effective use-site SIZE lowering for BIT/OCTET, focused tests and independent primitive references, independent review/fix closure, full131 readiness rescan, and routine commit/push. Preserve frozen schemas and old generated outputs. Named-bit lists, fragmented strings, extensible SIZE codecs, unrepresented SIZE additions, NULL/private-IE semantics and general NGAP wire qualification remain outside this milestone. Stop after N15.

## N16 selected capability milestone

Owner authorization: “做N16”. Implement single finite non-extensible INTEGER intervals and effective use-site value constraints, with full int64/uint64 runtime arithmetic, owned extraction, generated codecs, focused independent references, independent review/fix closure, full131 readiness rescan, and routine commit/push. Preserve frozen schemas and historical generated outputs. Extensible INTEGER, discontinuous permitted sets, unconstrained INTEGER, unrepresented registry constraints, NULL/private-IE semantics and general NGAP interoperability remain outside this milestone. Stop after N16.

## N17 selected capability milestone

Owner authorization: “做N17”. Inspect remaining shared compound/reference failures and implement evidence-backed inline ENUMERATED field lowering, focused correctness/reference tests, independent review/fix closure, full131 readiness rescan and routine commit/push. Preserve frozen schemas and existing successful generated outputs. Character strings, extensible INTEGER/SIZE, fragmented collections, NULL/private-IE semantics and general NGAP wire qualification remain separately scoped. Stop after N17.

## N18 selected study milestone

Owner authorization: “做N18”, following the N17 recommendation. Study extensible INTEGER root/extension domains using the accepted 20 INTEGER and four reference first failures, actual owned/frozen evidence, normative rules and bounded independent model/native comparisons. Record a reviewed single-contiguous-root/int64 contract and implementation gates; use independent review/fix and routine commit/push. This milestone does not change production runtime/generation, schemas or readiness, and does not qualify new complete NGAP messages. Discontinuous roots/additions, arbitrary-precision integers and other capability clusters remain separately scoped. Stop after N18.
