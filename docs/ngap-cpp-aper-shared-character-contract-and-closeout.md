# Shared character-string capability contract and closeout

This capability belongs to the Owner-authorized continuous shared-capability batch following N18. It does not change frozen TS 38.413 V18.10.0 ASN.1 sources or claim complete NGAP wire qualification.

## Actual schema evidence

Frozen NGAP declares AMFName and RANNodeName as PrintableString SIZE(1..150,...). The associated extended-name structures also reference VisibleString and UTF8String with the same SIZE root. The implementation handles all three owned primitive kinds rather than assuming the first PrintableString failure is the complete dependency set.

## Wire and storage contract

Each named character type stores an owned `std::string`. PrintableString is restricted to its 74-character ISO 646 repertoire; VisibleString accepts ASCII 32..126. UTF8String accepts well-formed Unicode scalar sequences, rejects overlong forms, surrogate code points, invalid continuation sequences and code points above U+10FFFF. UTF8 SIZE counts decoded scalars, not octets.

[ITU-T X.691 (02/2021)](https://www.itu.int/rec/T-REC-X.691-202102-I/en), clauses 30.4, 30.5.2–30.5.7 and 30.6, supplies the wire rules:

- Printable/VisibleString are known-multiplier strings. Their aligned character width is eight bits, and the code point itself is encoded because the repertoire's largest value fits in that width. This is not a dense permitted-alphabet index.
- Their extensible SIZE prefix is zero for a root length, one for an out-of-root length. Root lengths use the constrained whole-number determinant. Extension lengths use the unconstrained determinant. Variable root payloads align when the upper size times eight is at least 16; fixed payloads align only when that product exceeds 16. Fixed SIZE 2 and variable SIZE 1..2 consequently differ.
- UTF8String is not a known-multiplier string. SIZE and its extension marker do not affect PER framing: the UTF8 octets are encoded as an unconstrained OCTET STRING. SIZE is still validated semantically against the scalar count; an extensible SIZE accepts values outside its root.
- Root SIZE bounds are representable closed nonnegative intervals up to 65535. Known-multiplier extension payloads and UTF8 unconstrained payloads must contain fewer than 16384 units in this implementation. A fragmented determinant is refused as a resource limit. This operational limit is not written back as a schema constraint.
- Extra permitted-alphabet constraints, unconstrained declarations without owned SIZE evidence, other character primitive kinds and character use-site constraints remain refused.

## Integration and failure semantics

The ordinary `asn1typed_render_cpp_owned_character_{types,mapping,codec}` APIs explicitly opt into the new capability. Older ordinary shape/value APIs retain their support boundary. Physical IOC generation adopts the capability. Headers include `<string>` only when a character type is actually present; existing successful schemas without these types receive unchanged output.

The runtime `CharacterStringKind` selects a single `write_character_string` / `read_character_string_owned` primitive. It validates live/sticky state before arguments or value checks. Length, alignment, payload, repertoire/Unicode validation and allocation publish atomically. Decode rejects malformed input without advancing cursor or wire counters; complete wrappers publish values only after padding and trailing-data checks. UTF8 decode reuses the existing octet-frame primitive and rolls back its cursor/counter publication if subsequent Unicode validation or string allocation fails. Known-open child accounting continues to use the existing child transaction.

All declarations, references and collision checks reuse the existing final naming implementation. The generated mapping distinguishes UTF8's non-PER-visible SIZE from known-multiplier SIZE. `bits_per_character == 0` denotes UTF8's variable-width Unicode characters rather than claiming eight bits per scalar.

## Validation evidence

- True Parser → Fixer → owned extraction; Parser tree deleted before generation; repeated types/mapping/codec output compared byte for byte.
- The generated/runtime C++ test compares 395 complete encodings with a separate bit-builder model and checks decode values. It covers the complete Printable and Visible repertoires, prefix positions, length boundaries 0/1/2/127/128/150/151/16383, fixed 2 versus variable 1..2, constrained determinant alignment, UTF8 multibyte scalars, and UTF8 scalar SIZE versus octet length.
- `tools/shared-character-qualification/qualify.py` builds and runs the actual generated C++ codecs. Against asn1tools 0.167.0, 383 root/UTF8 vectors agree in both encode and decode directions. Twelve known-multiplier out-of-root extension vectors are model-only because that reference explicitly raises `NotImplementedError` for string SIZE extensions. The report does not count these as native agreement.
- A real physical IOC fixture includes Printable/Visible/UTF8 payload fields, generates after parser destruction, checks deterministic output and allocation failures through the existing IOC driver, and compares the complete independent vector `0001005B0009E0004100004002C3A9`; decode checks all fields.
- Refusals check missing/invalid metadata, unsupported size, invalid namespace, invalid repertoire, malformed UTF8, fragmentation threshold, truncation, nonzero alignment, budgets and sticky failure. Custom REQUIRE checks remain active under NDEBUG.
- Strict C11 generator warnings and strict C++20 generated/runtime warnings pass. Full typed checks pass 35/35 and runtime checks pass 7/7. Focused ASan/UBSan generated/runtime checks pass. Actual writer allocation failure and both UTF8 decode allocation failures preserve cursor and counters. Finished/moved-from operations and known-open UTF8 rollback are explicitly tested. LeakSanitizer was attempted with detect_leaks=1 but encountered a fatal proc/ptrace attachment error, so no usable LSan result is claimed.

The reference report is replayable from the repository after its C test drivers have been built. Its fingerprints must be regenerated after shared production files are merged with the other capability branches.

## Unconstrained URI-address follow-up

The final combined scan revealed `URI-address ::= VisibleString` without SIZE in four additional messages. Absent SIZE is now represented explicitly by `unconstrained = true`, not by a synthetic SIZE(0..0) constraint. PrintableString and UTF8String without SIZE use the same opt-in owned representation. Generated calls select an explicit runtime mode; prior constrained call text and defaults remain unchanged. The mode has no extension selector, aligns its one/two-octet length determinant, then emits validated characters as octets. UTF8 validates scalars while its wire determinant counts octets. Bounds arguments must be zero and extensibility false in this mode. Fragmented lengths >=16384 remain a resource refusal, including decode fragmentation determinants.

The replay retains all 395 earlier model vectors and adds 32 generated unconstrained cases: 427 independent model vectors, 415 native encode/decode comparisons, 12 extension model-only cases. New cases include the exact URI-address declaration, URI text, zero length, 127/128 determinant boundary, maximum supported 16383 length, prefixed alignment, multibyte UTF8, and fragmentation/charset rejection. Reference version is asserted as asn1tools 0.167.0 and source fingerprints cover core IR, extraction, naming and inline-enum delegation. These are capability checks rather than complete NGAP qualification.
