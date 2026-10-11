# RRC-P3 — ordinary named type specialization and scalar DEFAULT ownership

Baseline: `e7b5b95ee70200cf110923c9262cd27cc1c01d08` (accepted RRC-P2).
Branch: `feature/f1ap-cpp-aper`. Implementation and independent acceptance are
recorded in `tools/rrc-readiness/verification-rrc-p3.json` and
`tools/rrc-readiness/review-rrc-p3.md`. Frozen schema/fixture bytes are unchanged.
This batch implements owned semantic extraction, with no merge or benchmark.

## Ordinary specialization contract

The existing opt-in `asn1typed_extract_root_graph` now accepts ungoverned named
ordinary type arguments. `ASN1TYPED_ACTUAL_TYPE_REFERENCE` is appended to the
actual-kind enum; existing object-set values and IOC APIs retain their meaning.
Ordinary instances live in `types[]`, not IOC `bound_instances` or registries.
Each owns an `ordinary_instance` reference with its actual identity vector.

For example, `SetupRelease { SL-RequestedSIB-List-r17 }` has a deterministic key
`$type$SetupRelease$PC5-RRC-Definitions$SL-RequestedSIB-List-r17`, in the template's
module. Template name and the ordered module/name vector form the key. `$` is
outside ASN.1 source identifiers. Repeated keys deduplicate; actuals with the
same spelling in different modules remain distinct. Named aliases retain their
use-site identity rather than replacing it with the terminal type name.

The extractor resolves the source generic declaration and every named actual
through explicit local/import/module-qualified namespaces. Fixed source uses
must cache a clone belonging to that exact generic's specialization table whose
actual module/name vector matches. Nil argument caches are permitted because the
fixer does not necessarily populate them; non-nil stale argument caches fail.
An original template-internal use may have no cache and is resolved by the
explicit binding context. There is no global same-name fallback, clone-name
inference, AST mutation, manual SetupRelease expansion or IOC dispatch substitution.

Materialization walks the original generic's supported semantic body with an
explicit per-instance binding context. Formal references become the actual
named identity. Multi-argument templates, imported templates, module-qualified
actuals and forwarding an outer formal to a nested named instance are supported.
A directly parameterized named alias keeps its own root/type identity and owns
the resolved instance key. Existing ordinary alias behavior remains available;
alias chains that reach fixer-only generic clones can still fail conservatively.

Closure includes every actual type, including unused formals, as well as the
materialized body's references. All published strings/keys/bodies are independent
of the parser/fixer. BFS traversal and transactional failure publication remain
as in P2. Actual dependencies count toward the reference budget; actual argument
AST visits now count toward the expression-node budget. Each use allows at most
64 ordinary type arguments, in addition to the existing graph/depth bounds.

The initial profile deliberately rejects governed value/object/value-set formals,
inline built-in or constructed actuals, nested parameterized actual expressions,
parameterized generic aliases, constraints/defaults attached to a formal use,
and anonymous constructed bodies within a generic. These shapes are not in the
338 frozen RRC SetupRelease actual sites, which use named types. `audit_p3_inputs.py` confirms all 338 source sites are named SetupRelease uses
with 264 distinct module-qualified instance keys. This shape/identity audit does
not materialize all 264 instances. This batch does
not claim that every possible ASN.1 parameter form is supported, or that all 338
source sites have successful reachable closures. Later constraints/additions can
still prevent a complete graph.

CHOICE tag evidence is taken from supported source evidence. Unavailable tags
remain unavailable; binding an actual does not synthesize a successful wire
mapping. No C++ renderer or UPER codec qualification follows from graph success.

## DEFAULT contract

`ASN1TYPED_PRESENCE_DEFAULT` is appended to presence; it differs from OPTIONAL.
An ordinary SEQUENCE field owns `default_value`, a target-neutral scalar with
INTEGER, BOOLEAN or ENUMERATED kind. Enum values own their source symbol and
assigned ASN.1 integer, not a PER index. INTEGER defaults are checked against the
supported effective field range (including inherited named constraints); BOOLEAN
must be TRUE/FALSE; ENUMERATED must identify a declared item. Unsupported,
malformed or out-of-domain defaults fail the entire graph.

Owned clear/copy/equality helpers include enum-symbol allocation and deep copy.
Field copy retains DEFAULT semantics transactionally. The
`asn1typed_field_default_effective` helper reconstructs a missing value, copies a
present scalar, and sets `suppress` exactly when the effective scalar equals the
declared default. The caller must validate a present value against its ASN.1
field domain first: this helper checks scalar storage and kind, not integer
ranges or enum membership. It performs no wire I/O, default bitmap assignment,
or mutation of the declared default. Its output must be fresh/cleared; failure
leaves output empty. An absent DEFAULT is a semantic value, not a missing OPTIONAL.

Legacy whole-module/IOC/envelope extraction retains profile zero and still
rejects ordinary DEFAULT. Existing renderers reject DEFAULT presence rather
than silently treating it as mandatory/optional. Strings, bit/octet values,
NULL and constructed defaults remain unsupported by this scalar contract.
RRC-P6 owns generated storage/encode/decode behavior using these semantics.

## Frozen RRC evidence and retained gaps

P1's six-module authentication and 80 deduplicated channel/payload/bare roots
remain the matrix authority. `record_p3.py` checks owned graphs after fixed-tree
destruction, closure, actual identity equality and DEFAULT evidence consistency.
It records a new report without rewriting the historical P1/P2 artifacts.

| Scope | P2 PASS / FAIL | P3 PASS / FAIL |
|---|---:|---:|
| 80 selected roots | 44 / 36 | **45 / 35** |
| 12 channel roots | 5 / 7 | 5 / 7 |
| 66 payloads | 39 / 27 | **40 / 26** |
| Four overlapping bare seeds | 1 / 3 | 1 / 3 |

`RemoteUEInformationSidelink-r17` now succeeds with 21 owned types and three
SetupRelease instances: SL-RequestedSIB-List-r17, SL-PagingInfo-RemoteUE-r17 and
SL-RequestedPosSIB-List-r18. The eight P2 first parameter failures are removed;
seven expose subsequent semantic gaps and one becomes PASS. No P2 PASS regresses.

| Remaining first gap | Failed roots |
|---|---:|
| SEQUENCE known additions/groups | 29 |
| Inline constrained type | 3 |
| CHOICE known additions | 2 |
| Constrained constructed alias | 1 |

The complete MTC closure remains blocked by MeasTiming additions. SIB1 and SIB2
also remain blocked; optional fields and extension chains are not filtered.
All gaps stay in the deterministic first-failure ledger, not message patches.
RRC-P4 is next for ordinary additions/groups and CHOICE extension evidence;
inline/use-site constraints and constrained aliases remain explicit follow-ups.

Five additional frozen roots (FilterConfig, FilterConfigCLI-r16,
QuantityConfigUTRA-FDD-r16, QuantityConfigNR and Q-OffsetRangeList) all succeed.
They expose **13 distinct DEFAULT fields** with enum symbol/assigned number:
fc4 / 4 and dB0 / 15. Repeated FilterConfig appearances are not counted twice.
These five are semantic seeds outside the 80-root denominator. P1 inventories
20 DEFAULT source sites; the fresh P1 AST audit confirms 15 ENUMERATED and five
INTEGER sites. The audit checks field types and identities, not all default values.
This is not a claim that all 20 complete containing
closures or their generated/wire paths have been qualified.

## Validation and limits

* Focused shared fixtures cover actual identity/order, local/imported same-name
  types, imported templates, aliases, nested forwarding, unused actual closure,
  duplicate instance reuse, stale actual and specialization caches, governed or
  wrong-arity formals, unsupported actuals/inline bodies, invalid INTEGER/BOOLEAN/
  ENUMERATED defaults, field deep copy, absent reconstruction and present-value
  equality/suppression after tree deletion.
* Every allocation position in the parameter graph (**366**) and DEFAULT graph
  (**62**) is failed in turn; diagnostics and zero unpublished output are checked.
  The P2 ordinary graph's **140** allocation positions still pass.
* Strict focused/root builds use GNU C11 with `-Wall -Wextra -Werror`.
  Typed suite **44/44**, APER runtime **10/10**, PDU suite **3/3**.
* ASan/UBSan checks the focused test plus all 80 roots and five DEFAULT seeds,
  instrumenting the changed extractor and typed core. Parser/fixer and remaining
  static libraries are the ordinary build. `detect_leaks=0` is explicit; no P3
  leak scan is claimed (P2 documented the environment's unreadable `/proc` block).
* A final-source P1 probe/scanner preserves its original report bytes. Full
  APER artifact regression covers 361 BODY/target-envelope message families:
  F1AP 158, NGAP 131, E1AP 72. All **1773** available historical header hashes
  match. NGAP historical comparison covers BODY only; its envelopes are generated
  without historical byte comparison. This does not rerun full installed SDK or
  all-message C++ compile/wire qualification.
* New RRC C++ generation/compile, UPER wire and installed SDK tests remain
  NOT_RUN. Frozen schemas and four historical bare fixtures stay unchanged.

Independent review must pass on the final source/evidence snapshot before commit
and push. No merge or performance benchmark.
