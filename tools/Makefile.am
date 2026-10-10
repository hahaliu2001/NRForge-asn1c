AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -DHAVE_CONFIG_H -I$(top_srcdir) -I$(top_srcdir)/libasn1common -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -I$(top_srcdir)/libasn1typed

tool_common = developer_tree.c developer_tree.h
common_libs = $(top_builddir)/libasn1fix/libasn1fix.la $(top_builddir)/libasn1parser/libasn1parser.la $(top_builddir)/libasn1common/libasn1common.la
noinst_PROGRAMS = asn1typed_real_probe asn1typed_tree_inspect
TESTS = test_tools.sh
EXTRA_DIST = developer_tree.h test_tools.sh README.md qualification/ngap-rel18.modules
EXTRA_DIST += n7-sequence-extension-qualification/qualify.py n7-sequence-extension-qualification/driver.cpp n7-sequence-extension-qualification/README.md n7-sequence-extension-qualification/requirements.txt n7-sequence-extension-qualification/qualification-summary.json n7-sequence-extension-qualification/accepted-profile.json
EXTRA_DIST += n8-collection-qualification/qualify.py n8-collection-qualification/driver.cpp n8-collection-qualification/sender.asn1 n8-collection-qualification/README.md n8-collection-qualification/requirements.txt n8-collection-qualification/qualification-summary.json n8-collection-qualification/accepted-profile.json
asn1typed_real_probe_SOURCES = asn1typed_real_probe.c $(tool_common)
asn1typed_real_probe_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
asn1typed_tree_inspect_SOURCES = asn1typed_tree_inspect.c $(tool_common)
asn1typed_tree_inspect_LDADD = $(common_libs)
