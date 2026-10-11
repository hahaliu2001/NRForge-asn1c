# RRC-P15 inherited constructed alias contract

Baseline: fc2877dd5c52b52c3f8dbe7134ae50fc3192a28a.
Primary objective: preserve ordinary constructed alias inherited semantics.
Independent minimum-contract review confirms existing owned collection model is
sufficient; no new IR, renderer, runtime or schema.

MobilityHistoryReport-r16 ::= VisitedCellInfoList-r16 has no declared constraint.
The terminal VisitedCellInfoList-r16 declares SIZE(1..16); the fixer copies its
effective constraint into each alias. New declared restrictions on constructed
aliases, including an equal-looking SIZE, remain unsupported.

For a terminal SEQUENCE OF, parse clean supported SIZE using the existing owned
size representation. Every alias effective constraint must match all semantic
fields: presence, lower/upper, extensibility, known extension-addition presence
and lower/upper. The terminal declared/effective SIZE must also agree. Missing
inherited bounds, widening, narrowing, mixed/malformed and hidden intermediate
restrictions fail. An unbounded terminal and absent alias SIZE remain valid.
Other constructed kinds retain their existing constraint rejection. This is
inheritance recognition, not constructed subtype intersection/narrowing support.

Keep declaration module/name/location on the alias and terminal element identity,
bounds and inherited source location on owned members. Anonymous elements retain
the existing authenticated alias-context identity and equivalent terminal shape. Keep ordinary namespace,
reference cache, graph budgets and parameterized-use checks unchanged. No parser
pointer is retained and extraction does not mutate AST. P14 primitive Contents
behavior remains intact. Validate direct/alias wire agreement on focused fixtures,
owned lifetime, failed rollback and quotas; measure fresh readiness and shared
APER artifacts. Known explicit SIZE addition metadata remains owned but its
existing renderer rejection is preserved. Expanded real-message qualification and SDK delivery remain
P16-P18. Historical evidence is immutable. No benchmark or merge.
