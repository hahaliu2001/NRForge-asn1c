
ACLOCAL_AMFLAGS = -I m4

@CODE_COVERAGE_RULES@
CODE_COVERAGE_IGNORE_PATTERN="tests/*" "lex.yy.c" "y.tab.c" "asn1p_l.l" "asn1p_y.y"

SUBDIRS =                       \
    libaper libuper libngap libasn1common libasn1typed libasn1parser \
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

EXTRA_DIST += docs/rrc-cpp-uper-p5-runtime-closeout.md \
 tools/uper-qualification/README.md tools/uper-qualification/requirements.txt \
 tools/uper-qualification/probe.cpp tools/uper-qualification/qualify.py \
 tools/uper-qualification/qualification.json tools/uper-qualification/verification-rrc-p5.json \
 tools/uper-qualification/review-rrc-p5.md

EXTRA_DIST += tools/rrc-uper/probe.c tools/rrc-uper/generate.py tools/rrc-uper/check.py \
 tools/rrc-uper/compound.asn1 tools/rrc-uper/check_generated.cpp tools/rrc-uper/README.md \
 tools/rrc-uper/readiness-rrc-p6.json tools/rrc-uper/verification-rrc-p6.json \
 tools/rrc-uper/review-rrc-p6.md docs/rrc-cpp-uper-compound-contract-and-closeout.md
EXTRA_DIST += tools/rrc-uper/observe_oracles.py tools/rrc-uper/oracle-observation.json \
 tools/rrc-uper/aper-regression-rrc-p6.json
EXTRA_DIST += tools/rrc-wire-qualification/qualify.py tools/rrc-wire-qualification/native_values.py \
 tools/rrc-wire-qualification/semantic_bridge.py tools/rrc-wire-qualification/audit_reference.py \
 tools/rrc-wire-qualification/compound.asn1 tools/rrc-wire-qualification/requirements.txt \
 tools/rrc-wire-qualification/README.md tools/rrc-wire-qualification/qualification.json \
 tools/rrc-wire-qualification/qualification-sanitized.json tools/rrc-wire-qualification/reference-boundary.json \
 tools/rrc-wire-qualification/verification.json tools/rrc-wire-qualification/review.md \
 docs/rrc-cpp-uper-p7-wire-qualification-closeout.md
EXTRA_DIST += cmake/NrforgeRrcSdk.cmake docs/rrc-cpp-sdk-contract.md docs/rrc-cpp-sdk-closeout.md \
 tools/rrc-sdk/build.py tools/rrc-sdk/verify_seal.py tools/rrc-sdk/record.py \
 tools/rrc-sdk/README.md tools/rrc-sdk/verification-summary.json tools/rrc-sdk/review.md \
 tools/rrc-sdk-consumer/prepare.py tools/rrc-sdk-consumer/isolate.py tools/rrc-sdk-consumer/README.md \
 tools/rrc-sdk-consumer/example/CMakeLists.txt tools/rrc-sdk-consumer/example/mib.cpp

EXTRA_DIST += docs/rrc-python-sdk-contract-and-closeout.md \
 python/rrc/CMakeLists.txt \
 python/rrc/LICENSE \
 python/rrc/PYBIND11-LICENSE \
 python/rrc/README.md \
 python/rrc/bindings/conversion.hpp \
 python/rrc/bindings/generate_bindings.py \
 python/rrc/bindings/module.cpp \
 python/rrc/pyproject.toml \
 python/rrc/src/nrforge_rrc/__init__.py \
 python/rrc/tests/golden.json \
 python/rrc/tests/test_generator.py \
 python/rrc/tests/test_sdk.py \
 tools/rrc-python/README.md \
 tools/rrc-python/prepare.py \
 tools/rrc-python/prepare_vectors.py \
 tools/rrc-python/record.py \
 tools/rrc-python/run_installed.py \
 tools/rrc-python/verification-summary.json \
 tools/rrc-python/review.md

EXTRA_DIST += docs/rrc-remaining-blockers-and-batch-plan.md \
 tools/rrc-blockers/study.py tools/rrc-blockers/inspect.c tools/rrc-blockers/study.json \
 tools/rrc-blockers/README.md tools/rrc-blockers/verification.json tools/rrc-blockers/review.md

EXTRA_DIST += docs/rrc-singleton-integer-contract-and-closeout.md \
 tools/rrc-singleton/check.py tools/rrc-singleton/singletons.asn1 \
 tools/rrc-singleton/check_generated.cpp tools/rrc-singleton/README.md \
 tools/rrc-singleton/readiness.json tools/rrc-singleton/verification.json \
 tools/rrc-singleton/review.md

EXTRA_DIST += docs/rrc-constrained-collection-elements-contract-and-closeout.md \
 tools/rrc-collection-elements/check.py tools/rrc-collection-elements/collections.asn1 \
 tools/rrc-collection-elements/check_generated.cpp tools/rrc-collection-elements/README.md \
 tools/rrc-collection-elements/readiness.json tools/rrc-collection-elements/verification.json \
 tools/rrc-collection-elements/review.md

EXTRA_DIST += docs/rrc-fixed-choice-tag-transfer-contract-and-closeout.md \
 tools/rrc-fixed-choice/check.py tools/rrc-fixed-choice/choices.asn1 \
 tools/rrc-fixed-choice/check_extract.c tools/rrc-fixed-choice/check_instances.c \
 tools/rrc-fixed-choice/check_generated.cpp tools/rrc-fixed-choice/README.md \
 tools/rrc-fixed-choice/readiness.json tools/rrc-fixed-choice/verification.json \
 tools/rrc-fixed-choice/review.md

EXTRA_DIST += tools/rrc-late-choice/contract.md tools/rrc-late-choice/check.py \
 tools/rrc-late-choice/late.asn1 tools/rrc-late-choice/check_fix.c \
 tools/rrc-late-choice/check_instances.c tools/rrc-late-choice/check_generated.cpp \
 tools/rrc-late-choice/README.md tools/rrc-late-choice/readiness.json \
 tools/rrc-late-choice/verification.json tools/rrc-late-choice/review.md \
 docs/rrc-late-choice-tag-completion-closeout.md

EXTRA_DIST += tools/rrc-opaque-contents/contract.md tools/rrc-opaque-contents/check.py \
 tools/rrc-opaque-contents/opaque.asn1 tools/rrc-opaque-contents/check_extract.c \
 tools/rrc-opaque-contents/check_generated.cpp tools/rrc-opaque-contents/README.md \
 tools/rrc-opaque-contents/readiness.json tools/rrc-opaque-contents/verification.json \
 tools/rrc-opaque-contents/review.md docs/rrc-opaque-contents-closeout.md

EXTRA_DIST += tools/rrc-inherited-alias/contract.md tools/rrc-inherited-alias/check.py \
 tools/rrc-inherited-alias/aliases.asn1 tools/rrc-inherited-alias/check_extract.c \
 tools/rrc-inherited-alias/check_generated.cpp tools/rrc-inherited-alias/check_real.c \
 tools/rrc-inherited-alias/README.md tools/rrc-inherited-alias/readiness.json \
 tools/rrc-inherited-alias/verification.json tools/rrc-inherited-alias/review.md \
 docs/rrc-inherited-alias-closeout.md

EXTRA_DIST += tools/rrc-null-open/contract.md tools/rrc-null-open/check.py \
 tools/rrc-null-open/null.asn1 tools/rrc-null-open/check_extract.c \
 tools/rrc-null-open/check_generated.cpp tools/rrc-null-open/README.md \
 tools/rrc-null-open/readiness.json tools/rrc-null-open/verification.json \
 tools/rrc-null-open/review.md docs/rrc-null-open-closeout.md

EXTRA_DIST += tools/rrc-wire-expanded/contract.md tools/rrc-wire-expanded/qualify.py \
 tools/rrc-wire-expanded/native_values.py tools/rrc-wire-expanded/witnesses.py \
 tools/rrc-wire-expanded/semantic_bridge.py tools/rrc-wire-expanded/compound.asn1 \
 tools/rrc-wire-expanded/requirements.txt tools/rrc-wire-expanded/README.md \
 tools/rrc-wire-expanded/qualification.json tools/rrc-wire-expanded/qualification-sanitized.json \
 tools/rrc-wire-expanded/verification.json tools/rrc-wire-expanded/review.md \
 docs/rrc-cpp-uper-p16-wire-qualification-closeout.md

EXTRA_DIST += cmake/NrforgeRrcSdkExpanded.cmake \
 tools/rrc-sdk-expanded/contract.md tools/rrc-sdk-expanded/build.py \
 tools/rrc-sdk-expanded/verify_seal.py tools/rrc-sdk-expanded/record.py \
 tools/rrc-sdk-expanded/README.md tools/rrc-sdk-expanded/verification.json \
 tools/rrc-sdk-expanded/review.md tools/rrc-sdk-expanded-consumer/prepare.py \
 tools/rrc-sdk-expanded-consumer/isolate.py tools/rrc-sdk-expanded-consumer/README.md \
 tools/rrc-sdk-expanded-consumer/example/CMakeLists.txt \
 tools/rrc-sdk-expanded-consumer/example/mib.cpp \
 docs/rrc-cpp-sdk-expanded-p17-closeout.md

EXTRA_DIST += tools/rrc-sdk-expanded/runtime-acceptance/evidence-sha256.json \
 tools/rrc-sdk-expanded/runtime-acceptance/example_build.log \
 tools/rrc-sdk-expanded/runtime-acceptance/example_configure.log \
 tools/rrc-sdk-expanded/runtime-acceptance/example_test.log \
 tools/rrc-sdk-expanded/runtime-acceptance/mismatched_header_archive.log \
 tools/rrc-sdk-expanded/runtime-acceptance/original-path-restoration.json \
 tools/rrc-sdk-expanded/runtime-acceptance/resume-runtime.py \
 tools/rrc-sdk-expanded/runtime-acceptance/root000.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root001.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root002.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root003.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root004.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root005.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root006.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root007.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root008.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root009.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root010.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root011.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root012.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root013.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root014.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root015.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root016.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root017.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root018.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root019.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root020.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root021.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root022.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root023.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root024.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root025.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root026.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root027.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root028.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root029.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root030.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root031.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root032.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root033.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root034.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root035.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root036.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root037.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root038.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root039.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root040.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root041.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root042.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root043.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root044.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root045.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root046.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root047.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root048.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root049.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root050.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root051.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root052.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root053.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root054.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root055.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root056.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root057.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root058.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root059.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root060.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root061.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root062.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root063.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root064.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root065.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root066.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root067.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root068.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root069.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root070.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root071.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root072.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root073.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root074.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root075.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root076.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root077.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root078.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root079.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root080.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root081.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root082.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root083.log \
 tools/rrc-sdk-expanded/runtime-acceptance/root084.log \
 tools/rrc-sdk-expanded/runtime-acceptance/verification.json \
 tools/rrc-sdk-expanded/runtime-acceptance/wrong_fingerprint.log

EXTRA_DIST += docs/rrc-python-expanded-p18-closeout.md \
 python/rrc-expanded/CMakeLists.txt \
 python/rrc-expanded/LICENSE \
 python/rrc-expanded/PYBIND11-LICENSE \
 python/rrc-expanded/README.md \
 python/rrc-expanded/bindings/conversion.hpp \
 python/rrc-expanded/bindings/generate_bindings.py \
 python/rrc-expanded/bindings/module.cpp \
 python/rrc-expanded/pyproject.toml \
 python/rrc-expanded/src/nrforge_rrc/__init__.py \
 python/rrc-expanded/tests/golden.json \
 python/rrc-expanded/tests/test_generator.py \
 python/rrc-expanded/tests/test_sdk.py \
 tools/rrc-python-expanded/README.md \
 tools/rrc-python-expanded/contract.md \
 tools/rrc-python-expanded/evidence/aper-install.log \
 tools/rrc-python-expanded/evidence/e1ap-installed.log \
 tools/rrc-python-expanded/evidence/f1ap-installed.log \
 tools/rrc-python-expanded/evidence/generator.log \
 tools/rrc-python-expanded/evidence/isolated.json \
 tools/rrc-python-expanded/evidence/isolated.log \
 tools/rrc-python-expanded/evidence/isolated.restoration.json \
 tools/rrc-python-expanded/evidence/native.log \
 tools/rrc-python-expanded/evidence/ngap-installed.log \
 tools/rrc-python-expanded/evidence/rrc-install.log \
 tools/rrc-python-expanded/evidence/rrc-installed.log \
 tools/rrc-python-expanded/evidence/sdist.log \
 tools/rrc-python-expanded/evidence/wheel-final.log \
 tools/rrc-python-expanded/evidence/wheel-initial.log \
 tools/rrc-python-expanded/prepare.py \
 tools/rrc-python-expanded/prepare_vectors.py \
 tools/rrc-python-expanded/record.py \
 tools/rrc-python-expanded/run_installed.py \
 tools/rrc-python-expanded/evidence/elf-exports.log \
 tools/rrc-python-expanded/evidence/elf-dynamic.log \
 tools/rrc-python-expanded/verification.json \
 tools/rrc-python-expanded/review.md

# Unified SDK source delivery; historical receipts are not regenerated.
EXTRA_DIST += tools/unified-sdk/README.md \
  tools/unified-sdk/build_rrc.py \
  tools/unified-sdk/check_f1_oracle.py \
  tools/unified-sdk/consume.py \
  tools/unified-sdk/prepare_rrc_consumers.py \
  tools/unified-sdk/produce.py \
  tools/unified-sdk/release.py \
  tools/unified-sdk/run.sh \
  tools/unified-sdk/scope.json \
  tools/unified-sdk/stage_schema.py \
  tools/unified-sdk/test_release_guards.py \
  python/ngap_sdk/root_catalog.py

EXTRA_DIST += docs/unified-sdk-expanded-source-review.md
