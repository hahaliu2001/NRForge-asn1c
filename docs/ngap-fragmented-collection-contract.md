# Bounded fragmented SEQUENCE OF collections

Owner's continuous shared-capability batch authorizes this work. Frozen ASN.1
sources are not changed. This implements non-extensible SIZE constraints with
upper bound exactly 65536; existing upper bounds <=65535 keep their previous
constrained determinant and generated output. Bounds above 65536, extensible
SIZE and unbounded collections remain refused. This is not general PER support.

X.691 11.9 unconstrained length determinants apply when the bounded upper limit
is >=65536: counts below 128 have one aligned octet; 128..16383 have two;
fragmented determinants C1..C4 announce 1..4 units of 16384 elements. Each
segment's element payload immediately follows its determinant. A fragmented
segment always requires a following determinant after its payload, including a
zero final determinant for exact multiples. Determinants count elements, never
payload octets. The lower bound does not become a determinant offset here.

Runtime read/write_collection_segment primitives atomically check live/sticky
state, zero alignment, determinant validity, wire/output limits and shared
collection-element budget, then publish cursor/counters only after successful
preflight/allocation. Payload loops are composite operations; failure records
sticky state and complete wrappers never publish incomplete values. Decode
checks accumulated schema upper bounds before allocation and final lower bounds.
Zero-length final segments consume no additional element budget.

Generated tests use true Parser/Fixer/Owned IR extraction followed by deletion
of the parser, repeated deterministic generation and every C allocation failure
point. The generated codec covers 1/127/128/16383/16384/32768/65535/65536 elements,
compares complete independent-model bytes, retains owned values, checks exact
multiple terminator truncation, collection/output/wire limits and decode OOM.
Runtime tests cover alignment residues 0..7, nonzero alignment, sticky replay,
truncated determinants and determinant allocation/budget atomicity.

`tools/fragmented-collection-qualification/compare.py` compares saved actual
generated C++ bytes against asn1tools 0.167.0 APER encode/decode. Set
FRAGMENT_VECTOR_DIR to an existing directory when running the collection
script, then supply that directory to compare.py. The checked-in results cover
8 vectors in both directions. Independent reference and focused synthetic
checks do not constitute complete NGAP-PDU qualification.

Execution evidence: libasn1typed check 34/34; libaper check 7/7; strict C11
renderer and C++20 generated code checks passed. Runtime and generated collection
checks passed ASan+UBSan with leak detection disabled; no LSan claim is made.
Tools distribution includes the reference comparator and its result manifest.
