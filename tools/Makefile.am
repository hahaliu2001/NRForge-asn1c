AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -DHAVE_CONFIG_H -I$(top_srcdir) -I$(top_srcdir)/libasn1common -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -I$(top_srcdir)/libasn1typed

tool_common = developer_tree.c developer_tree.h
common_libs = $(top_builddir)/libasn1fix/libasn1fix.la $(top_builddir)/libasn1parser/libasn1parser.la $(top_builddir)/libasn1common/libasn1common.la
noinst_PROGRAMS = asn1typed_real_probe asn1typed_tree_inspect asn1typed_ioc_probe
TESTS = test_tools.sh
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
asn1typed_codec_coverage_SOURCES = asn1typed_codec_coverage.c $(tool_common)
asn1typed_codec_coverage_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
EXTRA_DIST += n12-codec-coverage/messages.txt n12-codec-coverage/run.py n12-codec-coverage/README.md n12-codec-coverage/coverage.json

EXTRA_DIST += n13-full-codec-readiness/scan.py n13-full-codec-readiness/README.md n13-full-codec-readiness/readiness.json
