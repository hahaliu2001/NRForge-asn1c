# M6.003 finite native C++ root campaign acceptance

Owner accepted this finite verification stage on 2026-10-18 America/Los_Angeles
with "验证通过", after the independent read-only report found no substantive
native qualification blocker. This is not acceptance of M6.003 as a whole.

## Evidence and attribution

- Tested source: 460c79059a2af0a4ea52198206bf639c5f4234cc.
- Owner terminal status=PASS, qualification=PASS, checks=250.
- Local directory: /home/jie/m6003-transfer-qualification-20261018T044106.740278Z/.
- campaign/report.json SHA-256:
  c95aa6e8369238cc2dd2621074641a04f4030a025f5e21b57517cba55d821397.
- Independent reviewer read code and local artifacts and recalculated report,
  generated-header, consumer and executable hashes; no build, test, consumer,
  oracle execution, edit or push was performed during that review.
- Author records supplied terminal/hash evidence and the reviewer report here;
  the author did not access that native directory or rerun this campaign.

The reviewer independently reconstructed the 250 execution checks: 91 cases,
58 oracle backdecodes of native bytes already required equal to oracle bytes,
77 mutations, 20 budgets and four SEQUENCE-addition checks. These are execution
counts, not 250 logically independent proofs. All recorded subprocess statuses
were EXIT 0. Six schema identities/order, isolated hash-locked oracle, fresh
schema generation and four deterministic header triples were verified.

Oracle diagnostics for unknown table entries and negative cases did not fail
the campaign. The reviewer confirmed oracle-invalid examples were actual
CharpyErr/ASN1PERDecodeErr cases from each root's first oracle seed. The
ReleaseResponse mutation 80400180 was independently decoded as _ext_0=80;
native metadata was additions:1:1:0:80. No byte normalization was introduced.

Two earlier bootstrap attempts failed before building/qualifying: the archive
shorthand exclusion at d761932 and absolute historical CI test links at
bd74025. They were fixed by explicit exclusion syntax and a bounded temporary
snapshot link rebase. Original failed evidence remains local; failures are not
relabeled PASS. The exporter-only author regression passed five small tests.

## Accepted proof and retained limits

All four standalone roots passed this finite C++ campaign. Oracle-to-native
semantic assertions, native semantic construction-to-oracle exact encoding,
owned input lifetime and raw unknown metadata are supported by the recorded
cases. The later oracle backdecode is redundant once byte equality is required.
Policy classification acts on oracle fixtures before native execution and is
linked transitively by field equality, not executed on native decoded objects.
A production adapter remains separate implementation and verification work.

The review observations are retained:

- invalid_argument, invalid_state and allocation_failure mappings were not
  exercised by native failure injection. Encoder constraint_violation coverage
  concerns refusing retained unknown values, not all local validation paths.
- The 4097 input refusal is not isolated from the 32768-bit wire budget. Output
  size-1 tests independently constrain output limits; do not overclaim input
  attribution from the combined profile.
- Oracle-rejected bit mutations are few and near the start; integer range
  violations such as ARP's spare encoding were not separately qualified.
  Budget refusal alone does not prove absence of prior large allocation.
- Unknown SEQUENCE-addition evidence specifically covers ReleaseResponse;
  do not extrapolate that one suffix case into full four-root extension proof.
- Frozen qualification-contract.json remains the unmodified run input.
- VM network loss and Owner's manual shutdown were reported after the run;
  report hash survived. No causal OOM diagnosis or resource-protection fix is
  claimed. No repeat campaign was requested for this documentation closeout.

## Merge hygiene remains open

Review identified consumers/bulk run files committed under verification/ in
1e91dfb9 and local absolute .vscode/settings.json in b2d56775. They did not affect
this run because verification/ was excluded from its source export. They must
be removed from the merge delivery. History rewriting/filtering is not
implicitly authorized; no deletion, rewrite or merge is performed here.
The reviewer also observed an untracked local __pycache__, not remotely cleaned.

No RAN SDK sealing/publication, Python binding, new wheel, installed SDK
qualification, outer 131-message regression, live RAN/SCTP, branch merge/deletion
or M6.003/M6/R1 completion is authorized or claimed by this acceptance.
