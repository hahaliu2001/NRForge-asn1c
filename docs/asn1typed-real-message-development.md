# ASN1TYPED Real-Message Development Framework

## 1. Status and Scope

**Status:** Authoritative development-process contract

**Applies to:** Real-message Typed extraction qualification in NRForge-asn1c

**Protocol families:** NGAP, F1AP, E1AP, RRC

This document defines the required workflow for adding or qualifying real ASN.1 messages against the `libasn1typed` extraction foundation. It MUST be reviewed before design or implementation begins for any new real message.

Before work starts on a new message, the developer MUST:

1. Review this document.
2. Review `docs/asn1typed-semantic-capabilities.md`.
3. Run the permanent real-message probe against the unmodified current foundation.

The normal target is **2–4 interaction rounds per new message**. More than four rounds requires explicit **FOUNDATION ESCALATION**.

## 2. Purpose

A new ASN.1 message does not imply a new semantic study. The default workflow is:

```text
review framework
-> review capability matrix
-> run real message
-> PASS: qualify/review/commit
-> FAIL: inspect and classify Level 1 / 2 / 3
```

Studies are opened only when real fixed-tree evidence shows that the current semantic capability contract is insufficient.

## 3. First Real-Message Qualification Baseline

`NGSetupRequest` was the first real 3GPP Release-18 NGAP message used to qualify the Typed extraction foundation. It moved `libasn1typed` from primarily synthetic-fixture validation to a real multi-module qualification case.

Accepted baseline:

```text
Authoritative NGAP modules: 6
Parse:                      PASS
Fix:                        PASS
Typed extraction:           PASS
Extraction return code:     0
Diagnostic:                 empty
Owned types:                29
Bound instances:            20
Parser-tree independence:   PASS
Repeated clear:             PASS
```

This is a foundation qualification baseline, not a claim that every ASN.1 semantic is supported. Exact boundaries belong in `docs/asn1typed-semantic-capabilities.md`.

## 4. Capability Families Qualified by NGSetupRequest

The qualification exercised:

- owned type/module identity and cross-module dependency closure;
- CHOICE, SEQUENCE, and SEQUENCE OF ownership;
- extensible SEQUENCE and ENUMERATED metadata;
- parameterized object-set reference identity;
- bound-instance ownership and deduplication;
- semantic specialization recovery;
- bound SEQUENCE and SEQUENCE OF materialization;
- class-field and selected-type relation semantics;
- primitive types required by the qualification path;
- bounded and exact SIZE ownership;
- bounded INTEGER value-range ownership;
- inline field and CHOICE-alternative SIZE ownership;
- fail-closed handling of unsupported semantics;
- parser-tree-independent owned IR.

This section identifies capability families only. T3's Semantic Capability Matrix owns precise support and fail-closed boundaries.

## 5. Why the First Bring-Up Took Too Long

### 5.1 Blocker-by-Blocker Discovery

The initial loop repeatedly used:

```text
run -> first failure -> study -> implement -> review -> rerun
```

This is safe but inefficient for foundation discovery. Future work MUST classify failures against the capability matrix before opening a study.

### 5.2 Architecture Clusters Were Discovered Incrementally

Related semantics such as parameterized references, bound-instance identity, specialization recovery, body materialization, and class-field relations appeared as separate blockers. When failures belong to one architecture problem, work SHOULD escalate into a decomposed foundation task instead of continuing an unbounded blocker sequence.

### 5.3 Oversized Studies and Reviews

Small compatibility questions do not require architecture-level checklists. Study and review depth MUST be proportional to task level.

### 5.4 Repeated Parse/Fix and Temporary Probes

The first qualification repeatedly rebuilt the same multi-module environment and temporary fixed-tree probes. Permanent tooling MUST replace routine scratch harnesses.

### 5.5 No Authoritative Semantic Capability Inventory

Without a matrix, each blocker had to rediscover whether it was already supported, a compatibility bug, a bounded new semantic, or new architecture. The Semantic Capability Matrix MUST become authoritative for this classification.

### 5.6 Scope Was Sometimes Frozen Too Early

A diagnostic says where extraction stopped, not the complete semantic shape. Implementation scope MUST NOT be frozen from diagnostic text alone.

Before implementation, evidence MUST establish the parent construct, base type/body kind, complete relevant constraint shape, parameterization shape when applicable, exact rejection, and exact Typed IR gap.

## 6. Non-Negotiable Correctness Rules

### 6.1 Fail Closed

If Typed IR cannot faithfully retain a semantic fact, extraction MUST reject the construct. It MUST NOT silently discard constraints, parameter actuals, named semantics, extension semantics, module identity, selected/open-type relationships, or required dependency semantics.

### 6.2 Owned IR

Typed IR MUST own durable semantic state. Parser/fixer pointers MUST NOT serve as durable semantic identity.

### 6.3 Parser-Tree Independence

New owned semantics MUST be validated after parser/fixer-tree destruction where relevant:

```text
extract -> destroy parser/fixer tree -> inspect owned IR successfully
```

### 6.4 Real-Source Qualification

Synthetic fixtures are necessary but not sufficient. Real-message capability acceptance MUST eventually run against authoritative ASN.1 sources.

### 6.5 Direct Semantic Acceptance

When practical, directly inspect the real construct added by a task. Do not infer support only because the full message moved to a later blocker.

### 6.6 Independent Review Proportional to Risk

Foundation architecture changes require deep review. Small compatibility changes require compact review.

### 6.7 Stop at the Next Deterministic Failure

A bounded implementation MUST stop after its acceptance point. Later failures belong to later tasks.

## 7. Task Classification

### 7.1 Level 1 — Compatibility

The fixed-tree semantic is already inside the frozen capability contract, but extraction does not accept or route it correctly.

Workflow:

```text
reproduce -> classify -> bounded compatibility fix -> real rerun
-> compact review -> commit
```

A Level-1 task MUST NOT open an architecture study.

### 7.2 Level 2 — Bounded New Semantic

The fixed tree exposes one semantic fact not represented today, but existing architecture can accommodate it with a small target-neutral extension.

Workflow:

```text
short fixed-tree study -> freeze one bounded rule -> implementation
-> real rerun -> independent semantic review -> commit
```

A Level-2 task SHOULD prefer narrow owned semantics over a generic ASN.1 framework.

### 7.3 Level 3 — Foundation Architecture

The blocker requires new ownership, identity, dependency, parameterization, specialization, or semantic-materialization architecture.

A Level-3 task MUST begin with:

```text
FOUNDATION ESCALATION
```

Workflow:

```text
architecture study -> task decomposition -> small implementation tasks
-> deep independent review -> real qualification -> commit/freeze
```

Level 3 is exceptional. A normal message task MUST NOT silently evolve into an unbounded B1/B2/B3-style sequence.

## 8. Mandatory Workflow for Every New Real Message

1. Review this framework.
2. Review `docs/asn1typed-semantic-capabilities.md`.
3. Run the permanent real-message probe on the unmodified foundation.
4. If extraction **PASS**, do not perform a semantic study; proceed to qualification/review and commit/freeze.
5. If extraction **FAIL**, inspect the exact failing construct with the permanent fixed-tree inspector.
6. Compare the observed semantic with the capability matrix.
7. Classify the task as Level 1, 2, or 3.
8. Execute only the workflow required by that level.

**NEW MESSAGE DOES NOT IMPLY NEW STUDY.**

## 9. Frozen-Semantic Rule

A semantic already frozen in the Semantic Capability Matrix MUST NOT be re-studied merely because it appears in another message, field, module, or procedure.

A new bounded study is permitted only when fixed-tree evidence shows the observed shape lies outside the frozen support boundary. An already-covered semantic is normally Level 1.

### 9.1 Opaque OCTET STRING Contents Compatibility

When fixed-tree inspection finds a primitive `OCTET STRING` use site with
a `ContentsConstraint`, classification MUST use the bounded opaque-payload
compatibility rule in `docs/asn1typed-semantic-capabilities.md`.

A diagnostic such as `inline constrained type is unsupported` does not by
itself establish a new semantic. If the matrix's complete bounded rule is
satisfied, the case is Level 1 compatibility and reuses the existing OCTET
STRING Typed IR. Unsupported inline constraints outside that frozen rule
MUST continue to fail closed.

The development framework does not redefine the semantic boundary here; the
capability matrix remains authoritative.

## 10. Interaction Budget

### 10.1 Message Passes Existing Foundation — Target 2 Rounds

- **Round 1:** review framework + matrix; run probe.
- **Round 2:** independent qualification; commit/freeze.

### 10.2 Level-1 Compatibility — Target <= 3 Rounds

- **Round 1:** probe + classification.
- **Round 2:** implementation + focused tests + real rerun.
- **Round 3:** compact independent review + commit.

### 10.3 Level-2 Bounded New Semantic — Target <= 4 Rounds

- **Round 1:** probe + classification.
- **Round 2:** fixed-tree inspection + bounded semantic decision.
- **Round 3:** implementation + focused tests + real rerun.
- **Round 4:** independent review + commit.

### 10.4 More Than Four Rounds

Normal message development MUST stop and declare:

```text
FOUNDATION ESCALATION
```

The escalation report MUST state:

1. what architectural assumption failed;
2. why the capability matrix is insufficient;
3. what architecture capability is missing;
4. proposed architecture-task decomposition;
5. why continuing blocker-by-blocker is inappropriate.

An unbounded blocker sequence is not permitted under a normal message task.

## 11. Study Rules

Studies MUST be evidence-driven and short by default. A normal bounded study SHOULD answer only:

1. What is the real fixed-tree shape?
2. Why does current Typed extraction reject it?
3. Is the semantic already represented?
4. If not, what is the smallest missing semantic fact?
5. What is one bounded target-neutral rule?

Before freezing implementation scope, establish the parent construct, base type/body kind, complete relevant constraint shape, parameterization shape when applicable, exact rejection, and exact IR gap.

Do not freeze implementation scope from diagnostic text alone. Do not re-study accepted foundation semantics without evidence that the fixed-tree shape lies outside their frozen boundary.

## 12. Implementation Rules

A real-message implementation MUST:

- make the smallest target-neutral change;
- avoid protocol-name mappings in production code;
- reuse existing owned semantics where possible;
- fail closed outside the frozen rule;
- add focused synthetic regression coverage;
- directly verify the real construct when practical;
- rerun the real message;
- stop at the next deterministic failure.

Do not combine unrelated future blockers into one task.

## 13. Review Rules

Review depth MUST match task level.

- **Level 1:** compact diff/contract/regression/real-rerun/cleanup review.
- **Level 2:** independent semantic review covering ownership, fail-closed boundary, parser-tree independence, regression, direct real acceptance, and full-message rerun.
- **Level 3:** deep architecture review covering identity, ownership, deduplication, failure atomicity, dependency closure, parameter binding, specialization, pointer/reallocation safety, parser-tree independence, and real qualification as applicable.

Do not use Level-3 review templates for small compatibility fixes.

## 14. Commit and Session Rules

NRForge uses this durable-memory model:

```text
repository history + authoritative documents = durable project memory
AI session = temporary execution context
```

Work SHOULD be decomposed so one small logical task can be understood, implemented, tested, independently reviewed, corrected if needed, and committed.

After an accepted task is committed, start a fresh session for the next task.

Scratch investigations are temporary evidence. Important conclusions MUST be captured in code, tests, authoritative documentation, or commit history.

## 15. Permanent Tooling Contract

T2 of the Real-Message Workflow Hardening milestone will provide permanent tooling. This section defines its role, not its implementation.

### 15.1 `tools/asn1typed_real_probe`

Purpose: replace repeated temporary real-message extraction harnesses.

It SHOULD support:

- parsing authoritative modules;
- combining modules in explicit order;
- running the fixer;
- extracting a selected root message;
- stable PASS/FAIL output;
- exact diagnostic;
- owned type count;
- bound-instance count.

Conceptual success output:

```text
PARSE PASS
FIX PASS
EXTRACT PASS
ROOT NGAP-PDU-Contents.NGSetupRequest
OWNED_TYPES 29
BOUND_INSTANCES 20
```

Conceptual failure output:

```text
PARSE PASS
FIX PASS
EXTRACT FAIL
DIAGNOSTIC <exact diagnostic>
```

### 15.2 `tools/asn1typed_tree_inspect`

Purpose: replace repeated temporary fixed-tree probes with stable module-qualified inspection.

It SHOULD expose, as applicable:

- module/type/member;
- source location;
- `meta_type` / `expr_type`;
- reference components and resolved target;
- parameterization;
- presence;
- structural constraints;
- direct children.

After T2 acceptance, routine studies SHOULD use these tools. Scratch probes MAY remain for exceptional debugging or unsupported inspection needs.

## 16. Semantic Capability Matrix Contract

T3 will create `docs/asn1typed-semantic-capabilities.md`.

The matrix is the authoritative support-boundary inventory. Each capability SHOULD record:

- semantic;
- support status;
- owned IR representation;
- frozen support boundary;
- fail-closed boundary;
- real qualification evidence.

The matrix answers: **Is this semantic already supported, and within what boundary?**

This framework answers: **What process do we follow next?**

The documents MUST NOT duplicate each other's purpose.

## 17. Message Completion and Freeze

A real message is **Complete / Accepted** only when:

- authoritative modules parse;
- fixer passes;
- Typed extraction succeeds;
- diagnostic is empty;
- representative owned semantics are validated;
- relevant owned state is parser-tree independent;
- cleanup is safe.

After acceptance, freeze the message qualification. Do not keep adding extraction features to that message without a newly demonstrated regression or new requirement.

Accepted message evidence SHOULD be recorded in the capability matrix where it qualifies semantic support.

## 18. Golden Baseline

`NGSetupRequest` is the first golden real-message qualification baseline:

```text
Protocol:                   NGAP Rel-18
Authoritative modules:      6
Typed extraction:           PASS
Owned types:                29
Bound instances:            20
Parser-tree independence:   PASS
Cleanup:                    PASS
```

The permanent T2 tooling SHOULD reproduce this baseline automatically.

This document intentionally does not enumerate all 29 owned types or all 20 bound instances.

## 19. Operational Rule for the Next Message

For the next NGAP/F1AP/E1AP/RRC message:

```text
review this document
-> review capability matrix
-> run probe first
-> PASS: qualification only
-> FAIL: inspect exact fixed tree
-> classify Level 1 / 2 / 3
-> stay within interaction budget
```

The default expectation is that subsequent messages reuse the qualified foundation and finish in a small, finite number of rounds.

## 20. Normative Summary

1. **Run first; study only on evidence.**
2. **Do not re-study frozen semantics.**
3. **Fail closed; never silently lose semantics.**
4. **Own durable semantics; do not depend on parser/fixer lifetime.**
5. **Use real-source qualification.**
6. **Classify every failure Level 1/2/3 before implementation.**
7. **Match review depth to task risk.**
8. **Stop at the next deterministic failure.**
9. **Target 2/3/4 rounds; more than four requires FOUNDATION ESCALATION.**
10. **Use repository history and authoritative documents as durable project memory.**
