# N13 — Full NGAP Codec Readiness

Accepted scan baseline: `31bf9c431123b7d10e53a12ab88d7c41ff10a09c`.
Owner authorization: 2026-10-09 (America/Los_Angeles). This milestone replaces the proposed two-message batch with all-message readiness inspection. It does not expand production codec/runtime semantics or qualify additional wire encodings.

## Source and method

Frozen TS38.413 V18.10.0, RAN commit `d6e514a33ec3695925c24aa292900514b371b074`, six ordered modules verified against the existing blob/SHA256 manifest before and after each execution. The procedure declarations and class-1/class-2 root sets reconcile to 81 procedures and 131 unique payloads: 81 initiating messages, 32 successful outcomes and 18 unsuccessful outcomes. All actual procedure criticalities are explicit. Roots are 130 protocol-IE containers and one private-IE container.

Each scan uses one six-module Parser/Fixer pass, followed by all physical IOC extractions. Parser trees are deleted before BODY types/mapping/codec rendering. Generation-success triples are compiled together with the current runtime headers using strict C++20 syntax-only compilation. The report records compiler/options, input hashes, generated header hashes, all message identities/roles/procedure codes/criticalities, stage statuses and diagnostics.

Independent source reconciliation used separate source inspection and matched all 131 ordered rows. Two fresh scans produced identical message rows, diagnostics and all 18 generated-header hash triples. The second scan includes the final input-fingerprint/reporting improvements; neither execution parses separately for individual messages.

The complete machine-readable matrix is [readiness.json](../tools/n13-full-codec-readiness/readiness.json); reproduction instructions are [README.md](../tools/n13-full-codec-readiness/README.md).

## Results

| Stage | PASS | FAIL | NOT_RUN |
|---|---:|---:|---:|
| Physical extraction | 104 | 27 | 0 |
| Three BODY renderers | 18 | 86 | 27 |
| Strict combined-header C++20 syntax compile | 18 | 0 | 113 |

These are nested readiness stages. Historical ordinary Typed IR extraction acceptance for 131 messages is a different contract from physical IOC extraction. All 18 generated BODY triples compiled successfully; this establishes neither executable linkage nor complete-PDU/wire qualification. UEContextReleaseCommand retains its historical N11 bounded complete-PDU qualification; N13 adds no new wire-qualified message.

## BODY compile-ready inventory

| Message | Role |
|---|---|
| AMFConfigurationUpdateFailure | UNSUCCESSFUL OUTCOME |
| HandoverCancel | INITIATING MESSAGE |
| HandoverCancelAcknowledge | SUCCESSFUL OUTCOME |
| HandoverSuccess | INITIATING MESSAGE |
| LocationReportingFailureIndication | INITIATING MESSAGE |
| MTCommunicationHandlingResponse | SUCCESSFUL OUTCOME |
| MTCommunicationHandlingFailure | UNSUCCESSFUL OUTCOME |
| NGSetupFailure | UNSUCCESSFUL OUTCOME |
| OverloadStop | INITIATING MESSAGE |
| RANConfigurationUpdateAcknowledge | SUCCESSFUL OUTCOME |
| RANConfigurationUpdateFailure | UNSUCCESSFUL OUTCOME |
| UEContextModificationFailure | UNSUCCESSFUL OUTCOME |
| UEContextReleaseCommand | INITIATING MESSAGE |
| UEContextReleaseRequest | INITIATING MESSAGE |
| UEContextResumeFailure | UNSUCCESSFUL OUTCOME |
| UEContextSuspendFailure | UNSUCCESSFUL OUTCOME |
| UERadioCapabilityCheckResponse | SUCCESSFUL OUTCOME |
| UETNLABindingReleaseRequest | INITIATING MESSAGE |

Seven are initiating messages and eleven are outcomes (four successful, seven unsuccessful). The probe samples the existing initiating target-descriptor API; that does not supply an outcome-envelope contract or qualify new complete envelopes.

## Observed failures and shared capabilities

Extraction stops at its first error. Independent frozen-source inspection supports the following grouping of the 27 failures:

| Observed first failure | Messages | Scope implication |
|---|---:|---|
| Main IE-selected OCTET STRING with CONTAINING | 17 | Opaque contents use-site ownership route; inspect exact declared/combined constraints |
| Extension-selected OCTET STRING with CONTAINING | 1 | Same candidate ownership route, extension registry path |
| NULL CHOICE alternatives | 7 | New NULL primitive ownership and wire contract |
| BIT STRING known SIZE addition | 1 | Preserve known extension-domain evidence before wire support |
| Private-IE class/key specialization | 1 | Separate private-container architecture study |

The NULL witnesses are ReportingSystem.noReporting (two messages), AreaScopeOfMDT-NR.pLMNWide (three) and ClockQualityDetailLevel.clockQualityMetrics (two). The known SIZE-addition witness is the DownlinkNASTransport closure: ExtendedRATRestrictionInformation.primaryRATRestriction, SIZE(8,...,16). PrivateMessage uses PrivateIE-ID (local integer/global object identifier) and a non-UNIQUE private class key; it must not be reinterpreted as an ordinary numeric ProtocolIE-ID registry.

For the 86 extracted but generation-rejected graphs, first failing renderer diagnostics group as follows. These are diagnostic groups, not proven single semantic causes:

| First diagnostic | Messages |
|---|---:|
| Unsupported compound type shape/storage/metadata | 39 |
| Missing/unsupported physical graph reference | 40 |
| INTEGER constraint requires one non-extensible zero-based root range | 4 |
| Missing/unsupported IOC registry payload reference | 2 |
| INTEGER domain not exactly 8/16/32/40 root bits | 1 |

The ordinary-declaration/use-site inventory in the 104 extracted graphs gives overlapping observed lower bounds:

| Feature present | Messages |
|---|---:|
| OCTET STRING | 72 |
| BIT STRING | 52 |
| UTF8String | 6 |
| PrintableString | 6 |
| VisibleString | 7 |
| INTEGER outside current codec domain | 24 |
| Inline ENUMERATED | 10 |
| Field use-site INTEGER constraint | 10 |
| CHOICE alternative use-site SIZE constraint | 13 |

Bound-instance internals and dependencies hidden by the 27 extraction failures are not in that inventory. Counts overlap; they are neither total missing-type counts nor forecasts of messages unlocked by one change.

## Next shared-capability batch

Recommend N14 as an OCTET STRING/reference-lowering batch: study exact size/length/opaque-contents contracts, implement the accepted bounded ownership/runtime/mapping/codec capability, add independent byte/error evidence and rerun this full matrix. Include named/anonymous physical references and the selected CONTAINING route only when their exact semantics are supported; do not broaden constraints by inference. The 72 observed graphs and 18 extraction failures prioritize investigation but promise no specific unlock count.

Following shared batches should consider BIT STRING/use-site SIZE, broader INTEGER/use-site constraints and inline ENUMERATED, then bounded strings. Keep NULL, known SIZE additions and private class/key architecture explicit. Outcome-envelope support and new complete-message qualification are separate from BODY compilation. Select messages for qualification after shared capability acceptance, rather than restarting a per-message design/approval loop. N13 ends here; N14 production work is not started by this report.

## Verification and limits

The coverage probe builds with `-O2 -Wall -Wextra -Werror`. All 18 triples pass g++ 13 strict C++20 syntax checks including conversion/sign-conversion warnings; exact flags and input SHA256 values are in the matrix. Existing tools check passes 1/1. Five deliberately malformed report variants (missing row, duplicate identity, wrong generation family, missing owned-lifetime flag and failed parse) are rejected by the guard. Independent feature-count checks and Python syntax compilation pass. The probe and all three N13 tool artifacts are byte-identical in the generated tools distribution directory; tracked and new-file whitespace checks pass. Two independent review tasks passed: methodology/evidence reconciliation and frozen-source gap/priority reconciliation. Both returned no blocking findings, without repeating the expensive parse pass or modifying the repository.

No production codec/runtime source changed. No new byte vectors, runtime execution, native differential qualification, benchmark, schema mutation/pruning or full NGAP interoperability claim is part of N13. Equal first-error strings do not establish equal underlying gaps. Historical N11 qualification is not rerun or broadened here.
