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
