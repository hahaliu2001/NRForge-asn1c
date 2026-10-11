# RRC-P13-B late specialization CHOICE verification

```sh
make -C libasn1fix
make -C libasn1typed check
python tools/rrc-late-choice/check.py --asn1-root /path/to/frozen/schema \
  --work /new/work/directory --output tools/rrc-late-choice/readiness.json
```

Rebuild libasn1typed after libasn1fix: its convenience archive bundles fixer
objects. The driver recompiles current probes, binds current source and archives,
authenticates all six frozen schema files, verifies all ten named instance
contexts, compares the 74 P13-A RRC headers, compiles newly rendered roots, and
compares all 361 APER messages/1773 historical physical headers.

The native check_asn1typed_late_choice test checks module-order identical output,
template-owner modes, nested clone and inline CHOICE, explicit root/member tags,
extensions, root CHOICE requiring EXPLICIT mode, generic-body immutability,
owned lifetime, three fixer errors and unresolved actual extraction rejection.
The legacy fixer itself can accept unresolved actuals; typed extraction cannot.
Seven generated fixture roots exercise fourteen hand-derived goldens under all
three completion modes, with constraints, missing branches, truncation and quota
failures. ASan/UBSan directly instrument the current fixer scheduling and tag
helpers plus the focused test/extractor; leak scanning is unavailable here.

Current readiness uses text-rendering PASS separately from strict compilation.
LocationMeasurementIndication retains P13-A's known renderer unused-io compile
block with its byte-identical header; this task does not repair that renderer.
SDK delivery stays at 64 roots. New real-message wire qualification and SDK
expansion remain P16-P18 work. No benchmark or merge is performed.
