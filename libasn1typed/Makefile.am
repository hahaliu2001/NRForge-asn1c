AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/libasn1common

noinst_LTLIBRARIES = libasn1typed.la libasn1typed_extract.la
libasn1typed_la_SOURCES = asn1typed.c asn1typed.h asn1typed_name.c asn1typed_name.h
libasn1typed_extract_la_SOURCES = asn1typed_extract.c asn1typed_extract.h
libasn1typed_extract_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix
libasn1typed_extract_la_LIBADD = \
	libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

check_PROGRAMS = check_asn1typed check_asn1typed_extract check_asn1typed_name
TESTS = $(check_PROGRAMS)
check_asn1typed_name_SOURCES = check_asn1typed_name.c
check_asn1typed_name_LDADD = libasn1typed.la
check_asn1typed_SOURCES = check_asn1typed.c
check_asn1typed_LDADD = libasn1typed.la
check_asn1typed_extract_SOURCES = check_asn1typed_extract.c check_asn1typed_ioc.c
check_asn1typed_extract_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -DT2_FIXTURE=\"$(abs_srcdir)/fixtures/ordinary-t2.asn1\" -DT3_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-t3.asn1\"
check_asn1typed_extract_LDADD = \
	libasn1typed_extract.la libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

EXTRA_DIST = fixtures/ordinary-t2.asn1 fixtures/ioc-t3.asn1
