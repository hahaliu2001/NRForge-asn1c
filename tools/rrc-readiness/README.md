# RRC-P1 read-only readiness evidence

See `../../docs/rrc-cpp-uper-readiness-and-batch-plan.md`. All source identities
are pinned in `source-manifest.json`; source files stay outside this repository.
Input root corresponds to NRForge-RAN `src`, with the six `rrc/asn1/*.asn` files.
The report does not contain generated C++ or wire qualification.

From a configured/built repository at the recorded baseline, compile the probe
(use your own scratch destination, not the checked-in report directory):

```sh
gcc -std=gnu11 -Wall -Wextra -Werror -DHAVE_CONFIG_H \
  -I. -Ilibasn1common -Ilibasn1parser -Ilibasn1fix -Ilibasn1typed -Itools \
  tools/rrc-readiness/probe.c tools/developer_tree.o \
  libasn1typed/.libs/libasn1typed_extract.a \
  libasn1typed/.libs/libasn1typed.a libasn1fix/.libs/libasn1fix.a \
  libasn1parser/.libs/libasn1parser.a libasn1common/.libs/libasn1common.a \
  -lm -o /tmp/rrc-readiness-probe
PYTHONDONTWRITEBYTECODE=1 python3 tools/rrc-readiness/scan.py \
  --asn1-root /path/to/frozen/src --probe /tmp/rrc-readiness-probe \
  --ast /tmp/rrc-fixed-ast.json --output tools/rrc-readiness/readiness.json --check
```

Omit `--check` only to intentionally record new evidence. Source drift or module
order drift fails before the probe runs. The probe has no renderer/wire calls;
its raw scratch JSON inventories the fixed AST before executing 22 existing API
probes. Count all named AMT_TYPE/AMT_TYPEREF declarations, including ordinary
aliases and the SetupRelease template, excluding value declarations. Features
include inline members and actual-parameter subtrees, excluding specialized
clones. The 12 declared channel selectors are reconciled with the fixed AST;
their payload references are module/import resolved and spare/empty branches
are retained. All 2,630 type identities and lines remain in the committed report.
This is not an instantiated reachable graph or evidence of generated coverage.

`fixtures-manifest.json` authenticates the four historical bare-value seeds;
it does not add, refresh or decode the external fixtures. `verification.json`
records checks performed in this batch. The independent review receipt records
the exact reviewed file snapshot, excluding itself.

## RRC-P2 ordinary root graph

The P1 files remain historical evidence. See
`../../docs/rrc-cpp-uper-p2-root-graph-contract-and-closeout.md` for the new API,
budgets, measured blockers and correction to the proposed MTC expectation.
`readiness-rrc-p2.json` is the deduplicated 80-root ordinary extraction matrix.
The probe deletes the fixed tree before serializing successful graphs; failed
roots have no partial graph. It invokes no renderer or wire codec.

Build `root_probe.c` with the same command above, replacing the source and output
binary names, then run:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/rrc-readiness/record_p2.py \
  --asn1-root /path/to/frozen/src --probe /tmp/rrc-root-probe \
  --work /tmp/rrc-p2-check --check
make -C libasn1typed check
```

The focused `check_asn1typed_root_graph` test is part of Automake's suite and
injects every allocation failure in its extraction path. It includes imported
aliases, same-named declarations in different modules, cyclic dependencies,
opaque contained payloads and failure rollback; it performs no generation.
The fixture uses the fixer's `A1F_COMPOUND_NAMES` for its deliberate name clash.

`check_aper_regression.py` uses a freshly linked
`tools/asn1typed_codec_coverage.c` probe and authenticates all three frozen APER
sources. It regenerates all 361 message BODY/envelope families and compares
available historical hashes (1773 headers). To reproduce it, compile that source
using the same include/library list, then run:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/rrc-readiness/check_aper_regression.py \
  --asn1-root /path/to/frozen/src --probe /tmp/aper-coverage-probe \
  --work /tmp/rrc-p2-aper-check --output /tmp/aper-regression.json
```

The APER regression work directory must be new. Its report records the current
extractor and baseline file hashes; it does not rebuild installed SDKs or rerun
their complete wire corpus. RRC generated compile and UPER remain NOT_RUN.
