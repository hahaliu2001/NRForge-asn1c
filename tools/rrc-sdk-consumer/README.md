# Installed RRC public consumer campaign

See `tools/rrc-sdk/README.md` for generation and isolation commands. `prepare.py`
recompiles the original authenticated frozen modules with native pycrate 0.7.11,
checks stored P7 native semantic receipts and constructs full expected public
models through the qualification-only bridge. It prepares ordinary C++/CMake
files. Python/schema/generator are not used by the resulting consumer build.

The relocated campaign runs 64 root executables, 143 independent real cases and
inherited bounded negative checks. Public encode/decode uses the installed archive,
checks full owned models and exact bytes/bits, tests logical-bit truncation and a
zero-depth quota, and checks octets/contained_bits/exact completion. The separately
owned P7 compound fixture is not an installed real RRC root and is not shipped.
The mixed executable uses actual NGAP/F1AP/E1AP installed registry archives and
RRC public API together; it checks one populated independent setup wire from each
APER protocol and MIB. This is SDK coexistence evidence, not a new full protocol
wire qualification campaign.

The four-archive mixed executable requires LLVM `ld.lld` on `PATH` and links with
one linker thread to fit the constrained verification environment. For Ubuntu's
versioned LLVM package, prepend `/usr/lib/llvm-18/bin` to `PATH`. This changes only
consumer linking; installed archive contents and codec algorithms are unchanged.

`example/` is a small ordinary CMake MIB consumer that needs only NRForgeRRC:

```sh
cmake -S tools/rrc-sdk-consumer/example -B /new/mib-build \
  -DCMAKE_PREFIX_PATH=/path/relocated-rrc-install
cmake --build /new/mib-build
ctest --test-dir /new/mib-build --output-on-failure
```
