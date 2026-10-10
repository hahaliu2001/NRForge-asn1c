# Shared ASN.1 NULL generation contract and evidence

## Bounded capability

This batch adds ASN.1 NULL to Owned IR extraction as a distinct primitive. The
primitive enum is appended so existing primitive numeric values stay stable.
NULL is not BOOLEAN, an unresolved reference, or an absent value. Parser/Fixer
lifecycle and Owned IR ownership follow the existing primitive contracts.

Ordinary generation opts in through `asn1typed_render_cpp_owned_null_types`,
`asn1typed_render_cpp_owned_null_mapping`, and
`asn1typed_render_cpp_owned_null_codec`. Existing ordinary entry points retain
NULL refusal. Physical IOC generation enables the capability. No schema pruning,
parser tag guessing, or protocol naming special case is added.

Generated NULL values use `std::monostate`; named declarations, CHOICE wrappers,
SEQUENCE fields, and OPTIONAL presence remain distinct. Mandatory NULL emits no
field bits. OPTIONAL NULL still has its presence bitmap bit. A CHOICE containing
NULL still emits its canonical selector. Mapping declares zero payload bits and
no payload alignment. Outer complete encoding remains a single zero-octet
substitution for a zero-bit field encoding, according to the frozen runtime
contract. A NULL open payload similarly uses complete child encoding.

NULL helpers perform no runtime primitive and therefore behave like the accepted
empty SEQUENCE helper: they do not independently query or replay a failed Field
view. Generated enclosing helpers return promptly after primitive failures, and
complete wrappers still enforce sticky state and never publish failed results.
No runtime API change is introduced.

Unsupported constraint metadata, malformed references/storage, DEFAULT,
CONDITIONAL, unavailable CHOICE evidence, and unsafe naming remain fail-closed.
Named NULL declarations carry neither INTEGER value domains nor SIZE metadata.

## Related CHOICE inline ENUMERATED closure

NULL unblocks three frozen message graphs only to reveal a related inline
ENUMERATED CHOICE payload. A CHOICE alternative now owns an identity-free
`inline_enumerated` body with an empty type reference. The new add API deep-copies
enum item names, source locations, numeric evidence, root/addition boundaries,
and finalized enum indexes transactionally. Type clear recursively releases it.
Extraction uses the existing enum Parser/Fixer numeric proof and finalization;
outer CHOICE selector evidence remains independent and is not guessed.

The generation-local lowering shared with N17 copies alternative arrays only
when necessary, creates collision-checked named synthetic enum declarations,
then delegates to the existing enum generator/runtime. The original Owned IR is
never modified. Missing enum evidence, residual reference/constraint metadata,
unsafe final spelling, and synthetic source/final-name collisions refuse output.
No general aliases, recursion or arbitrary anonymous constructed payloads are
introduced. A failure leaves no published output or partial owned alternative.

## Focused verification

`check_asn1typed_null_render.c` exercises real Parser → Fixer → Owned IR, deletes
the Parser tree before rendering, compares repeated output byte-for-byte, checks
input IR identity/primitive preservation, sweeps malloc/calloc/realloc failures,
and checks invalid namespace and residual NULL metadata refusal. A renamed
module/type test rejects any dependence on `Nothing`/`Packet` names.

Strict C++20 generated checks cover eight ordinary complete vectors: standalone
NULL (`00`), four OPTIONAL/presence BOOLEAN packets (`00`, `40`, `80`, `C0`),
reordered-tag CHOICE NULL (`00`) and BOOLEAN (`C0`), and a renamed mandatory NULL
plus BOOLEAN envelope (`80`). Decode checks presence, storage wrapper, and value.
The NULL zero-bit complete encoding metrics and nonzero substitution offset are
checked. A real physical IOC NULL row has full bytes `00 01 00 5B 00 01 00` and
decodes to the known typed payload alternative. Reordered CHOICE inline enum root values with
numeric identities -2 and 9 encode `80` and `A0`; its known extension is `C0 00`,
and an unknown extension index 2 is `C1 00`. Both wrapper and enum variants are
checked after decode. The new alternative add API has a separate allocation
failure sweep and verifies deep ownership with no partial alternative published.

The native check is `tools/shared-null-qualification/verify.py`, using
asn1tools 0.167.0. Its eleven encode/decode cases match the same hand-model complete
vectors, with two explicit limitations: native zero-octet NULL is normalized to
the runtime's outer complete substitution; asn1tools does not canonicalize the
reordered outer tags in this CHOICE, so native comparison uses the canonical
source-ordered equivalent. The actual generator test retains the reversed
storage order and reads finalized Owned IR canonical indexes. This is focused
capability evidence, not complete NGAP interoperability qualification.

Seven previously NULL-blocked frozen NGAP physical-message extractions were
re-probed. All seven advance past NULL and the related inline ENUMERATED CHOICE payload.
Five physical extractions succeed; HandoverRequest and InitialContextSetupRequest
expose a later extensible use-site SIZE (`primaryRATRestriction`). Successfully
extracted graphs then expose later INTEGER/compound dependencies. Exact rows and diagnoses are recorded
in `tools/shared-null-qualification/physical-extraction.json`; no whole-message
codec readiness increase is claimed by this isolated batch.

## Scope exclusions

No character strings, INTEGER extensions, extensible SIZE, PrivateIE keys,
fragmented collections, NULL value constraints, CHOICE extensions, or benchmarks
are introduced here. Final combined-batch readiness is assessed separately.

## Executed checks at isolated batch closeout

- `make -C libasn1typed check`: 35/35 PASS.
- `make -C libaper check`: 7/7 PASS.
- Strict C11 core/shared renderers and strict generated C++20: PASS.
- Focused changed C generator/core plus ownership/OOM driver and generated C++
  ASan/UBSan with no-recover: PASS. Parser/Fixer archives were not comprehensively
  instrumented in this focused run.
- LeakSanitizer was actually attempted and failed on ptrace/proc attachment; no
  usable leak result is claimed.
- Distribution contains all five new focused source/script/fixture files, byte
  equal to working source; `git diff --check` clean.

Independent combined-batch review and qualification are performed by the batch
coordinator after this local capability commit.

## Independent review correction

The physical envelope name-checker preflight now enables NULL with the same flag
as actual IOC rendering. A real owned physical NULL fixture checks both a safe
additional envelope symbol and a colliding payload symbol after Parser deletion.
The seven-message extraction report is frozen at capability commit `a405fdbb`;
this internal name-checker-only follow-up does not re-run that historical report.
The combined batch's final frozen-source scan supersedes it.

The new inline ENUMERATED CHOICE add API also rejects scalar metadata that its
owned body copy would otherwise discard: constraint residue, body location,
reference kind/primitive residue, and SEQUENCE/CHOICE mapping state. Its enum
storage-size overflow guard runs before item traversal. Seventeen malformed-body
probes confirm failure without publishing an alternative; the existing inline
SEQUENCE field API remains unchanged. This is a defensive manual-IR guard and
adds no schema acceptance or wire mapping rule.
