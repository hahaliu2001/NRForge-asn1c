# N7-P1 — Owned root-only SEQUENCE structure evidence

2026-10-09. Base: `5242b1e24bc5c4c898a83eb4bdef2a6ecc54d204`.

## Scope and design

This milestone implements the IR prerequisite in the accepted [N7 ownership contract](ngap-cpp-aper-n7-sequence-extension-ownership-contract.md). It does not implement extension framing, retention budgets, runtime primitives or generated extensible codecs.

Explicit source evidence records an extensible SEQUENCE's physical root-field count and zero schema-declared additions. Default initialization and `is_extensible` alone provide no proof. Finalization requires explicit resolved evidence and validates storage/count consistency before publishing a valid state. Successful field insertion invalidates evidence; callers editing public structures directly must validate before use. Invalid API calls leave existing evidence unchanged.

Ordinary extraction may publish proof only after walking the complete source structure and confirming one trailing extension marker. Bound SEQUENCE bodies require the same one-to-one source-component walk. The IOC message extractor flattens object-set rows into fields: these rows do not prove the physical SEQUENCE layout and cannot receive root-only wire-structure proof. Structural proof does not qualify field payloads or remove IOC/collection support restrictions.

Existing extraction acceptance, extension-addition rejection and renderer outputs remain within their committed boundaries. Independent design review confirmed these boundaries before implementation.

## Implementation and verification

The owned type stores `sequence_extension_evidence`, `sequence_root_field_count`, `sequence_known_addition_count` and `has_valid_sequence_extension_structure`. SEQUENCE-scoped setters record explicit resolved/unavailable/unsupported evidence; finalize publishes validation only on success, and validate rechecks the count/storage/status invariants. No borrowed pointers or new production allocations are introduced.

All six successful field-insertion APIs erase the evidence. Type destruction clears it, and the existing bound-body ownership move transfers its scalar metadata with the owned fields. A real parameterized fixture exercises extraction of an extensible bound body containing three physical class-selected fields, after Parser destruction. Its structural proof does not authorize encoding those selected payloads.

## Source normalization boundary

The repeated-marker source fixture is normalized by the existing Fixer to a single trailing marker. An independent scratch comparison compiled base HEAD and the worktree against the same Parser/Fixer: both extract successfully with an empty diagnostic. P1 does not change that acceptance. The focused test separately injects a second marker into a fixed AST to exercise the extractor's existing duplicate-marker rejection. Declared components after a marker remain rejected on the ordinary extraction path.

## Executed verification and independent review

| Check | Result |
| --- | --- |
| Final libasn1typed / libaper checks | 14/14 PASS / 1/1 PASS |
| Ordinary trailing marker, OPTIONAL roots and zero-root SEQUENCE | Resolved proof survives Parser destruction |
| Non-extensible/default evidence, known additions, stale valid flag and malformed storage | Validate/finalize reject as expected; no inferred proof |
| Setter misuse, successful field addition/copy, unavailable/unsupported transitions | Existing state preserved on misuse; successful mutation invalidates proof |
| Bound-body ownership move and real parameterized extraction | Metadata and owned fields survive source/body destruction |
| Flattened IOC message | Unsupported physical root-layout evidence, no valid proof |
| Native additions and injected fixed-AST duplicate marker | Existing extraction rejection retained |
| Base/worktree repeated-marker source comparison | Same Fixer normalization and successful extraction |
| Strict core C11 with conversion/sign-conversion/shadow/missing-prototypes | PASS |
| Extractor GNU99 Wall/Wextra/Werror/missing-prototypes | PASS |
| Focused core/extractor/Naming/test ASan+UBSan, NDEBUG | PASS; Parser/Fixer/common archives uninstrumented |
| Separate leak-detection attempt | LSan fatal proc/ptrace environment restriction; no usable leak verdict |
| Distribution test source and four fixtures | Present and byte-identical |
| Diff/new-file whitespace | PASS |

Independent design and final implementation review: **PASS, no blocking findings**. The reviewer inspected all six insertion invalidation paths and extraction boundaries, independently ran the final focused driver and strict core compilation, and confirmed real bound evidence after Parser destruction. The identified bound-source test gap was closed before final review. No runtime or renderer source changed; existing generated suites continue to pass.

This is structural IR verification, not APER wire or whole-message qualification. No full frozen-schema parse, benchmark, external codec comparison or NGAP codec qualification was repeated. N7-P2 framing/API contracts and runtime implementation, followed by N7-P3 generated extension preservation, remain separate tasks. Committed/uploaded under continuous authority after PASS; no merge or force push.
