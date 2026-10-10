# Shared batch: extensible BIT/OCTET SIZE and effective use-sites

Owner continuous execution authority selects shared capabilities 2 and 4.
Baseline: c06613a7e9693e68d4ffbea011347fd0cb111fb7; frozen NGAP source unchanged.

## Contract

The opt-in `asn1typed_render_cpp_owned_size_{types,mapping,codec}` APIs lower
owned inline ENUMERATED fields and permit bounded extensible BIT/OCTET STRING
SIZE roots. Historical ordinary shape/value/bit APIs retain their rejection
boundaries. Physical IOC generation adopts this capability only when required.
Unchanged successful schemas retain identical generated output.

A scalar owned SIZE extension-addition interval preserves the frozen
`SIZE(8, ..., 16)` evidence. General unions, multiple addition intervals and
unrepresented constraints remain rejected. Named use-sites preserve identity;
closed intersections with an extensible declared root are accepted only when
provably root subsets. Two extensible intersected roots must match exactly.
No root range or addition evidence is dropped to make extraction succeed.

The runtime optional `extensible` parameter defaults false. When true, the
root/extension selector, alignment, determinant, payload and owned allocation
are one atomic operation. In-root lengths use the existing bounded rules;
outside-root lengths use unconstrained BIT/OCTET length units. Encode chooses
root when possible; decode refuses an extension encoding of an in-root length.
Sticky errors, logical cursor and budgets retain the old contract. Extension
lengths 0 through 16383 are supported; lengths/determinants requiring
fragmentation explicitly return `resource_limit`, not a fabricated schema
constraint violation. A zero-length extension is valid when outside the root.

## Evidence

* 384 complete independent bit-model vectors cover BIT and OCTET STRING,
  root/extension branches, offsets 0..7, fixed 8/16 and range 1..160/0..256,
  zero extension and length determinant boundaries 127/128/16383.
* Real Parser/Fixer → Owned IR → Parser deletion → generated three-header
  tests retain exact SIZE additions, effective named SIZE and CHOICE use-sites.
  Repeated output and wrapped allocation failures are checked.
* 256 native/model/runtime vectors cover BIT framing. **asn1tools 0.167.0
  cannot encode actual BIT STRING extension branches.** Original extensible
  root schemas are checked directly; extension cases use an explicit BOOLEAN
  selector + unconstrained BIT STRING surrogate. This is transparent native
  primitive framing evidence, not full extensible-schema qualification.
* Six frozen physical graphs are inspected. All five former use-site SIZE
  first-failure messages now generate all three families: HandoverRequired,
  PWSFailureIndication, PWSRestartIndication and both RIM transfers. Their
  actual use-sites are TNGF-ID/TWIF-ID SIZE(32,...) and W-AGF-ID SIZE(16,...).
  DownlinkNASTransport extraction now preserves primaryRATRestriction's
  SIZE(8,...,16), but a subsequent missing physical reference still prevents
  codec generation. No complete NGAP/PDU qualification is claimed.

Focused runtime ASan/UBSan, strict C11 renderer compilation and generated
C++20 compilation pass. LSan must be reported from the aggregate environment,
not inferred from an ASan run with leak detection disabled. The aggregate
integration replays evidence after shared production sources are merged.

## Follow-up: bounded wide BIT SIZE exposed by the integrated graph

The frozen `NGAP-IEs.asn` DRBStatusUL18 declaration at lines 1705–1709 has
`receiveStatusOfUL-PDCP-SDUs BIT STRING (SIZE(1..131072)) OPTIONAL`.
After preceding shared capabilities were integrated, this became the first
remaining effective SIZE failure in Downlink/UplinkRANStatusTransfer. This
follow-up expands only the existing owned-size/physical IOC capability path
for nonextensible BIT STRING roots whose upper bound is 65536 through 131072.
Named declarations, inline members and effective named use-site constraints
retain their actual lower/upper bounds. Historical shape/bit/runtime entry
points keep their old domain and output boundaries; OCTET STRING and extensible
BIT roots above 65535, and any upper bound above 131072, still fail closed.

The new `read_bit_string_owned_fragmented_size` and
`write_bit_string_fragmented_size` primitives align before framing, encode
actual bit counts using unconstrained length determinants, and interleave each
fragment determinant with its BIT payload. Fragment multipliers 1–4 represent
16384–65536 **bits**, not octets. The encoder uses maximal permitted multiples;
the decoder rejects nonmaximal splitting, malformed multipliers and overlong
short determinants. Exact multiples require a terminal zero determinant.
No extension selector is present for this nonextensible root. Payloads end at
the last logical bit; complete-value final padding stays at the outer wrapper.

The complete primitive scans and preflights all determinants and payloads
before owned allocation or output/state publication. Failure preserves cursor,
wire counters and logical output length, and enters the existing sticky state.
Allocation failure, input/output/wire limits, truncated fragments and absent
terminal determinants are checked. Zero is valid only when the supplied lower
bound is zero; the frozen 1..131072 domain rejects it.

`check_fragmented_bits.cpp` compares 80 independent model vectors at all eight
prefix offsets, including 0/1/127/128/16383/16384/65535/65536/131071/131072 bits.
The owned generator fixture covers named, inline and effective named use-site
framing after parser-tree destruction, deterministic generation and allocation
failure. `tools/batch-fragment-bits-qualification` compares 152 actual native
ASN.1 aligned-PER vectors (304 encode/decode comparisons) with the model/runtime.
Unlike the earlier extensible BIT evidence, this uses the **actual** bounded
nonextensible BIT schemas; there is no extension surrogate. It qualifies the
bounded primitive, not a complete NGAP PDU or all frozen message codecs.
