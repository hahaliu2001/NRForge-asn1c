# N11 actual NGAP-PDU envelope qualification

This directory provides bounded qualification of the generated actual NGAP-PDU
envelope with the initiating UEContextReleaseCommand typed body. A fresh
six-module generated integration run passed the independently approved exact
profile: 2,652 matched checks and zero unresolved production disagreements.
`qualification-summary.json` records source and generated header hashes.

The reference compiles all six exact authoritative modules listed in
`source-manifest.json`, after checking their ordered list, Git blob identities,
and SHA-256 hashes. It never substitutes an installed NGAP release, filters
schema declarations, changes sender bytes, or normalizes oracle output.
Install the pinned dependency from `requirements.txt` in an isolated environment.

Current native-only preflight command:

```sh
python tools/n11-envelope-qualification/qualify.py \
  --repo . --asn1-root /path/to/frozen-source-root \
  --work /path/to/scratch-work --output /path/to/preflight.json \
  --native-preflight-only
```

The generated qualification uses a fresh manifest-guarded envelope/body
probe, strict C++20 compilation, mandatory driver self-test, and exact per-case
wire fingerprints. The adapter driver is owned by the generated integration
workstream. Its commands are `encode-pdu`, `decode-pdu`, and `refuse-pdu`; `-`
selects stdin. Models are `i:CODE:CRIT:<N10-body-model>`,
`o:ROOT:CODE:CRIT:HEX` (ROOT i/s/u), and `x:INDEX:HEX`.

The native matrix has 1,311 cases: the 375 N10 body models under each of three
received envelope criticalities (1,125), 144 opaque root procedure/outcome cases,
30 opaque outer CHOICE extensions, and 12 unsupported native-schema-decodable examples. Sizes include 1, 3, 127, 128, 16,384 and
65,536 octets. Known initiating procedure 41 is always typed; malformed known
payloads must fail rather than become opaque. Received criticality does not
change this dispatch rule.

Native preflight observations are not receiver results. Seventy-two fragmented
known UE-ID cases reproduce the existing native BODY decoder exception, while
independent canonical envelope framing, standalone BODY byte equality, and
standalone UE-ID child framing/semantic checks succeed. Separately, 72 deliberately raw opaque
procedure/outcome cases select a known native registry payload and fail native
schema decoding; those bytes establish opaque framing, not valid typed values
for those unsupported procedures; they are not counted as pycrate bugs or inherited oracle limitations. The generated bounded receiver must preserve
opaque bytes and refuse their encoding. No byte repair is permitted. Twelve additional cases contain native-schema-decodable
empty protocolIEs for code 0 initiating/successful/unsuccessful and code 41
successful outcomes, each under all three criticalities. They remain opaque to
the bounded generated receiver; empty IE lists do not establish application-level
mandatory presence conformance.

The frozen registry independently reports 81 distinct procedure codes (0–80),
81 root rows, no addition rows, class DEFAULT criticality ignore, and target
code 41 with explicit reject / UEContextReleaseCommand initiating payload and
UEContextReleaseComplete successful outcome. Only the initiating target is in
the typed envelope integration scope.

Two native SEQUENCE suffix API observations are recorded separately with the
exact input key, observed key or exception, full narrow wire and its hash:
`_ext_0` loses its payload and fails selfdecode; `_ext_2` becomes `_ext_1`.
Independent suffix literals are receiver-only evidence, not native agreement.
The generated receiver passed all 28 independent physical-error literals,
including the native `_ext_0` invalid suffix, plus independent `_ext_2` suffix
ownership and encode refusal.

Generated integration command:

```sh
python tools/n11-envelope-qualification/qualify.py \
  --repo . --asn1-root /path/to/frozen-source-root \
  --probe /path/to/ngap-cpp-aper-probe \
  --work /path/to/scratch-work --output /path/to/qualification-summary.json
```

The runner verifies frozen provenance before invoking the probe, generates all
six body/envelope headers with determinism checks, captures their hashes, strictly
compiles the actual driver plus runtime, checks compilation input stability, and
runs the mandatory self-test. It then checks complete models, byte equality,
semantic native backdecode for reencodable typed cases, opaque refusal, and
independent physical-error literals. Compact per-case signatures record wire
hashes and response hashes. An absent or changed accepted profile writes only a
candidate `actual-profile.json` and exits unsuccessfully; it never auto-accepts.
Only exact checked-in profile equality publishes qualification success and zero
unresolved production disagreements.

The target-body model category includes both reencodable and opaque nested
contents: 693 envelope cases are reencodable (231 body cases × 3 criticalities),
and 432 preserve opaque nested content with encode refusal. The additional
186 unsupported root/outer-extension cases are also receive-only. These
relations are recorded separately rather than treating all 1,311 as complete
native typed semantic agreement.

The focused ownership/metadata variants can be reproduced separately:

```sh
check_asn1typed_envelope_render --actual \
  tools/qualification/ngap-rel18.modules /path/to/frozen-source-root /tmp/variants
```

This test driver emits `code73_` and `closed_` families of all six headers.
For each variant, copy its headers into a separate scratch directory with the
prefix removed (`body_types.hpp`, `envelope_codec.hpp`, etc.), then strictly
compile this directory's `driver.cpp` plus `libaper/runtime.cpp`, with includes
for `libaper`, this tools directory, and that variant's header directory.
Build code73 with `-DN11_TARGET_CODE_EXPECTED=73`; build closed with
`-DN11_CLOSED_TABLE_TEST`; execute each binary's `self-test` command. These are
explicitly hand-modified owned metadata tests. Their output is not represented
as authoritative schema extraction or native target wire evidence.

## Observed exact-profile results

| Relation | Checks |
| --- | ---: |
| Complete generated PDU model decode | 1,311 |
| Complete typed PDU byte equality and native semantic backdecode | 693 |
| Opaque encoding refusal | 618 |
| Independent receiver error literals | 28 |
| Independent suffix ownership decode and refusal | 2 |
| Total | 2,652 |

Target models comprise 1,125 cases: 1,053 have exact native full-PDU selfdecode,
and 72 have the inherited known-body fragment exception with independent outer,
BODY and UE-ID child evidence. The other categories are 72 intentionally raw
known-other framing inputs, 72 unregistered opaque procedures/outcomes, 30 outer
CHOICE extensions, and 12 unsupported but native-schema-decodable examples.
These relations remain separate in the accepted profile and summary.

The exact minimal target envelope is
`002900100000020072000400010002000f400140` (20 octets). The 16-octet BODY starts
at bit 32. The native sender preserves all three received outer criticalities;
criticality mismatch never selects an opaque fallback for initiating code 41.

The final fresh pipeline verified authoritative module identities before both
native compilation and actual Parser/Fixer extraction. It regenerated six headers
with determinism checks, preserved all three N10 BODY header hashes, strictly
compiled and self-tested the actual driver, then passed exact profile equality.
The pinned profile SHA-256 is
`fdf25531099ca94a671bc6ecfba66ec2c7e1d8ca040216caa9662643a3f6c421`.
A scratch test using the checked-in guard rejected a changed response fingerprint
with the same 2,652-check count. A copied source module with one added newline
was rejected by the frozen Git blob/SHA-256 guard before compilation. Neither
test modified repository profiles or authoritative schemas.

The recorded execution used Python
`/opt/codex/runtimes/codex-primary-runtime/dependencies/python/bin/python`,
pycrate 0.7.11, and g++ strict C++20 options including `-pedantic-errors`,
`-Wconversion`, `-Wsign-conversion`, and `-DNDEBUG`. This runner's results do not
claim sanitizer coverage, qualification of other procedure payloads, application
mandatory IE policy, general NGAP qualification, or benchmarks. Independently
run producer/runtime tests are reported in their own evidence.
