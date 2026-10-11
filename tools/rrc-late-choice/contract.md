# RRC-P13-B tagging contract (pre-implementation)

Baseline: 698ba246456d785a6c72ab3e39c0a821813e767c.
Primary objective: finalize tags on late-created fixed parameterized CHOICEs.

The fixer processes phase I per module. A later module can instantiate an earlier
module's template after its constructed-tag decision and automatic-tag pass.
The minimal Templates/Consumer fixture reproduces fix success with untagged
Pick<Consumer.Local> and NestedChoice<Consumer.Local>, auto_tags_OK=0. Phase II
does not repeat tag decisions. The generic template is intentionally untouched.

After all normal fixer phases, visit fixed-specialization trees under their
owning template module's namespace. For CHOICE bodies with auto_tags_OK=0,
validate the CHOICE root with asn1f_fix_constr_tag(...,1), then
reuse asn1f_fix_constr_tag(...,0) and asn1f_fix_constr_autotag. Preserve explicit
root tags and module default rules: no unconditional automatic tagging, no
release/setup or source-order wire indexes. Include nested CHOICE bodies within
fixed-specialization trees, but do not finalize generic source bodies or broaden
the pass to SEQUENCE/SET. Already automatic-tagged CHOICEs remain unchanged.

Tag lookup may create additional specializations. Repeat completion while the
global specialization count grows, prewarm each member using the same outmost-tag lookup and AFT_IMAGINARY_ANY
flags as distinct validation (lack of a single outer tag is not an error), then
perform existing distinct-tag validation
on all fixed-specialization CHOICE trees. If validation creates new instances,
complete/validate again until the count stabilizes. Tag errors fail the fixer;
malformed implicit CHOICE tags, duplicate explicit tags and unresolved references
remain unusable. The legacy fixer can permit an unresolved actual; existing
owned extraction still rejects it, and completion does not relax that check.
No extractor, public IR, schema, renderer, runtime or SDK changes.

Verify module order, imported and module-qualified actual identities, explicit
and implicit defaults, mixed explicit/automatic tags, extension partitions,
nested actual CHOICE requiring EXPLICIT automatic mode, nested specialization
and inline CHOICE, generic template immutability, already fixed evidence and
AST-destroyed owned metadata. Fresh 85-root matrix preserves all 74 P13-A rendered
headers; LocationMeasurementIndication's existing strict compile block stays
explicit. All 361 messages / 1773 APER headers and relevant native fixer/compiler
regressions must pass. New real-message wire qualification and SDK expansion
remain separate P16-P18 batches. No benchmark or merge.
