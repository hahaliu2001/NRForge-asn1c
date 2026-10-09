# S2 — Synthetic APER Wire Mapping Contract Study

## Status and scope

This is an S2 study and approval candidate for the accepted synthetic slice. It records a hand-derived wire mapping; it does not qualify an encoder or decoder. The frozen schema is `CppAperSlice DEFINITIONS AUTOMATIC TAGS` with `Count ::= INTEGER (0..65535)`, `Selection ::= CHOICE { flag BOOLEAN, count Count }`, and `Packet ::= SEQUENCE { count Count, enabled BOOLEAN OPTIONAL, selection Selection }`.

The mapping below is for **basic aligned PER (APER)**. “Bit 0” means the most significant bit of the first octet; bit strings and non-negative binary integers are written most-significant bit first. `Count` has 65,536 permitted values. Its offset is `value - 0`; since `range = ub - lb + 1 = 65536 = 64K`, X.691 11.5.7.3 selects a two-octet, octet-aligned field. The field is the offset encoded as a two-octet non-negative binary integer, most-significant octet first. Every 16-bit pattern represents one permitted offset. Input 65536 is outside the ASN.1 constraint and is an encode input constraint failure, not a special wire pattern.

## Standards basis

The normative sources used here are freely accessible official ITU-T texts:

| Subject | Edition and provisions | Official source |
| --- | --- | --- |
| APER/PER rules | ITU-T X.691 (02/2021), especially 11.1 (complete encoding and alignment), 11.3, 11.5.3–11.5.7 (constrained whole numbers), 12 (BOOLEAN), 13 (INTEGER), 19.1–19.4 (SEQUENCE), 23.1–23.8 (CHOICE) | [X.691 official PDF](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I%21%21PDF-E&lang=e&type=items); [official recommendation record](https://www.itu.int/rec/T-REC-X.691-202102-I/en) |
| Module default, automatic tags, tag order | ITU-T X.680 (02/2021), 8.5–8.6 (canonical tag order), 13.2–13.3 (TagDefault and automatic tagging), 25.3, 25.8–25.10 (SEQUENCE tags), 29.2–29.5 (CHOICE tags) | [X.680 official PDF](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.680-202102-I%21%21PDF-E&lang=e&type=items); [official recommendation record](https://www.itu.int/rec/T-REC-X.680-202102-I/en) |

X.691 11.5.3 defines `range = ub - lb + 1`; 11.5.7 divides aligned encodings into bit-field, one-octet, two-octet, and indefinite-length cases. 11.5.7.3 specifies the two-octet case for range 257 through 64K. X.691 11.1.4 says octet-aligned fields are preceded by zero to seven zero bits as needed; 11.1.4 also requires zero padding at the end of a complete outermost aligned encoding. X.691 12.1–12.3 encodes BOOLEAN as one bit, true `1`, false `0`. X.691 13.2.2 applies the constrained whole-number procedure to a constrained INTEGER.

X.680 13.3 makes automatic tagging a syntactical transformation. X.680 29.2 and 29.5 assign context-specific tags `[0]`, `[1]`, in order, to these untagged root CHOICE alternatives. X.680 8.6 orders context-specific tags by ascending tag number; X.691 23.2 uses that canonical order for CHOICE indexes. Thus for this schema `flag` has effective tag `[0]`, `count` has `[1]`, and their PER root indexes are respectively 0 and 1. X.680 29.3 states the distinct-tag condition; 29.5 specifies the automatic tag numbers. These effective tags identify the reason this exact fixture's source order corresponds to PER order; that coincidence is not a general IR guarantee.

## Storage to wire mapping

| ASN.1/storage value | Wire meaning | Alignment and width |
| --- | --- | --- |
| `Count = v` (`uint64_t` plus owned range 0..65535) | unsigned offset `v`; 16-bit big-endian bit field | Align to next octet, then 16 bits; no length determinant |
| `Selection_flag{b}` / `variant` holding `flag` | root index 0, then BOOLEAN `b` | One selector bit, then one BOOLEAN bit; neither field itself octet-aligned |
| `Selection_count{v}` / `variant` holding `count` | root index 1, then `Count(v)` | One selector bit; align payload to octet, then 16 bits |
| `Packet.count` | first SEQUENCE component | After preamble, align to octet, then 16 bits |
| `Packet.enabled = nullopt` | OPTIONAL presence bit 0; no BOOLEAN value field | Preamble bit; no content bit |
| `Packet.enabled = false/true` | presence bit 1; later BOOLEAN content bit 0/1 | Preamble bit then one content bit after `count` |
| `Packet.selection` | encoded after present root components, in SEQUENCE component order | CHOICE mapping above |

The C++ wrapper type names and `std::variant` storage index are not a wire contract. The fixture-specific mapping is `Selection_flag -> PER index 0`, `Selection_count -> PER index 1`; this must be selected from owned wire metadata in a future implementation. No extension bit occurs in `Selection` (X.691 23.5 says none without a marker) or in `Packet` (X.691 19.1 says none without a marker).

## Count vectors

Standalone `Count` is an outermost complete value. Since it starts octet aligned, there is no leading or trailing padding beyond its 16-bit field. In a containing value the same 16 value bits follow the required octet alignment at that position. Octet order is big-endian.

| Input | Permitted-value offset | 16-bit field | Standalone hex | Length |
| ---: | ---: | --- | --- | ---: |
| 0 | 0 | `0000000000000000` | `00 00` | 2 octets |
| 1 | 1 | `0000000000000001` | `00 01` | 2 octets |
| 255 | 255 | `0000000011111111` | `00 FF` | 2 octets |
| 256 | 256 | `0000000100000000` | `01 00` | 2 octets |
| 65535 | 65535 | `1111111111111111` | `FF FF` | 2 octets |

The full 16-bit domain maps exactly onto the full constrained domain. There is no in-range “overflow” bit pattern.

## Packet bit layout and golden vectors

For `Packet`, X.691 19.2 places the single OPTIONAL presence bit at the beginning of the SEQUENCE preamble. The first and only bitmap bit represents `enabled` (`1` present, `0` absent). The following components are encoded in declaration order. Consequently the layout starts with the presence bit, then seven zero alignment bits, then the 16-bit `count`. If enabled is present, its BOOLEAN value follows `count`; then comes the CHOICE selector and branch payload. The selector has range 0..1 and is one bit, with no alignment of the selector itself. The `flag` branch contributes one BOOLEAN bit. The `count` branch aligns its payload to an octet boundary after the selector.

Vectors use `Packet.count = 1`; `Selection.flag = TRUE` or `Selection.count = 255` so both payload branches are visible. `E` is the preamble bit, `C` the 16-bit Packet count field, `B` the optional BOOLEAN content when present, `I` the one-bit CHOICE index, `F` the flag BOOLEAN, `p` intermediate zero alignment, and `z` final complete-encoding zero padding. Bit groups shown by octet are separated with spaces. These are manually derived vectors, not independently checked codec outputs.

| `enabled` | Selection | Fields, in order (including padding) | Final octets | Hex | Length |
| --- | --- | --- | --- | --- | ---: |
| absent | `flag = TRUE` | `E0 · p0000000 · C0000000000000001 · I0 · F1 · z000000` | `00 00 01 40` | `00 00 01 40` | 4 |
| present `FALSE` | `flag = TRUE` | `E1 · p0000000 · C0000000000000001 · B0 · I0 · F1 · z00000` | `80 00 01 20` | `80 00 01 20` | 4 |
| present `TRUE` | `flag = TRUE` | `E1 · p0000000 · C0000000000000001 · B1 · I0 · F1 · z00000` | `80 00 01 A0` | `80 00 01 A0` | 4 |
| absent | `count = 255` | `E0 · p0000000 · C0000000000000001 · I1 · p0000000 · C0000000001111111` | `00 00 01 80 00 FF` | `00 00 01 80 00 FF` | 6 |
| present `FALSE` | `count = 255` | `E1 · p0000000 · C0000000000000001 · B0 · I1 · p000000 · C0000000001111111` | `80 00 01 40 00 FF` | `80 00 01 40 00 FF` | 6 |
| present `TRUE` | `count = 255` | `E1 · p0000000 · C0000000000000001 · B1 · I1 · p000000 · C0000000001111111` | `80 00 01 C0 00 FF` | `80 00 01 C0 00 FF` | 6 |

The `count` branch's constrained payload values, like standalone `Count`, are two octets in most-significant-octet-first order. `enabled` absent omits its value bit entirely; present false and present true both include a presence bit of 1 in the preamble and then distinct content bits 0 and 1. This separates absence from a present false value.

### Complete-encoding boundary

For standalone `Count`, the field ends at bit position 16 and the result is 2 octets. For Packet flag vectors, the final field bit position is 26 when enabled is absent and 27 when it is present; 6 or 5 zero bits respectively complete the outermost value to an octet boundary. For Packet count-branch vectors, the final payload ends at bit position 48 in all three enabled states; no additional final bits are needed. Intermediate alignment is distinct from final padding: it is inserted only before an octet-aligned field, while complete outermost APER appends zero bits at the end as required by X.691 11.1.4.

The decoder's padding acceptance, truncation error details, and trailing-data policy are project decisions, not established by these value mappings. A future decoder contract should decide whether it validates required alignment/final padding as zero, how it reports an incomplete field, and whether a caller must supply exactly one complete value or may provide trailing bytes. This study does not implement those policies.

## Parser, Fixer, and Owned IR evidence

The extraction entry point `asn1typed_extract_module()` walks the parser/fixed tree's module members and calls `populate_type()`; the parser module has `module_flags`, including `MSF_AUTOMATIC_TAGS`, and expressions carry tag-related state. The owned public IR in `libasn1typed/asn1typed.h` owns type identity, source order, field presence, CHOICE alternative names/references, constraints and an `is_extensible` flag. It has no module tag-default property, no effective-tag record per alternative, and no canonical-order/index field. Extractor copies CHOICE alternatives in encountered member order. Once the parser tree is deleted, only the owned IR survives; parser/Fixer evidence is gone unless separately copied.

| Mapping evidence | Grade | Finding |
| --- | --- | --- |
| `Count` bounds 0..65535; 16-bit payload width and range arithmetic | A — Owned IR directly proves bounds; X.691 supplies width rule | `value_range` is owned and numeric. The rule is generic and applies to these values. |
| OPTIONAL `enabled` presence and field order | A — Owned IR directly proves | `presence` and ordered `fields[]` are owned. |
| Packet component encoding order | A — Owned IR directly proves source component order; X.691 19.4 encodes components in turn | `fields[]` preserves extraction order. The OPTIONAL bit is also explicitly represented. |
| BOOLEAN bit value convention | A — Owned primitive kind proves type; X.691 supplies rule | BOOLEAN payload truth value maps to one bit by standard. |
| Absence of SEQUENCE/CHOICE extension bits | A — Owned IR directly proves | `is_extensible` is owned; it is false for both fixture constructs. |
| Module `AUTOMATIC TAGS` default | B — Schema / Parser/Fixer only | Parser module flags retain it during extraction. `asn1typed_module_t` does not own it. |
| Effective CHOICE tags `[0]` / `[1]` | B — Schema / Parser/Fixer only | Schema plus X.680 proves them; fixed AST may retain tag data, but extractor does not copy effective tags. |
| Canonical tag order and resulting PER indexes | B — Schema / Parser/Fixer only | Can be proven for this fixture while schema/tree evidence remains available. Alternative source order alone is not proof in the general case. |
| Evidence after Parser/Fixer destruction | C — Current Owned IR missing | No module tag mode, effective alternative tags, or canonical PER order survives. |
| Generic storage alternative to PER index mapping | C — Current Owned IR missing | Do not infer `variant.index()` or current source order as universal PER index. |

### S2-P1 closeout status

The table above records the study-time evidence gap. S2-P1 subsequently added owned module tag-default state, per-alternative effective-tag evidence, and an independent PER root index through the real Parser → Fixer → Owned IR extraction path. It was completed and passed independent review in commit `5805694d79fca295c4b805ff3f81b5896a2230eb`. The current supported scope is non-extensible CHOICE with resolved outer tags. Inherited tags, complex tag resolution, or missing/unsupported evidence still cannot produce a valid mapping. This work does not establish a general-purpose ASN.1 tag parser and does not qualify an APER codec. The historical gap above remains accurate for the study's original inspection point.

### Minimal prerequisite

At study time, the evidence gap required one narrowly scoped Owned IR prerequisite: own the module's tag-default state and each CHOICE alternative's effective tag identity plus its computed canonical PER root index, with no borrowed Parser/Fixer pointers. S2-P1 completed this prerequisite for the supported scope described above, using the real Parser → Fixer path and fail-closed validation. The historical table remains as the study-time finding; this document closeout makes no IR or extractor changes.

## Approved decisions

Owner approval recorded on **2026-10-08**.

| Decision | Accepted contract | Status |
| --- | --- | --- |
| S2-01 | The initial synthetic codec target is basic aligned PER under ITU-T X.691 (02/2021). | Owner Approved / Accepted |
| S2-02 | Bounded `Count (0..65535)` encodes as the 16-bit big-endian offset, octet-aligned at its use position; out-of-domain values fail before emission. | Owner Approved / Accepted |
| S2-03 | CHOICE indexes follow X.680 canonical effective-tag order; implementation reads owned order metadata, never assumes storage/variant index. | Owner Approved / Accepted |
| S2-04 | For this slice, encode the OPTIONAL presence bitmap first, then encode components in SEQUENCE order; it has one presence bit and no extension bit. DEFAULT is outside this round's implementation scope. | Owner Approved / Accepted |
| S2-05 | Intermediate APER alignment and complete outermost zero padding follow X.691 11.1.4; decoder validation, truncation diagnostics, and trailing-data handling require a separate project decision. | Owner Approved / Accepted |
| S2-06 | The S2-P1 owned tag/order prerequisite is complete for its supported scope; use that evidence for mapping and reject inherited, complex, missing, or unsupported tag evidence. | Owner Approved / Accepted |
| S2-07 | Golden vectors in this document are review aids only until checked against an independent implementation; they do not qualify a codec. | Owner Approved / Accepted |

## Future implementation boundary

Under this approval, implementation can use the completed S2-P1 evidence for this synthetic slice's `Count`, `Selection`, and `Packet` mapping, within the supported tag-evidence boundary above. This study does not authorize changes to the S1 renderer, Naming, existing tests, IR, extractor, or C runtime; it does not implement APER runtime, encode/decode, DEFAULT, IOC, extensions, or real NGAP codec behavior. No vector here has been independently qualified.
