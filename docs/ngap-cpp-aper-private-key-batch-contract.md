# Bounded private selector / empty extensible object-set contract

This batch implements the frozen NGAP PrivateMessage physical BODY graph. The
source declaration is a non-UNIQUE class key whose fixed type is CHOICE of local
INTEGER(0..65535) and global OBJECT IDENTIFIER. The object set is empty and
extensible. It is not an ordinary numeric IOC registry and no numeric registry
rows or synthetic numeric IDs are created.

The extractor recognizes the actual class field/type/object-set shape. It owns
a separately copied object-set actual identity and a positive empty-private
binding marker. The validator rejects stale actuals, inconsistent numeric
binding metadata, missing or altered key tag/PER evidence, selector relations,
local domains and criticality evidence. The marker's setter is an evidence API:
callers may only publish it after verifying an empty extensible source object
set. Direct public-IR mutations require validation. Private sets containing
known rows remain rejected; no key values are guessed and no global keys are
lost. The generation-local view lowers the private physical entry into all
three ordered fields, preserving the real key CHOICE, received criticality
and opaque raw complete-encoding payload bytes.

OBJECT IDENTIFIER has independent primitive semantics. Its value is the
canonical BER contents-octet representation, not a machine-integer arc vector
or an OCTET STRING reinterpretation. This is lossless for arbitrary-sized arcs,
including a second arc larger than uint64. Empty contents, unterminated base-128
subidentifiers and nonminimal leading 0x80 subidentifiers are rejected. The
first subidentifier encodes the first two arcs and is retained exactly.

Normative rules inspected: [X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en),
clause 24 (aligned BER contents preceded by octet-count determinant), 23
(canonical CHOICE tag order), and 11.2 (open complete encodings);
[X.690 (02/2021)](https://www.itu.int/rec/T-REC-X.690-202102-I/en),
8.19.2–8.19.5 (minimal base-128 contents and first-two-arc packing).

The runtime OID read operation validates live state before work, snapshots its
cursor/shared wire charge, and rolls both back if canonical content validation
fails. Allocation/determinant/payload failures inherit the existing atomic
owned OCTET STRING implementation. First error remains sticky. Private unknown
open decoding uses the existing atomic open-type reader, including retained
unknown payload-octet and record budgets. Raw private encode rejects an empty
complete encoding and emits the original contents; received criticality is
preserved without claiming vendor payload semantics. Existing numeric IOC
unknown encoding remains refused.

Current pre-fragmentation implementation ceiling is 16383 unconstrained content
or encoded payload octets, inherited from the existing runtime; fragmentation
is a separate batch capability. Container SIZE is the unchanged frozen
1..65535 domain. Input/output/wire, collection and retained-unknown budgets
remain per-call. No schema declarations are weakened or pruned.

Evidence: real renamed Parser→Fixer→Owned IR fixture; Parser deleted before
three-family generation; repeat-output comparison; exhaustive renderer
allocation-failure sweeps; transactional private evidence-setter failures;
malformed/stale evidence refusal; local/global/raw complete-value roundtrips;
malformed OID cursor/wire rollback and sticky error; truncation, trailing,
private raw retention limits. 240 generated BODY vectors match native APER
framing, including determinant boundaries and payloads through 16383 octets.
Independent pycrate OID decoding verifies 120 global cases. The native
asn1tools 0.167.0 decoder's incorrect first-arc handling is explicitly recorded
in tools/private-key-qualification/README.md and results.json.

This is a bounded private BODY capability, not known vendor IOC dispatch,
private full NGAP-PDU interoperability, an external codec qualification claim,
a benchmark or a claim that all 131 messages are supported. Frozen PrivateMessage
physical extraction/types/mapping/codec generation has been observed to pass;
the combined batch's final scan and independent review govern final readiness.

Focused verification: strict C11 generator warning checks; strict generated
C++20/NDEBUG compilation; normal Typed checks 35/35 and runtime checks 7/7;
C generator plus exhaustive renderer allocation-failure paths under ASan/UBSan,
and generated runtime/codec ASan/UBSan, all pass. The C sanitizer run caught and
fixed a selected-field translation before raw-open replacement; final headers
match the normal accepted outputs. Parser/Fixer archives were not rebuilt with
sanitizers. LSan was actually attempted but failed to inspect `/proc` under the
execution environment; it is not reported as a passing leakage result.

Independent final read-only review: PASS, no blocking findings. Reviewer
replayed the instrumented C-generator allocation sweep, strict generated
ASan/UBSan checks, all 8 OID alignment residues and live/sticky priority, native
240 cases plus 120 pycrate global OID checks, current 11 source fingerprints
and the normal 35/35 + 7/7 logs.
