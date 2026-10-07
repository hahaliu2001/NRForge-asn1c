AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -DHAVE_CONFIG_H -I$(top_srcdir) -I$(top_srcdir)/libasn1common -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -I$(top_srcdir)/libasn1typed

tool_common = developer_tree.c developer_tree.h
common_libs = $(top_builddir)/libasn1fix/libasn1fix.la $(top_builddir)/libasn1parser/libasn1parser.la $(top_builddir)/libasn1common/libasn1common.la
noinst_PROGRAMS = asn1typed_real_probe asn1typed_tree_inspect
TESTS = test_tools.sh
EXTRA_DIST = developer_tree.h test_tools.sh README.md qualification/ngap-rel18.modules
asn1typed_real_probe_SOURCES = asn1typed_real_probe.c $(tool_common)
asn1typed_real_probe_LDADD = $(top_builddir)/libasn1typed/libasn1typed_extract.la $(top_builddir)/libasn1typed/libasn1typed.la $(common_libs)
asn1typed_tree_inspect_SOURCES = asn1typed_tree_inspect.c $(tool_common)
asn1typed_tree_inspect_LDADD = $(common_libs)
