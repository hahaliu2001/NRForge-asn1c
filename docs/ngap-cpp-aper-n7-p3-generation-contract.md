# N7-P3 — Generated root-only SEQUENCE extension integration

2026-10-09. Base: `d0b9e608f5aacd068ffce2d04f2657ff5f4c5f03`. Accepted under continuous execution authority after independent design review **PASS**, with no blocking findings.

## Opt-in generation and compatibility

Add `asn1typed_render_cpp_owned_sequence_extension_types`, `_mapping` and `_codec` as an opt-in family sharing N6's compound planner/emitter. Existing N6 entrypoints retain their rejection of extensible SEQUENCEs and their byte-identical output. New entrypoints admit ordinary N6 graphs plus root-only extensible SEQUENCEs with validated N7-P1 structural evidence. All three outputs must come from the same unmodified IR and namespace; do not mix generation families for the same graph.

Reject missing/unsupported/stale evidence, known additions/groups, extension CHOICE, IOC/bound instances, collections, DEFAULT/CONDITIONAL and other unsupported N6 payload semantics. Structural proof does not expand payload support. No schema filtering or name-based wire behavior.

The new family reserves `nrforge::aper` and its descendant namespaces for runtime/support declarations and rejects them before output. Other prefixes such as `foo::nrforge` remain supported. This restriction does not change the existing N6 entrypoints.

## Owned sidecar and naming

New support header `libaper/sequence_extensions.hpp` defines `::nrforge::aper::UnknownSequenceAddition` with `std::uint64_t addition_index` and `std::vector<std::byte> payload_octets`, and `SequenceExtensionData` with `std::size_t received_bitmap_bit_count` and `std::vector<UnknownSequenceAddition> unknown_additions`. Default initialization sets width/index to zero and vectors empty. Standard copies deeply copy bytes/records; moves transfer ownership.

Only new output containing an extensible SEQUENCE includes the support header. Each extensible struct appends one sidecar after its root fields. Planning starts with final spelling `sequence_extensions`, then chooses deterministic `_1`, `_2`, etc. suffixes when a root/member/type name already uses it. Do not reject an otherwise supported root field to reserve the preferred spelling. Declaration, references, mapping member pointer and collision checks use the same planned final name.

Mapping records `extensible=true`, validated physical `root_field_count`, `known_addition_count=0`, `extension_data_type` and `extension_data_member` (pointer to the selected sidecar). Root declaration/presence ordinals remain separate from addition indexes. Non-extensible N6 types in the new family retain their existing layout and wire rules.

## Encode/decode behavior

For an extensible SEQUENCE, encode first checks its sidecar: any nonzero width or nonempty records calls P2 `reject_sequence_extension_data()` before emitting a bit or root payload. Default sidecar encodes an extension bit of zero, then the root OPTIONAL bitmap and fields in declaration order. No opaque extension bytes are encoded or silently discarded.

Decode reads the extension bit before root presence/fields. After roots, a set extension bit invokes P2 bitmap framing; every set position invokes P2 owned open-type framing in increasing order. Retain the exact received width, full uint64 addition index and payload bytes. Check conversions before narrowing/indexing. No inner payload interpretation, fresh context, complete wrapper or finalization occurs inside helpers.

Compound helpers in the new family move successfully decoded fields/wrappers/results, including non-extensible parents of extensible children. New owned vectors can allocate during result/container construction: catch `bad_alloc` as sticky `allocation_failure` and `length_error`/unrepresentable sizes as sticky `resource_limit`, at the current field cursor. Completed primitive charges/cursor remain committed if later record-container allocation fails; complete wrappers publish no partial object. A caller-side throwing variant construction is guarded by the existing valueless check rather than assumed unreachable.

## Minimal runtime failure-report hook

Add header-only `Result<void> FieldReader::record_failure(Error)` for generated helper failures. It validates reader live state first, replays any existing first error, returns finished/moved-from `invalid_state` without context mutation, and otherwise records the supplied structured failure through the reader's existing sticky mechanism. Friendship provides internal access without exposing context counters or reservation mutation. No runtime primitive implementation or framing behavior changes.

This hook is required for a container allocation failure after a successful owned-payload primitive. Do not simulate allocation/resource failure by misusing an unrelated primitive. An ignored helper failure or later exception cannot cause complete publication or replace the first error.

## Acceptance and N7 closeout

Persistent tests cover real Parser/Fixer extraction and Parser destruction; deterministic three-output generation; evidence/shape/name rejection; planned sidecar collisions; root-only independent full vectors; sender schemas with declared extensions decoded by untouched root-only receiver schemas; sparse/trailing-zero bitmap width; opaque bytes/fragments; input destruction/deep copies/moves; nested cumulative budgets; actual allocation failure in framing/container/generator paths; sticky encode refusal and decode errors. Strict C++20, focused sanitizers, fresh runtime/typed regressions and distribution precede independent review and upload.

Native APER vectors from a separate sender schema provide independent cross-checks; they do not qualify whole NGAP messages. Existing frozen schemas are untouched. No benchmark or full-schema qualification. The frozen N7 plan has P1, P2 and P3 only: once P3 and its qualification/closeout checks pass, N7 is complete for this initial domain. Declared known additions/groups, opaque re-encoding and IOC/collections remain separately scoped future milestones, not invented N7-P4/P5 tasks.
