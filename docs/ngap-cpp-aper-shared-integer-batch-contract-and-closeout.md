# Shared capability batch — signed extensible and permitted-set INTEGER

## Scope and authority

Owner authorized the seven remaining shared-capability batches to continue
without per-task handoffs, including the N19 prerequisite identified by N18.
This implementation follows the N18 contiguous extensible INTEGER contract and
adds a bounded, explicitly documented permitted-root-set subset. Frozen ASN.1,
Owned IR representation/extraction, old ordinary renderer entry points and
existing bounded uint/int primitives remain unchanged. Complete NGAP-PDU
qualification and performance work are not included in this component.

## Runtime contract

`read_extensible_int` / `write_extensible_int` accept one finite signed root.
`read_integer_set` / `write_integer_set` take canonical ascending, disjoint,
non-adjacent int64 intervals and an extensibility flag. Field views expose both.
The root hull is min(lower)..max(upper), while every permitted interval remains
available for membership validation. Root wire values are hull-relative offsets,
never dense ENUMERATED ranks. Zero/one flag, selector, alignment, determinant,
payload, wire/output charges and cursor publication form one atomic operation.

Extensions use actual minimal signed two's-complement octets, length 1..8,
aligned determinant/payload and no root subtraction. Both signed int64 extrema
are supported. Root canonicality, minimal signed payload/determinant, positive
sign protection, invalid fragment multipliers, supported storage ceiling,
truncation, first nonzero alignment bit and all budgets follow N18. Live/sticky
checks precede argument checks; invalid finished/moved-from objects do not mutate
context. Allocation failure leaves logical output/cursor/counters unchanged.

For discontinuous roots, the bounded policy **excludes hull-gap values**, even
when the schema is extensible. Encode and decode reject them as
`constraint_violation` without publishing cursor changes. Unknown int64 values
strictly outside the hull remain supported via the extension path. No claim is
made that this bounded policy qualifies gap-as-extension behavior. It retains
all root evidence and is narrower than arbitrary ASN.1 INTEGER.

Normative reference: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en),
11.4–11.9 and 13.1–13.2. The project-specific gap exclusion is a bounded support
policy, separate from the wire-layout rules. A legacy compiler cross-check of
`INTEGER (1..30 | 40 | 50 | 60 | 80 | 100 | 120 | 150 | 180 | 181, ...)`
produces constrained/extensible PER bounds 1..181 with 8 root bits while retaining
all nine semantic intervals; this confirms the hull offset, not a compact rank.
Pinned asn1tools 0.167.0 drops UNION tails and encodes tail-root values as
extensions. It is therefore not used as the oracle for discontinuous roots.

## Generation and evidence boundary

New ordinary opt-in APIs `asn1typed_render_cpp_owned_integer_set_{types,mapping,codec}`
(integer-only) and `asn1typed_render_cpp_owned_domain_{types,mapping,codec}`
(compound/inline-enum integration) consume existing Owned IR. Physical IOC
lowering uses the same domain support. Extensible and permitted-set storage is
int64 even for positive roots; historical positive non-extensible aliases remain
uint64. Generated mappings retain all root intervals alongside the PER-visible
hull and extensibility bit. New mapping output references runtime
`IntegerInterval`, so include runtime before types, mapping and codec as usual.

Inline primitive SEQUENCE fields and CHOICE alternatives retain their effective
interval/set evidence. Named references use their verified declaration. Any
named use-site extensible/set refinement is rejected; non-extensible refinement
of an extensible/set declaration is also rejected because effective intersection
and storage behavior have not been proved. Production extraction already rejects
those serial refinements. No declaration-only fallback or constraint erasure is
added. Residual metadata, invalid intervals, inconsistent tails, unsafe names,
collisions, unavailable references and allocation failures remain fail closed.

## Focused evidence and limitations

- Runtime test: 1,120 profile/residue/value combinations and every shorter bit
  prefix; signed extrema, singleton/full signed hull, malformed determinant/sign,
  padding-vs-truncation priority, output/wire budgets, sticky/lifecycle behavior,
  real allocation failure, root-gap rejection and canonical-set misuse.
- Real Parser/Fixer -> Owned IR -> Parser deletion -> actual generated C++:
  deterministic output, every injected generator allocation failure,
  source-array/tail invariants, invalid metadata, named refinement refusals,
  retained named/inline UNION sets, nested OPTIONAL/CHOICE signed extensions.
- `tools/shared-integer-qualification/qualify.py`: 768 actual generated codec
  cases agree with pinned native PER and an independent bit model, including
  decode fields and following-BOOLEAN end offsets. This native comparison covers
  contiguous roots only; UNION roots have separate hull-model checks and the
  legacy constraint cross-check described above.
- Full component regressions: libasn1typed 35/35 and libaper 8/8 pass. New
  renderers pass strict C11 including conversion/sign warnings; generated C++20
  and runtime pass strict warnings under NDEBUG. Runtime/generated ASan+UBSan and instrumented Typed IR/generator/OOM
  runs pass with leak detection disabled. LeakSanitizer was actually attempted
  and failed with `/proc/.../task` / ptrace attachment errors, so no LSan result
  is claimed by this component report.

Independent review and integration closeout are recorded by the batch owner once
source integration and final evidence fingerprints have been reconciled. This
component does not claim an increase in frozen full131 readiness by itself; the
batch owner performs that rescan after all shared capabilities are integrated.
