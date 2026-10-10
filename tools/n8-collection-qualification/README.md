# N8 bounded collection native APER qualification

The driver uses actual generated collection helpers on one runtime context, prefixed by 0–7 BOOLEAN bits. It compares full bytes and lengths and all decoded elements. The Python runner compiles a separate sender schema with pycrate 0.7.11 and asn1tools 0.167.0 native aligned PER; temporary wrapper definitions exercise count primitives at every bit residue. Generated namespaces are `collectiontest`. No native output is normalized, no receiver schema is filtered, and production generation has no fixture-name special cases.

## Reproduce

Install pinned dependencies outside the repository, then use the configured build containing `check_asn1typed_collection_render`:

```sh
python -m pip install -r tools/n8-collection-qualification/requirements.txt
python tools/n8-collection-qualification/qualify.py --repo /absolute/source/repo --build /absolute/build --output /outside/repo/n8-result.json
```

Alternatively pass `--generated /absolute/header/directory` containing types.hpp, mapping.hpp and codec.hpp produced from `collection-generation-n8.asn1` with namespace `collectiontest`. `--cxx 'clang++'` selects the compiler. The runner only compiles a scratch driver/runtime; it does not rebuild the repository. Generated headers, native oracle code and binaries live in a temporary directory.

`accepted-profile.json` records every matching case ID and exact compact mismatch signatures. A changed case ID or signature fails even if aggregate totals stay equal; observations are written for inspection. A zero-unresolved result is emitted only after exact comparison. Dependencies must match the pinned versions; upgrading requires reviewing the profile rather than accepting changed oracle behavior automatically.

## Observed evidence

| Native oracle | Normal cases attempted | Exact bidirectional matches | Explicit oracle discrepancies | Unknown-extension receive cases |
|---|---:|---:|---:|---:|
| pycrate 0.7.11 | 1128 | 1128 | 0 | 48 |
| asn1tools 0.167.0 | 1128 | 1122 | 6 | 48 |

The six asn1tools discrepancies are the three repeated pattern cases of standalone fixed `SIZE(0)` and the three cases of fixed three zero-bit SEQUENCE elements. Native asn1tools emits empty bytes rather than the `00` complete-encoding substitution required by X.691 clause 11.1. The receiver correctly reports `truncated_input@0`. Pycrate and generated encoding emit `00`. The empty native bytes are retained as an explicit discrepancy, never replaced with `00`.

Coverage includes BOOLEAN/vector<bool>, named INTEGER, SEQUENCE and CHOICE elements, nested collections, positive lower bound 1..2, exact SIZE 0/3, zero-bit elements, cardinalities 255/256/257/65536 and maximum count 65535, all prefix residues, optional element fields, and root-only extensible SEQUENCE elements. Separate sender-declared additions in `ExtPair` are decoded by untouched root-only receiver elements and checked for root values, exact bitmap width, addition index and retained raw payload bytes. Those opaque sidecars are not re-encoded.

Normative reference: [ITU-T X.691 (02/2021)](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-X.691-202102-I!!PDF-E&lang=e&type=items), clauses 20.2/20.5/20.6, 11.9.3.3, 11.5.7 and 11.1. Collection lengths count elements and add no alignment beyond their constrained determinant; element primitives keep their own alignment.

This tool qualifies generated success vectors and owned extension decoding. Runtime error priorities, sticky failures, resource budgets and allocation failures are checked by the focused repository tests, not inferred from successful differential cases. No fragmented/unbounded/extensible collection, opaque extension encoding, complete NGAP qualification or benchmark claim is made.
