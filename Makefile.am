
ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@
CODE_COVERAGE_IGNORE_PATTERN="tests/*" "lex.yy.c" "y.tab.c" "asn1p_l.l" "asn1p_y.y"

SUBDIRS =                       \
    libaper libngap libasn1common libasn1typed libasn1parser \
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

EXTRA_DIST += docs/ngap-cpp-aper-unified-dispatch-contract.md \
 tools/ngap-dispatch/README.md \
 tools/unified-ngap-qualification/README.md \
 tools/unified-ngap-qualification/semantic_bridge.py \
 tools/unified-ngap-qualification/qualify.py
EXTRA_DIST += tools/ngap-dispatch/check_public.cpp
EXTRA_DIST += tools/unified-ngap-qualification/compatibility-summary.json
EXTRA_DIST += docs/ngap-cpp-aper-unified-dispatch-closeout.md \
 tools/unified-ngap-qualification/accepted-profile.json \
 tools/unified-ngap-qualification/qualification-summary.json

EXTRA_DIST += docs/ngap-cpp-sdk-contract.md \
 cmake/NrforgeNgapSdk.cmake cmake/VerifyNgapSdk.cmake \
 cmake/WriteNgapSdkReceipt.cmake cmake/NRForgeNGAPConfig.cmake.in \
 tools/ngap-dispatch/seal_sdk.py \
 tools/ngap-sdk/build.py tools/ngap-sdk/README.md \
 tools/ngap-sdk-consumer/CMakeLists.txt tools/ngap-sdk-consumer/ng_setup.cpp \
 tools/ngap-sdk-consumer/prepare.py tools/ngap-sdk-consumer/verify.py \
 tools/ngap-sdk-consumer/native-fixtures.json tools/ngap-sdk-consumer/README.md \
 .github/workflows/ngap-sdk.yml

EXTRA_DIST += docs/ngap-cpp-sdk-closeout.md tools/ngap-sdk/verification-summary.json

EXTRA_DIST += python/CMakeLists.txt \
 python/LICENSE \
 python/PYBIND11-LICENSE \
 python/README.md \
 python/examples/ng_setup.py \
 python/ngap_sdk/conversion.hpp \
 python/ngap_sdk/generate_bindings.py \
 python/ngap_sdk/module.cpp \
 python/pyproject.toml \
 python/src/nrforge_ngap/__init__.py \
 python/tests/golden-131.json \
 python/tests/ng-setup-native.json \
 python/tests/run_installed.py \
 python/tests/test_sdk.py

EXTRA_DIST += python/tests/test_generator.py

EXTRA_DIST += docs/ngap-python-sdk-contract-and-closeout.md python/verification-summary.json
