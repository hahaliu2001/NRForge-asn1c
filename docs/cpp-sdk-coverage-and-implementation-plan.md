# C++ SDK coverage inventory and implementation plan

Status: planning / awaiting consolidated independent review and Owner acceptance.
Read-only source audit: 2026-10-18. No build, generation, tests or publication performed.

## 1. Verified remote baseline and ownership

| Repository | Branch | Verified HEAD |
| --- | --- | --- |
| NRForge-asn1c | feature/ngap-transfer-owned-roots | a759580b7e3edd9a7d303bc7e3ff6456fce5adb6 |
| NRForge-5GC | feature/n2-sm-transfer-sdk | c722ce219bae5867e9a0f6ec7dd8eec80818563e |
| NRForge-RAN | feature/ngap-transfer-sdk-delivery | 6d123e1169159a1409af98bffd93217b4ff22cc9 |

NRForge-asn1c owns production, verification and release of C++ SDKs and Python
wheels. RAN consumes a pinned C++ SDK; 5GC consumes a pinned Python wheel.
The previous 5GC-controlled three-repository production/publication chain is
withdrawn. Existing delivery tooling is unexecuted candidate code, not a release
receipt. Do not invoke its old full-publication entry point.

This document supersedes conflicting delivery ownership instructions. Historical
acceptance records remain historical evidence. No changes to private RAN/5GC
source are made by this planning batch; their orchestration must be adjusted
before later execution. Public ASN1 documentation records only authorized baseline
identities and ownership, not private repository source.

## 2. Coverage vocabulary and evidence rules

Coverage has separate axes: public standalone encode/decode entry, typed nested
field closure, opaque byte carrier, native finite wire qualification, installed
C++ consumer qualification, Python binding qualification, and published artifact.
A missing standalone root does not prove its nested fields are unsupported.
Bytes preserved by an outer codec do not prove the contained protocol was decoded.
A selected-case PASS does not establish every optional field, extension or value.

Each new root must record ASN.1 module/type, frozen schema hash, carrier field,
consumer procedure/direction, public header/API, typed/opaque field paths,
unsupported branches, qualification receipt, package version/fingerprint and
release digest. Uninspected paths remain UNVERIFIED. Counts describe registry
closure only; never label a whole protocol complete from message counts.

## 3. Protocol inventory

| Protocol/profile | Existing entry coverage | Separate gap or boundary | Evidence limit |
| --- | --- | --- | --- |
| NGAP | 131 outer messages / 81 procedures | 26 distinct explicit CONTAINING Transfer types; four candidate standalone entries | Outer registry is not contained-root coverage |
| RRC | 85 selected roots, including 12 channels | Six named practical roots below absent from selected manifest | full_rrc_coverage=false; nested closure must be assessed per path |
| F1AP | 158 outer messages / 94 procedures | Outer choice-extension unsupported; RRC octets cross a protocol boundary | All-slot integration is not populated mandatory-IE or exhaustive wire proof |
| E1AP | 72 outer messages / 40 procedures | No equivalent Transfer gap established by prior audit | No comprehensive absence-of-gaps proof |

Source contracts: docs/{ngap,f1ap,e1ap}-cpp-sdk-contract.md.
RRC authority: tools/rrc-sdk-expanded/build.py and
tools/rrc-sdk-expanded/verification.json. The latter contains both historical
pending-delivery wording and an exact-snapshot review record: neither establishes
a new release at this HEAD. This batch does not reconcile historical release state.

### NGAP: exact explicit CONTAINING inventory

Reconstructed by reading NGAP-IEs.asn at frozen RAN schema authority
d6e514a33ec3695925c24aa292900514b371b074, blob
a8974dd27e5c7f37bf439990fb39da397ec97044, as locked by
tools/n11-envelope-qualification/source-manifest.json.
There are 33 textual CONTAINING occurrences and 26 distinct target identities.
This inventory is limited to explicit CONTAINING targets in that module, not all
possible standalone ASN.1 types or implicitly specified byte carriers.

| Root in NGAP-IEs | Standalone candidate status | Scope |
| --- | --- | --- |
| HandoverCommandTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| HandoverPreparationUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| HandoverRequestAcknowledgeTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| HandoverRequiredTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| HandoverResourceAllocationUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyConfirmTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyIndicationTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyIndicationUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyRequestTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyResponseTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceModifyUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceNotifyReleasedTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceNotifyTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PDUSessionResourceReleaseCommandTransfer | Implemented; native finite PASS; installed SDK/wheel NOT_RUN | Batch A |
| PDUSessionResourceReleaseResponseTransfer | Implemented; native finite PASS; installed SDK/wheel NOT_RUN | Batch A |
| PDUSessionResourceSetupRequestTransfer | Implemented; native finite PASS; installed SDK/wheel NOT_RUN | Batch A |
| PDUSessionResourceSetupResponseTransfer | Implemented; native finite PASS; installed SDK/wheel NOT_RUN | Batch A |
| PDUSessionResourceSetupUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PathSwitchRequestAcknowledgeTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PathSwitchRequestSetupFailedTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PathSwitchRequestTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| PathSwitchRequestUnsuccessfulTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| SecondaryRATDataUsageReportTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| UEContextResumeRequestTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| UEContextResumeResponseTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |
| UEContextSuspendRequestTransfer | No standalone entry in four-root delivery profile | Deferred; admit only with concrete consumer need |

Four-root source entry authority: tools/ngap-transfer-sdk/prepare.py.
Headers: nrforge/ngap/transfers/{setup_request,setup_response,release_command,release_response}/root.hpp.
Python candidate API: transfers(), transfer_schema(), encode_transfer(),
decode_transfer(). New packaging/bindings are not execution-qualified yet.

Native acceptance is reused unchanged: tested source
460c79059a2af0a4ea52198206bf639c5f4234cc; 250 executed checks PASS;
evidence /home/jie/m6003-transfer-qualification-20261018T044106.740278Z/;
campaign/report.json SHA256
c95aa6e8369238cc2dd2621074641a04f4030a025f5e21b57517cba55d821397.
See tools/m6003-transfer-qualification/RESULTS.md for finite-case limitations.
The author did not access that Ubuntu directory or recalculate its report hash.
Git compare confirms accepted source is an ancestor of the baseline, two commits
behind; subsequent delivery changes do not become covered by the old receipt.

### RRC: practical missing standalone roots

| Root | Consumer purpose / priority | Current classification |
| --- | --- | --- |
| CellGroupConfig | RAN RRC setup and configuration; Batch B first | Standalone root absent; RRCSetup.masterCellGroup remains bytes per prior audit |
| UE-NR-Capability | RAN capability processing; Batch C after explicit flow selection | Standalone root absent; outer UECapabilityInformation exists |
| UE-MRDC-Capability | MRDC capability processing | Standalone root absent; deferred until MRDC required |
| CG-Config | Inter-node cell-group exchange | Standalone root absent; deferred until corresponding flow required |
| CG-ConfigInfo | Inter-node cell-group exchange | Standalone root absent; deferred until corresponding flow required |
| HandoverPreparationInformation | Mobility preparation | Standalone root absent; deferred until mobility flow required |

Absence is checked against the 85-root manifest, not inferred from a failed build.
The bytes classification above is carried forward from the Owner-supplied prior
audit; this batch has not regenerated RRCSetup headers. Do not assume that all
CellGroupConfig descendants already work merely because RRCSetup works.

Expose a separate CellGroupConfig UPER API and explicitly compose its output into
masterCellGroup bytes at the RAN adapter boundary. Keep the outer byte API
compatible. Test independent inner bytes/semantics and outer carrier preservation;
a roundtrip of arbitrary bytes is insufficient. Scope the finite field profile
before implementation; report extractor/renderer/runtime blockers rather than
silently dropping difficult fields or expanding to all RRC types.

### F1AP and E1AP boundaries

F1AP RRC byte containers remain bytes in F1AP. A caller selects the appropriate
RRC root and invokes its UPER API explicitly; do not recursively decode RRC in
the F1AP APER codec. The outer choice-extension exclusion stays explicit.
tools/f1ap-readiness/check_dispatch.py asserts unsupported_root_policy =
reject_before_header; extension support is a separate scope decision, not
implicitly part of CellGroupConfig work.

E1AP retains its finite existing profile. Audit concrete CU-CP/CU-UP payload
paths before adding a root; do not invent an E1AP expansion because NGAP has
Transfer roots. Nested IE/extension alternatives not inspected here remain
UNVERIFIED.

## 4. Implementation batches and acceptance

| Batch | Concrete work | Exit gate |
| --- | --- | --- |
| A: producer delivery ownership | Move/adapt reusable RAN packaging and 5GC wheel publication orchestration into ASN1; keep SDK generation and binding code already present | Source-only review first; no consumer repo may produce/publish ASN1 artifacts |
| A qualification | Owner later runs bounded SDK seal/build/install/relocation, four-root installed semantic/wire cases, outer 131 regression, isolated wheel fresh-install/error/limit checks | Exact source and artifact hashes; installed C++ and wheel PASS separately; accepted 250 campaign reused |
| B: CellGroupConfig | Add one independently consumable owned UPER root; finite supported field profile and carrier composition contract | Oracle/native exact wire + semantic assertions, lifetime/negative/budget checks, installed wrapper and RRCSetup carrier composition evidence |
| C: capability | Initially UE-NR-Capability only when selected RAN capability flow is defined | Root-specific finite profile and installed API evidence; do not include MRDC automatically |
| D: demand-driven expansion | Select from remaining 22 NGAP Transfer roots and deferred RRC roots using concrete procedure requirements | Explicit root list approved per substantive batch |
| Consumer adjustment | RAN pins immutable SDK asset/version/hash; 5GC pins immutable wheel/version/hash and retains R1 receive policy in adapter | Consumer-only integration checks; no generator/publisher dependency |

Batch A should preserve reuse of resource_guard.py, sealing/fingerprints,
installed consumers and independent vectors. It must remove reliance on invoking
the 5GC full-delivery driver, move producer orchestration authority into ASN1,
and separate build/qualification from publication. Frozen schemas currently
originating in RAN remain hash-pinned source inputs; this does not authorize RAN
to build/release SDKs. Decide an ASN1-owned reproducible schema input distribution
before publication without copying private consumer code into the public repo.
No standard schema change is assumed necessary simply to expose an existing root.

Publication is a later explicit execution stage after qualification, independent
review and Owner acceptance. Preserve old SDK assets and r1 wheel; allocate new
immutable versions/tags, reject collisions, and never overwrite. Record producer
SHA, frozen schema digests, SDK fingerprint, toolchain/platform/Python ABI,
wheel/SDK SHA256 and qualification receipts together. Do not invent successful
release IDs or hashes before artifacts exist.

## 5. Execution and review constraints

ChatGPT edits/pushes the authorized ASN1 feature branch. Heavy builds/tests run
only in Owner's Ubuntu terminal when subsequently requested. Use serial compile,
serial linker work, and a verified cgroup v2 limit over the entire descendant
process group. Existing candidate guard specifies MemoryMax=6 GiB,
MemorySwapMax=0, TasksMax=128, CPUQuota=150%; verify those effective limits before
launch and stop if delegation is unavailable. No sudo or unbounded fallback.
A per-process ulimit alone is not an aggregate process-group memory bound.
Do not reuse the older RRC builder's optional multi-job setting.

Retain logs and new work directories; no automatic retry after OOM. No WSLg,
benchmark, old full release driver, new build/publication, mainline merge,
history rewrite or old artifact replacement in this planning batch.
Consolidated independent read-only review and Owner acceptance remain pending.
Merge hygiene tracked in RESULTS.md (committed verification binaries and absolute
editor paths) must be handled in a separately reviewed cleanup before merge;
do not rewrite history.

## Appendix: exact historical selected RRC root entries

Copied from the manifest embedded in tools/rrc-sdk-expanded/verification.json at
the verified baseline. PASS is the historical finite profile status, not a fresh
HEAD build, full type coverage, or current publication proof.

| Module | Root | Profile status |
| --- | --- | --- |
| NR-InterNodeDefinitions | MeasurementTimingConfiguration | PASS |
| NR-RRC-Definitions | BCCH-BCH-Message | PASS |
| NR-RRC-Definitions | BCCH-DL-SCH-Message | PASS |
| NR-RRC-Definitions | CounterCheck | PASS |
| NR-RRC-Definitions | CounterCheckResponse | PASS |
| NR-RRC-Definitions | DL-CCCH-Message | PASS |
| NR-RRC-Definitions | DL-DCCH-Message | PASS |
| NR-RRC-Definitions | DLDedicatedMessageSegment-r16 | PASS |
| NR-RRC-Definitions | DLInformationTransfer | PASS |
| NR-RRC-Definitions | DLInformationTransferMRDC-r16 | PASS |
| NR-RRC-Definitions | DedicatedSIBRequest-r16 | PASS |
| NR-RRC-Definitions | FailureInformation | PASS |
| NR-RRC-Definitions | IABOtherInformation-r16 | PASS |
| NR-RRC-Definitions | IndirectPathFailureInformation-r18 | PASS |
| NR-RRC-Definitions | LocationMeasurementIndication | PASS |
| NR-RRC-Definitions | LoggedMeasurementConfiguration-r16 | PASS |
| NR-RRC-Definitions | MBSBroadcastConfiguration-r17 | PASS |
| NR-RRC-Definitions | MBSInterestIndication-r17 | PASS |
| NR-RRC-Definitions | MBSMulticastConfiguration-r18 | PASS |
| NR-RRC-Definitions | MCCH-Message-r17 | PASS |
| NR-RRC-Definitions | MCGFailureInformation-r16 | PASS |
| NR-RRC-Definitions | MIB | PASS |
| NR-RRC-Definitions | MeasurementReport | PASS |
| NR-RRC-Definitions | MeasurementReportAppLayer-r17 | PASS |
| NR-RRC-Definitions | MobilityFromNRCommand | PASS |
| NR-RRC-Definitions | MulticastMCCH-Message-r18 | PASS |
| NR-RRC-Definitions | PCCH-Message | PASS |
| NR-RRC-Definitions | Paging | PASS |
| NR-RRC-Definitions | RRCReconfiguration | PASS |
| NR-RRC-Definitions | RRCReconfigurationComplete | PASS |
| NR-RRC-Definitions | RRCReestablishment | PASS |
| NR-RRC-Definitions | RRCReestablishmentComplete | PASS |
| NR-RRC-Definitions | RRCReestablishmentRequest | PASS |
| NR-RRC-Definitions | RRCReject | PASS |
| NR-RRC-Definitions | RRCRelease | PASS |
| NR-RRC-Definitions | RRCResume | PASS |
| NR-RRC-Definitions | RRCResumeComplete | PASS |
| NR-RRC-Definitions | RRCResumeRequest | PASS |
| NR-RRC-Definitions | RRCResumeRequest1 | PASS |
| NR-RRC-Definitions | RRCSetup | PASS |
| NR-RRC-Definitions | RRCSetupComplete | PASS |
| NR-RRC-Definitions | RRCSetupRequest | PASS |
| NR-RRC-Definitions | RRCSystemInfoRequest | PASS |
| NR-RRC-Definitions | SCGFailureInformation | PASS |
| NR-RRC-Definitions | SCGFailureInformationEUTRA | PASS |
| NR-RRC-Definitions | SIB1 | PASS |
| NR-RRC-Definitions | SIB2 | PASS |
| NR-RRC-Definitions | SecurityModeCommand | PASS |
| NR-RRC-Definitions | SecurityModeComplete | PASS |
| NR-RRC-Definitions | SecurityModeFailure | PASS |
| NR-RRC-Definitions | SidelinkUEInformationNR-r16 | PASS |
| NR-RRC-Definitions | SystemInformation | PASS |
| NR-RRC-Definitions | UEAssistanceInformation | PASS |
| NR-RRC-Definitions | UECapabilityEnquiry | PASS |
| NR-RRC-Definitions | UECapabilityInformation | PASS |
| NR-RRC-Definitions | UEInformationRequest-r16 | PASS |
| NR-RRC-Definitions | UEInformationResponse-r16 | PASS |
| NR-RRC-Definitions | UEPositioningAssistanceInfo-r17 | PASS |
| NR-RRC-Definitions | UL-CCCH-Message | PASS |
| NR-RRC-Definitions | UL-CCCH1-Message | PASS |
| NR-RRC-Definitions | UL-DCCH-Message | PASS |
| NR-RRC-Definitions | ULDedicatedMessageSegment-r16 | PASS |
| NR-RRC-Definitions | ULInformationTransfer | PASS |
| NR-RRC-Definitions | ULInformationTransferIRAT-r16 | PASS |
| NR-RRC-Definitions | ULInformationTransferMRDC | PASS |
| PC5-RRC-Definitions | MasterInformationBlockSidelink | PASS |
| PC5-RRC-Definitions | MeasurementReportSidelink | PASS |
| PC5-RRC-Definitions | NotificationMessageSidelink-r17 | PASS |
| PC5-RRC-Definitions | RRCReconfigurationCompleteSidelink | PASS |
| PC5-RRC-Definitions | RRCReconfigurationFailureSidelink | PASS |
| PC5-RRC-Definitions | RRCReconfigurationSidelink | PASS |
| PC5-RRC-Definitions | RemoteUEInformationSidelink-r17 | PASS |
| PC5-RRC-Definitions | SBCCH-SL-BCH-Message | PASS |
| PC5-RRC-Definitions | SCCH-Message | PASS |
| PC5-RRC-Definitions | UEAssistanceInformationSidelink-r17 | PASS |
| PC5-RRC-Definitions | UECapabilityEnquirySidelink | PASS |
| PC5-RRC-Definitions | UECapabilityInformationSidelink | PASS |
| PC5-RRC-Definitions | UEInformationRequestSidelink-r18 | PASS |
| PC5-RRC-Definitions | UEInformationResponseSidelink-r18 | PASS |
| PC5-RRC-Definitions | UuMessageTransferSidelink-r17 | PASS |
| NR-RRC-Definitions | FilterConfig | PASS |
| NR-RRC-Definitions | FilterConfigCLI-r16 | PASS |
| NR-RRC-Definitions | Q-OffsetRangeList | PASS |
| NR-RRC-Definitions | QuantityConfigNR | PASS |
| NR-RRC-Definitions | QuantityConfigUTRA-FDD-r16 | PASS |
