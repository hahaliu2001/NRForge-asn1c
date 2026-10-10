
ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@
CODE_COVERAGE_IGNORE_PATTERN="tests/*" "lex.yy.c" "y.tab.c" "asn1p_l.l" "asn1p_y.y"

SUBDIRS =                       \
    libaper libasn1common libasn1typed libasn1parser \
    libasn1fix libasn1print     \
    libasn1compiler             \
    asn1-tools asn1c tools skeletons  \
    tests examples doc

docsdir = $(datadir)/doc/asn1c

docs_DATA = README.md INSTALL.md REQUIREMENTS.md FAQ ChangeLog BUGS .github/copilot-instructions.md

EXTRA_DIST = README.md INSTALL.md REQUIREMENTS.md FAQ LICENSE BUGS .github/copilot-instructions.md

EXTRA_DIST += tools/batch-fragment-bits-qualification/driver.cpp tools/batch-fragment-bits-qualification/qualify.py tools/batch-fragment-bits-qualification/README.md tools/batch-fragment-bits-qualification/qualification-summary.json tools/batch-fragment-bits-qualification/source-evidence.json

EXTRA_DIST += docs/ngap-cpp-aper-autonomous-execution-authority.md docs/ngap-cpp-aper-batch-size-contract.md docs/ngap-cpp-aper-private-key-batch-contract.md docs/ngap-cpp-aper-shared-character-contract-and-closeout.md docs/ngap-cpp-aper-shared-integer-batch-contract-and-closeout.md docs/ngap-cpp-aper-shared-null-contract-and-evidence.md docs/ngap-fragmented-collection-contract.md docs/ngap-cpp-aper-shared-capability-batch-closeout.md
EXTRA_DIST += docs/ngap-cpp-aper-full-pdu-qualification-contract.md docs/ngap-cpp-aper-full-pdu-qualification-closeout.md \
 docs/ngap-cpp-aper-target-envelope-outcome-closeout.md \
 tools/full-pdu-qualification/README.md tools/full-pdu-qualification/requirements.txt \
 tools/full-pdu-qualification/probe.c tools/full-pdu-qualification/native_values.py \
 tools/full-pdu-qualification/semantic_bridge.py tools/full-pdu-qualification/reference_framing.py \
 tools/full-pdu-qualification/qualify.py tools/full-pdu-qualification/accepted-profile.json \
 tools/full-pdu-qualification/qualification-summary.json tools/full-pdu-qualification/compatibility-summary.json
