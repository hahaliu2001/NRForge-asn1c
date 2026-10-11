AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -DHAVE_CONFIG_H -I$(top_srcdir) -I$(top_srcdir)/libasn1common -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -I$(top_srcdir)/libasn1typed

tool_common = developer_tree.c developer_tree.h
common_libs = $(top_builddir)/libasn1fix/libasn1fix.la $(top_builddir)/libasn1parser/libasn1parser.la $(top_builddir)/libasn1common/libasn1common.la
noinst_PROGRAMS = asn1typed_real_probe asn1typed_tree_inspect asn1typed_ioc_probe
TESTS = test_tools.sh
check_PROGRAMS = check_dispatch_schema
TESTS += check_dispatch_schema
check_dispatch_schema_SOURCES = ngap-dispatch/check_schema.c $(tool_common)
check_dispatch_schema_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
EXTRA_DIST = developer_tree.h test_tools.sh README.md qualification/ngap-rel18.modules
EXTRA_DIST += n7-sequence-extension-qualification/qualify.py n7-sequence-extension-qualification/driver.cpp n7-sequence-extension-qualification/README.md n7-sequence-extension-qualification/requirements.txt n7-sequence-extension-qualification/qualification-summary.json n7-sequence-extension-qualification/accepted-profile.json
EXTRA_DIST += n8-collection-qualification/qualify.py n8-collection-qualification/driver.cpp n8-collection-qualification/sender.asn1 n8-collection-qualification/README.md n8-collection-qualification/requirements.txt n8-collection-qualification/qualification-summary.json n8-collection-qualification/accepted-profile.json
asn1typed_real_probe_SOURCES = asn1typed_real_probe.c $(tool_common)
asn1typed_real_probe_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
asn1typed_tree_inspect_SOURCES = asn1typed_tree_inspect.c $(tool_common)
asn1typed_tree_inspect_LDADD = $(common_libs)
asn1typed_ioc_probe_SOURCES = asn1typed_ioc_probe.c $(tool_common)
asn1typed_ioc_probe_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)

EXTRA_DIST += \
	n9-ioc-qualification/driver.cpp \
	n9-ioc-qualification/qualify.py \
	n9-ioc-qualification/sender.asn1 \
	n9-ioc-qualification/raw-framing.asn1 \
	n9-ioc-qualification/requirements.txt \
	n9-ioc-qualification/README.md \
	n9-ioc-qualification/accepted-profile.json \
	n9-ioc-qualification/qualification-summary.json

EXTRA_DIST += \
	n10-body-qualification/body_adapter.hpp \
	n10-body-qualification/driver.cpp \
	n10-body-qualification/qualify.py \
	n10-body-qualification/requirements.txt \
	n10-body-qualification/source-manifest.json \
	n10-body-qualification/README.md \
	n10-body-qualification/accepted-profile.json \
	n10-body-qualification/qualification-summary.json

EXTRA_DIST += \
	n11-envelope-qualification/adapter.hpp \
	n11-envelope-qualification/driver.cpp \
	n11-envelope-qualification/qualify.py \
	n11-envelope-qualification/body_cases.py \
	n11-envelope-qualification/native_reference.py \
	n11-envelope-qualification/requirements.txt \
	n11-envelope-qualification/source-manifest.json \
	n11-envelope-qualification/README.md \
	n11-envelope-qualification/accepted-profile.json \
	n11-envelope-qualification/qualification-summary.json

noinst_PROGRAMS += asn1typed_codec_coverage
noinst_PROGRAMS += asn1typed_full_pdu_probe
asn1typed_full_pdu_probe_SOURCES = full-pdu-qualification/probe.c $(tool_common)
asn1typed_full_pdu_probe_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
asn1typed_codec_coverage_SOURCES = asn1typed_codec_coverage.c $(tool_common)
asn1typed_codec_coverage_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
EXTRA_DIST += n12-codec-coverage/messages.txt n12-codec-coverage/run.py n12-codec-coverage/README.md n12-codec-coverage/coverage.json

EXTRA_DIST += n13-full-codec-readiness/scan.py n13-full-codec-readiness/README.md n13-full-codec-readiness/readiness.json

EXTRA_DIST += n14-octet-qualification/driver.cpp n14-octet-qualification/qualify.py n14-octet-qualification/requirements.txt n14-octet-qualification/README.md n14-octet-qualification/qualification-summary.json n14-octet-qualification/readiness.json

EXTRA_DIST += n15-bit-qualification/README.md n15-bit-qualification/driver.cpp n15-bit-qualification/qualify.py n15-bit-qualification/requirements.txt n15-bit-qualification/qualification-summary.json n15-bit-qualification/readiness.json

EXTRA_DIST += n16-integer-qualification/README.md n16-integer-qualification/driver.cpp n16-integer-qualification/qualify.py n16-integer-qualification/requirements.txt n16-integer-qualification/qualification-summary.json n16-integer-qualification/readiness.json

EXTRA_DIST += n17-shape-qualification/README.md n17-shape-qualification/driver.cpp n17-shape-qualification/qualify.py n17-shape-qualification/requirements.txt n17-shape-qualification/qualification-summary.json n17-shape-qualification/baseline-shapes.json n17-shape-qualification/readiness.json

EXTRA_DIST += n18-integer-study/README.md n18-integer-study/probe.c n18-integer-study/inventory.py n18-integer-study/domain-inventory.json n18-integer-study/reference.py n18-integer-study/reference-summary.json n18-integer-study/requirements.txt

EXTRA_DIST += fragmented-collection-qualification/compare.py fragmented-collection-qualification/results.json
EXTRA_DIST += shared-character-qualification/README.md shared-character-qualification/qualify.py shared-character-qualification/requirements.txt shared-character-qualification/qualification-summary.json
EXTRA_DIST += shared-null-qualification/verify.py shared-null-qualification/native-summary.json shared-null-qualification/physical-extraction.json
EXTRA_DIST += batch-size-qualification/README.md batch-size-qualification/driver.cpp batch-size-qualification/qualify.py batch-size-qualification/requirements.txt batch-size-qualification/qualification-summary.json batch-size-qualification/frozen-use-sites.json
EXTRA_DIST += shared-integer-qualification/qualify.py shared-integer-qualification/requirements.txt shared-integer-qualification/README.md shared-integer-qualification/summary.json
EXTRA_DIST += private-key-qualification/qualify.py private-key-qualification/results.json private-key-qualification/requirements.txt private-key-qualification/README.md

EXTRA_DIST += shared-capability-qualification/README.md shared-capability-qualification/verify.py shared-capability-qualification/readiness.json shared-capability-qualification/summary.json

noinst_PROGRAMS += asn1typed_ngap_dispatch
asn1typed_ngap_dispatch_SOURCES = ngap-dispatch/generate.c $(tool_common)
asn1typed_ngap_dispatch_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)

EXTRA_DIST += f1ap-readiness/scan.py f1ap-readiness/test_scan.py f1ap-readiness/README.md f1ap-readiness/source-manifest.json f1ap-readiness/f1ap-rel18.modules f1ap-readiness/readiness.json f1ap-readiness/review.md
EXTRA_DIST += f1ap-readiness/check_dispatch.py f1ap-readiness/readiness-f1-p3.json f1ap-readiness/dispatch-integration-f1-p3.json f1ap-readiness/ngap-regression-f1-p3.json f1ap-readiness/ngap-artifact-regression-f1-p3.json f1ap-readiness/review-f1-p3.md
EXTRA_DIST += f1ap-wire-qualification/qualify.py f1ap-wire-qualification/native_reference.py f1ap-wire-qualification/native_values.py f1ap-wire-qualification/semantic_bridge.py f1ap-wire-qualification/reference_framing.py f1ap-wire-qualification/requirements.txt f1ap-wire-qualification/README.md f1ap-wire-qualification/accepted-profile.json f1ap-wire-qualification/qualification-summary.json f1ap-wire-qualification/review.md

EXTRA_DIST += e1ap-readiness/scan.py e1ap-readiness/test_scan.py e1ap-readiness/README.md e1ap-readiness/source-manifest.json e1ap-readiness/e1ap-rel18.modules e1ap-readiness/readiness.json e1ap-readiness/review.md
EXTRA_DIST += e1ap-readiness/check_regression.py e1ap-readiness/readiness-e1-p2.json e1ap-readiness/ngap-f1ap-regression-e1-p2.json e1ap-readiness/review-e1-p2.md
