# RRC NULL known-open lambda strict compilation repair

Baseline: `28ec9073b93605f3053510f4c64d6a2b65f07dc9` (RRC-P15).
Branch: `feature/f1ap-cpp-aper`. One objective: close direct NULL known-open
lambda unused-io errors before RRC-P16 expanded wire qualification.

Only `libasn1typed/asn1typed_render_uper.c` changes production behavior. Direct
primitive NULL emits no stream operation; SEQUENCE/CHOICE known-open encoder
and decoder lambdas therefore had unused io parameters under existing -Werror
flags. Add a void-use only for links with node==SIZE_MAX and primitive==NULL.
Named NULL still calls its generated put/get using io; non-NULL and other
lambda text remains unchanged. No codec operation, layout, ownership, IR,
extractor, fixer, runtime or frozen schema change.

Six focused SEQUENCE/CHOICE roots cover direct NULL, named NULL and BOOLEAN.
All pass strict C++20 and twelve hand-derived goldens in three completion modes.
NULL known-open payloads retain zero semantic payload bits and the existing
one-zero-octet empty child substitution, with unchanged presence/index framing.
Malformed zero-length/nonzero padding/trailing/truncated bodies and quotas reject.
Current renderer/extractor directly instrumented ASan/UBSan and AST-deleted owned
rendering pass; LSan is NOT_RUN due to environment task enumeration limits.
Native checks pass 52 typed, two fixer, three UPER, ten APER, three NGAP and one
parser outcomes: 71 total PASS.

The driver recompiles the immutable baseline renderer from Git plus the current
renderer and regenerates all 85 frozen roots. Each baseline physical header
matches its P15 immutable hash. Removing only the newly inserted NULL-lambda
void-use markers from current headers restores every baseline header exactly.
Only LocationMeasurementIndication and UL-DCCH-Message differ, each by two void-use
markers. Both now pass strict C++20 with unchanged warnings enabled. The other
83 headers remain byte-identical, retaining their prior compile/qualification
evidence. All 85 roots still extract and render; the two known compile blockers
are closed. This is not a new exhaustive wire campaign.

Shared APER comparison verifies 361 messages and 1773 historical physical headers,
all byte-identical: F1AP 158/948, NGAP 131/393, E1AP 72/432. Current sources,
archives, binaries and logs are bound by drift-checked verification and independent
exact-snapshot acceptance before commit/push. Historical receipts are immutable.
SDK delivery remains 64 roots; expanded real-message wire qualification is P16,
followed by P17/P18 delivery. Known explicit SIZE-addition codec support remains
a separate renderer limitation outside this finite profile. No benchmark or merge.
