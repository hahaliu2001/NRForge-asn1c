# Unified SDK 0.3.0 targeted review B1-B5

The executable recovery fix was committed before the successful run. The
qualification source remains the historical snapshot; its real component
identities are preserved in the receipt. The final executable recovery commit
was `853db64fe771238d422ffc43d90039ab65d7e15b` (with the preceding B1-B5
fixes in `a235dac8`, `db9f78f1`, `37097fbb`, and `e9753569`).

## Review result

- B1: the guarded rebuild produced wheel
  `3035f5bffd5d0dcd0670ad296d77904f6e67d44d6b587d7a9f57e49e7c5e7502`.
  A new venv installed that exact wheel; its fresh outer and Transfer reports
  are bound by SHA256 in the receipt and report `PASS`, 131 messages/12 tests
  and 143 vectors respectively.
- B2: the receipt separates `recovery_commit`, original producer/source
  revisions, qualification source revision, component revisions, SDK bundle
  source revision, tool hashes, and registered source patches. Release locks
  use the recovery commit.
- B3: evidence directories cannot be reused. Guard status and exit code,
  wheel-rebuild status, historical wheel identity, original failure receipt,
  and current wheel identity are checked independently.
- B4: tracked inputs under `python/`, `cmake/`, `tools/`, `libaper/`, and
  `libngap/` are checked. Only the explicitly registered historical recovery
  patches are accepted; all others fail closed. Runtime/tool hashes are
  recorded.
- B5: 21 targeted tests pass, including mocked recovery success and guard
  failure, source tampering, wheel/report/identity/count failures, E1AP scope,
  coexistence, evidence reuse, and release source gating.

The fresh recovery evidence is under
`/home/jie/asn1-unified-wsl-nxFcGZdf/work/wheel-recovery-20261020T172500Z`.
Its qualification receipt SHA256 is
`b0866d7eb8ac951604939e13d7801541bfb084ae3af5d0ea6225be0ae10ee1a4`.
The new candidate is
`/home/jie/asn1-unified-wsl-nxFcGZdf/work/local-candidate-20261020T173000Z`.
Its `candidate.json` SHA256 is
`26614884754e516eb9ff2c4f4c38a4e214bc6ed43548920a3b502f54b749d5b9`.
Candidate SDK tarball SHA256 is
`2bf5f1178b956a0fb3dc2f309319fec982870cb41cea5903c02f1fab95c693db`.

The candidate includes `qualification-receipt.json` and
`recovery-evidence.tar.gz`, whose SHA256 values are bound in
`candidate.json`. Publication and retirement were not run; independent review
and Owner acceptance remain pending. F1AP/E1AP remain finite qualification
scopes, not full protocol coverage. The old candidate and historical failure
evidence remain untouched.
