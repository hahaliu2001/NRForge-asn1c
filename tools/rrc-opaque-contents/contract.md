# RRC-P14 exact opaque ContentsConstraint contract

Baseline: 9c5f18a18cdc68d335a1365995ff0a1c11250261.
Primary objective: extend existing exact opaque OCTET STRING recognition to
named primitive and CHOICE contexts. Independently reviewed before final implementation.

Existing owned OCTET STRING storage suffices. No new IR, renderer or runtime.
The current complete SET/one CTNG/ATV_TYPE recognizer is reused unchanged.
Named declarations check both declared and effective constraints; ordinary
primitive alias layers must not conceal an extra restriction. An unconstrained
alias may inherit an exact effective ContentsConstraint; absence of that exact
fixed evidence does not permit guessing from its target or falling back to a
plain/SIZE-only primitive. CHOICE alternatives
use the same declared/effective boundary as existing SEQUENCE fields. Existing
parameterized-use rejection remains in effect.

Contained type references do not become graph dependencies, including
self-containing types and unsupported inner types. The carrier owns only opaque
bytes, with unconstrained OCTET STRING length framing. Neither extraction nor
carrier decoding validates the contained payload codec.

SIZE plus CONTAINING, union/intersection/mixed and malformed declared/effective
shapes remain unsupported. Non-OCTET and ENCODED BY remain unsupported (the
latter is rejected by the existing parser grammar). No SIZE constraint may be
silently discarded. Focused tests cover alias restrictions hidden by forged
effective evidence, failed graph rollback, quotas and post-AST ownership.
Readiness/strict compile results are measured; expanded real-message wire and
SDK qualification remain P16-P18. No benchmark or merge.
