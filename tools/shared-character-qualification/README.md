# Shared character qualification

Build the configured repository test drivers, then replay:

```sh
make -C libasn1typed check_asn1typed_character_render check_asn1typed_ioc_render
python3 -m pip install -r tools/shared-character-qualification/requirements.txt
python3 tools/shared-character-qualification/qualify.py
```

The script regenerates ordinary and physical IOC headers from owned extraction,
compiles the generated C++ under strict C++20, and runs generated helper-backed
vectors before native comparison. All 395 complete encodings have independent
bit-builder expectations. Of these, 383 are checked against asn1tools in both
encode and decode directions. The remaining 12 known-multiplier out-of-root
SIZE values are model-only because asn1tools 0.167.0 explicitly does not implement
those extensions. UTF8String SIZE does not add a PER extension bit and is
included in native comparisons, including multibyte scalar-size examples.

The summary records input and generated-header hashes. Regenerate it whenever
shared production files are merged or changed. This evidence qualifies the
bounded character capability, not whole NGAP interoperability.
