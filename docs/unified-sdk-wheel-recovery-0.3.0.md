# Unified SDK 0.3.0 wheel recovery

The interrupted receipt remains byte-for-byte at
`/home/jie/asn1-unified-wsl-nxFcGZdf/work/wheel-recovery-20261020T150500Z/original-producer.json`.
The dedicated `tools/unified-sdk/recover_wheel.py` entry point validated that
the failure was stage 13, checked the sealed `sdk` and `relocated-sdk`
inventories, checked the two qualification-source copies of the reviewed
fixes, and reused the existing Python build cache. The wheel rebuild ran under
the existing `resource_guard.py` contract; no SDK component rebuild or
benchmark ran.

The receipt records the original producer/component identities separately from
the qualification snapshot. The NGAP/F1AP/E1AP checks remain finite scope;
E1AP is explicitly not full-protocol coverage. The outer report is PASS for
131 messages and 12 unit tests; Transfer is PASS for 143 vectors, with vector
SHA256 `3db05f1a62a6ea981869cef91919d4cfc571d3bdda8c80cb001fc0e3b8d8bd62`.

The guarded rebuild produced wheel SHA256
`f035d184cdf6c55b0763c2cc92cc3979456d2df2bc3d598163e7851f8932e4fb`.
The earlier recorded wheel SHA256
`6f0b521ea7a2b9f59387c132ddf2f0f4cf2a3ebe8dffc84771b719c1e6837c0a` is
preserved as historical evidence; the candidate uses and hashes the actual
guarded rebuild bytes. The local candidate is
`/home/jie/asn1-unified-wsl-nxFcGZdf/work/local-candidate-20261020T151000Z`.
Its `candidate.json` SHA256 is
`dded6afac9bc283439101ccf70a0c9fee43138a9c71f963ac12ddb0960e68792`.

`release.py package` completed with `publication: NOT_RUN` and
`owner_acceptance: PENDING`; no publish, retire, review, or acceptance record
was created.
