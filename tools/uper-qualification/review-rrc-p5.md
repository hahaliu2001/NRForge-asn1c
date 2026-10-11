# RRC-P5 independent review receipt

Decision: **PASS — no blocker**. Reviewer: `/root/e1_sdk_review`.
Baseline: `4fa0b0d6832cf22f412f833e3284ed15783fab08`.
Branch: `feature/f1ap-cpp-aper`.

Reviewed snapshot: `ec9d9f22d2e4eebdfe77f41dd70a0a6350b074a74d5ac088ba31c6516c80ff9e`.
Digest is SHA256 of UTF-8 `json.dumps(map, sort_keys=True,
separators=(',', ':'))`, where map is verification's source_sha256 plus
verification itself. Receipt is excluded from its own digest. The approved
commit may add this receipt; production/test/contract/evidence bytes are frozen.

Independent checks: strict C++20 -Werror focused tests; rebuilt ASan/UBSan with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`; encode/decode allocation failure
positions 17/2; oracle rerun 1136 vectors and 2272 comparisons, producing a
byte-identical qualification report. Existing 245 tracked baseline files were
compared using git show; all bytes matched. Test-suite log hashes for UPER 2/2,
APER 10/10, protocol PDU 3/3 and typed 45/45 matched verification.

Initial staging span alias heap-use-after-free was independently reproduced,
then verified fixed by pre-mutation rejection and full/subspan tests for both
public span writers. Retained unknown records/bytes are checked before payload
allocation; zero budgets allocate nothing. Invalid Completion is rejected.

The finite primitive profile and native-oracle exclusions are honest. Each
recorded wire row has at least one unmodified independent codec encode/decode;
long bitmap values above 127 have focused normative checks only. The receipt
makes no RRC generated compound, full-message wire or installed SDK claim.
No merge or benchmark is authorized by this review.

## Reviewed file hashes

| Path | SHA256 |
|---|---|
| `.gitignore` | `a56495f4cba56071238ed2b54f4d7c14a495a6a06110eb9e54040c15679b71ca` |
| `Makefile.am` | `465919db0a9251d095d35fc697e13b74e51ce4d512157594983b204ff8b5f419` |
| `configure.ac` | `8c47f292c491e47a68ab8a339abfb18b7d83abd6bd29242bf5793e3d8d854664` |
| `docs/rrc-cpp-uper-p5-runtime-closeout.md` | `27787bca5b54817afc8f5ddf75f2a90c8542e02f8bb02b30c05d9c3b12ee2d0e` |
| `libuper/Makefile.am` | `15a26fd3ab40905d9dc163edf86ea007489083bef3c42ccfb01b4dcfbb44b607` |
| `libuper/check_coexistence.cpp` | `662d676f8079800c80577584d69bfb4d673bee60f96ada7f9c5d26c4e4a2fc5e` |
| `libuper/check_runtime.cpp` | `330f168b358afced95b57fd8c044edeaf6e1a9078ef258ae77df9c3b38fad937` |
| `libuper/runtime.cpp` | `ddfa4ee7d746847e4ed9aa6c049c874d52b1808ad0cb792c9280d2ba614b59b8` |
| `libuper/runtime.hpp` | `030b9f65e1868dcd17416cbb0658bdc2eafa898082c5b924543ff081cf2fd8e5` |
| `tools/uper-qualification/README.md` | `02db2e2740f22d7e3da8f150d4a12f20280a90a84c1d9de54a617fba3b755da7` |
| `tools/uper-qualification/probe.cpp` | `8524dc7829ef2f4b385d0d70b01ce6c89a7bf4f8d2dfc8245404f496d56186b4` |
| `tools/uper-qualification/qualification.json` | `df1dc48736987d41363fe4ea726aff7c09c3bbfad56936694ef4a5dd0564d88a` |
| `tools/uper-qualification/qualify.py` | `7b897d17245be47ab71fd46537862632179c3e066c3db71e47a4517a103c44a0` |
| `tools/uper-qualification/requirements.txt` | `22ecf9e5d3cfe843a1616e5f343c2988bed2ac5cb47f7071cb2a584dfea3ee7c` |
| `tools/uper-qualification/verification-rrc-p5.json` | `5f74704593018f1fd96a190977d3688c0a74dd6826c96cfff32b7c419517ce9a` |
