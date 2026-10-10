# N12 — NGAP Tier-A codec coverage and batch plan

Date: 2026-10-09 (America/Los_Angeles). Baseline: `b72c17cf25ee18d6e98a705abc65335bc08cbdb2`.
Scope: read-only readiness evidence, shared-gap classification and next-batch planning. No production codec expansion or new wire qualification in N12.

## Authority and evidence

The Owner accepted this planning milestone after N11. Tier-A procedure scope includes NG setup, Initial UE Message/NAS transport, Initial Context Setup, PDU Session Resource Setup and deterministic UE release. The repository does not contain a previously frozen exact Tier-A message selection list. The 14 rows below are therefore an explicit **proposed planning sample**, including relevant failure outcomes and UE-initiated release; they do not redefine prior Owner scope or claim all 131 NGAP messages are required.

All six untouched TS 38.413 V18.10.0 modules match the frozen NRForge-RAN commit `d6e514a33ec3695925c24aa292900514b371b074` manifest. One fresh Parser/Fixer run sampled the current foundation. It extracted each physical IOC graph and initiating target descriptor independently, deleted the Parser tree, then called all three BODY renderers. The recorded [coverage.json](../tools/n12-codec-coverage/coverage.json) distinguishes per-message outcomes; the [README](../tools/n12-codec-coverage/README.md) gives reproduction and inventory limitations.

Historical full ordinary Typed extraction acceptance does not establish physical IOC extraction or C++ codec support. Physical counts below include known registry dependency closure and differ from historical ordinary counts. Generation PASS means three output entrypoints returned valid text; **N12 did not strict-compile that text or run its wire codec**. Only N11's target has accepted complete-PDU qualification. Empty diagnostics accompany successful extraction/generation.

## Current sampled coverage

I/S/U denote initiating/successful/unsuccessful outcomes. Rows is the source main IE object-set row count; T/B/R are current physical owned type/bound-instance/registry counts. “Initiating descriptor” tests only the N11 initiating-target API, not whether the source outcome exists.

| Message | Procedure / role | IE rows | Physical T/B/R | BODY types/mapping/codec | Initiating descriptor | Complete PDU qualification |
| --- | --- | ---: | --- | --- | --- | --- |
| UEContextReleaseCommand | 41 / I | 2 | 14/6/4 | PASS/PASS/PASS | PASS | N11 accepted, bounded |
| UEContextReleaseComplete | 41 / S | 7 | FAIL | not run | outside target role | pending |
| UEContextReleaseRequest | 42 / I | 4 | 15/5/3 | PASS/PASS/PASS | PASS | pending |
| DownlinkNASTransport | 4 / I | 23 | FAIL | not run | PASS | pending |
| UplinkNASTransport | 46 / I | 7 | 39/33/18 | FAIL/FAIL/FAIL | PASS | pending |
| InitialUEMessage | 15 / I | 22 | 67/46/25 | FAIL/FAIL/FAIL | PASS | pending |
| NGSetupRequest | 21 / I | 7 | 43/34/21 | FAIL/FAIL/FAIL | PASS | pending |
| NGSetupResponse | 21 / S | 9 | 37/19/10 | FAIL/FAIL/FAIL | outside target role | pending |
| NGSetupFailure | 21 / U | 3 | 17/7/4 | PASS/PASS/PASS | outside target role | pending |
| InitialContextSetupRequest | 14 / I | 55 | FAIL | not run | PASS | pending |
| InitialContextSetupResponse | 14 / S | 5 | 17/10/5 | FAIL/FAIL/FAIL | outside target role | pending |
| InitialContextSetupFailure | 14 / U | 5 | 21/9/5 | FAIL/FAIL/FAIL | outside target role | pending |
| PDUSessionResourceSetupRequest | 29 / I | 7 | 22/12/6 | FAIL/FAIL/FAIL | PASS | pending |
| PDUSessionResourceSetupResponse | 29 / S | 6 | 49/41/22 | FAIL/FAIL/FAIL | outside target role | pending |

Totals: 11 physical extractions pass, 3 fail; 3 BODY generation triples pass, 8 fail, 3 are not run. Eight initiating descriptors pass; six sampled outcome targets lack the initiating association. These are readiness observations, not 11/14 or 3/14 qualified messages.

Exact extraction failures:

| Message | Current diagnostic | Direct source witness and classification |
| --- | --- | --- |
| ReleaseComplete | `PDUSessionResourceItemCxtRelCpl-ExtIEs.Extension: inline constrained type is unsupported` | NGAP-IEs line 5099: selected Extension is OCTET STRING(CONTAINING PDUSessionResourceReleaseResponseTransfer). Existing bounded Contents-as-opaque rule suggests a Level-1 ownership/route composition candidate; confirm the fixed declared/combined constraint shape before fixing. OCTET codec support is a separate gap. |
| DownlinkNAS | `NGAP-IEs.primaryRATRestriction: inline constrained type is unsupported` | line 2179: BIT STRING SIZE(8, ..., 16). The known SIZE addition 16 is not represented by the current lower/upper/extensible triple; existing extensible-exact rule excludes known additions. A bounded Level-2 metadata/semantic task is needed, not just setting an extension flag. |
| ICS Request | `AreaScopeOfMDT-NR.pLMNWide: unsupported or unresolved CHOICE alternative type at line 693` | line 693: NULL alternative (EUTRA has the same shape). NULL is absent from the Owned IR primitive enum. Requires a bounded Level-2 NULL value identity and zero-bit payload contract; never remove the alternative. |

Generation's first diagnostics are `missing or unsupported IOC registry payload reference` (UplinkNAS), `unsupported compound type shape, storage or metadata` (NGSetupResponse), and `missing or unsupported physical graph reference` (the other six failed triples). These diagnostics identify the first gate, not the full missing semantic inventory.

## Shared work clusters

Each family requires a separate small contract/implementation/review task. Reuse existing semantics rather than restudying them. The matrix describes Typed extraction; current renderer/runtime contracts separately govern codec support.

| Cluster | Real witnesses | Required boundary |
| --- | --- | --- |
| Outcome envelope dispatch | ReleaseComplete; NGSetup and ICS success/failure; PDU Setup response | Generalize the validated target-role association and helper/API generation to S/U without confusing variant ordinal, PER root index or procedure code. Preserve N11 initiating compatibility. A body generator PASS does not close this gap. |
| OCTET STRING and primitive references | NAS-PDU unconstrained; PLMN/TAC SIZE(3); TimeStamp SIZE(4); UplinkNAS anonymous OCTET IEs; session transfer CONTAINING | Named and anonymous owned payload identities, SIZE/wire fragmentation/alignment, shared resource budgets; CONTAINING bytes remain opaque and do not qualify the contained NAS or transfer protocol. No fabricated schema alias workaround. |
| BIT STRING and use-site constraints | NRCellIdentity 36; EUTRACellIdentity 28; GNB-ID 22..32; AMFSetID 10/Pointer 6; NID 44; SecurityKey 256; algorithms SIZE(16,...); transport address SIZE(1..160,...) | Distinguish bit length from octet storage, non-octet trailing bits, finite/ranged/extensible domains and CHOICE/field-use constraints. Known SIZE additions need distinct evidence. |
| General INTEGER and use-site lowering | RANPagingPriority 1..256; IndexToRFSP 1..256,...; BitRate 0..4000000000000,...; timeStayedInCell inline 0..4095; periodicTime inline 1..3600,...; ExpectedActivityPeriod permitted union | Current codec accepts only exact closed zero-based 8/16/32/40-bit domains. Freeze offsets, cardinalities, extension representation and permitted sets separately; preserve existing IR intervals/tails. |
| Inline ENUMERATED and NULL | AUN3DeviceAccessInfo; UE-DifferentiationInfo; AreaScopeOfMDT | Inline enum lowering reuses frozen enum evidence within its full domain. NULL needs new owned semantics and identity-preserving CHOICE wrapper, not BOOLEAN substitution. |
| Character strings | RANNodeName/AMFName; Extended-RANNodeName Visible/UTF8 alternatives | Printable/Visible repertoire and extensible SIZE; UTF8 character/octet length distinction and fragmentation. Existing string extraction is not codec support. |

This list is a planning inventory, not proof that these families alone unlock all sampled graphs. The recorded inventory excludes bound-body internals, and extraction failure hides later dependencies. Probe the unchanged complete graph after each accepted family; inspect only a newly demonstrated unsupported construct. No unbounded blocker-by-blocker architecture expansion.

## Proposed execution order

1. **N13 — UEContextReleaseRequest complete-PDU qualification by reuse.** Current BODY outputs and initiating association pass. Generate/strict-compile actual headers, then qualify procedure 42 with code-selected dispatch, all four known IE rows (including optional session list), bounded independent native bytes/values and receiver errors. Expected outer criticality is ignore; received values remain preserved. This is the lowest-evidence-risk next message, not yet a qualified codec.
2. **Outcome bridge and NGSetupFailure.** First qualify its already-generatable BODY; separately implement/review S/U target evidence and envelope generation, then qualify procedure 21 unsuccessful outcome. Exercise Cause, TimeToWait and CriticalityDiagnostics; do not prune the two optional known rows. Keep body acceptance and complete-PDU acceptance distinct.
3. **ReleaseComplete closure.** Resolve the Contents ownership route, then the demonstrated OCTET/BIT and use-site INTEGER prerequisites through separate tasks; add successful envelope qualification for procedure 41. This is the release-cycle milestone. It is deferred from the preliminary “Complete next” recommendation because current evidence shows several genuine gaps.
4. **NAS Transport / Initial UE Message.** Reuse OCTET/BIT, add bounded missing inline enum/scalar semantics, and explicitly preserve DownlinkNAS known SIZE additions. Qualify complete procedures 46/4/15 individually with all known IE rows represented, unknown reception separate and contained NAS treated as opaque.
5. **NG Setup request/response.** Reuse outcome and payload work; close character-string and remaining use-site constraints. Qualify both procedure-21 outcomes independently. Failure acceptance from step 2 does not imply setup success acceptance.
6. **PDU Session Resource Setup request/response.** Reuse opaque transfer octets, lists and wider scalar/use-site capability. Qualify both procedure-29 directions; source extension rows can reach ExpectedUEActivityBehaviour, so “transfer is opaque” does not eliminate that separate known dependency.
7. **Initial Context Setup request/response/failure.** Keep the 55-row request last; classify the NULL and subsequent fixed-tree gaps before bounded implementation tasks. Six mandatory and one conditional request IE do not justify dropping the remaining known optional rows. Shared capability reuse should reduce work, but no task-count or completion-date promise is supported yet.

Steps 2–7 are a prioritized backlog, not bundled implementation authorization or a single oversized prompt. Owner accepted N12 planning only in this turn. Future selected milestones use the agreed autonomous design/independent-review/fix/commit/push flow within their stated scope. No merge, branch deletion or force push is implied.

## Per-message acceptance and unknown-data policy

For each selected message: verify the six frozen source identities; run the permanent probe first; retain the full approved known dependency closure; destroy Parser/Fixer before deterministic generation; strict-compile types/mapping/codec/envelope; independently compare complete PDU byte lengths and all typed values in both directions. Cover field/IE order, OPTIONAL states, known enum/choice boundaries, unknown scalar extensions, malformed known payloads, input/output/wire/collection budgets, allocation failure and first-error replay. Reuse already-qualified runtime tests instead of rerunning every historical probe unnecessarily.

At the codec layer, unknown/unmaterialized procedure payloads or unknown extensible IOC rows with valid framing retain owned opaque bytes and remain encode-refused under the existing contract. **A known row lacking a supported codec is presently a generation blocker**, not permission to relabel it unknown or silently ignore malformed known data. Any selective known-row materialization would require a separate explicit contract and complete dispatch provenance, not schema pruning. Application ignore/reject/default/mandatory-IE policy remains outside this codec milestone.

Completion requires a message-specific exact reviewed profile and zero unresolved supported byte/value disagreements. Oracle limitations and framing-only literals remain classified separately. Receipt of an unsupported payload is not qualification of its internal schema. No benchmark, external vendor interoperability claim, all-NGAP qualification or Python codec work is part of N12.

## N12 closeout evidence

The fresh batch completed with the observed outcomes above. The probe compiled with GNU C99 and `-Wall -Wextra -Werror` against the current isolated library build. The guarded report/source validation passed; source and message-order/coverage checks are reproducible. No production renderer/runtime or ASN.1 source was modified. N12 is planning evidence only; N11's 2,652-check qualification remains the complete-message baseline.

Independent read-only planning/tool-methodology review: PASS. The reviewer confirmed table totals, the three frozen-source witnesses, proposed-sample scope and ReleaseRequest-first priority. Final developer-tool build, fresh distribution identity, CLI refusal and report family/order checks passed. No codec regression/benchmark/sanitizer qualification was claimed for this planning-only task.
