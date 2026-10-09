AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/libasn1common

noinst_LTLIBRARIES = libasn1typed.la libasn1typed_extract.la
libasn1typed_la_SOURCES = asn1typed.c asn1typed.h asn1typed_name.c asn1typed_name.h \
	asn1typed_render_cpp.c asn1typed_render_cpp_slice.c asn1typed_render_cpp.h
libasn1typed_extract_la_SOURCES = asn1typed_extract.c asn1typed_extract.h
libasn1typed_extract_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix
libasn1typed_extract_la_LIBADD = \
	libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

check_PROGRAMS = check_asn1typed_ioc_cpp check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp check_asn1typed_owned_slice
TESTS = check_asn1typed_ioc_cpp.sh check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp.sh check_asn1typed_owned_slice.sh
AM_TESTS_ENVIRONMENT = CXX="$(CXX)"; export CXX;
check_asn1typed_render_cpp_SOURCES = check_asn1typed_render_cpp.c
check_asn1typed_render_cpp_LDADD = libasn1typed.la
check_asn1typed_name_SOURCES = check_asn1typed_name.c
check_asn1typed_name_LDADD = libasn1typed.la
check_asn1typed_SOURCES = check_asn1typed.c
check_asn1typed_LDADD = libasn1typed.la
check_asn1typed_extract_SOURCES = check_asn1typed_extract.c check_asn1typed_ioc.c
check_asn1typed_extract_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -DT2_FIXTURE=\"$(abs_srcdir)/fixtures/ordinary-t2.asn1\" -DT3_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-t3.asn1\" -DT4_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-multimodule.asn1\" -DT5_FIXTURE=\"$(abs_srcdir)/fixtures/parameterized-reference-b7a.asn1\" -DT6_FIXTURE=\"$(abs_srcdir)/fixtures/visible-string-size-t6.asn1\" -DT7_FIXTURE=\"$(abs_srcdir)/fixtures/integer-value-range.asn1\" -DT8_FIXTURE=\"$(abs_srcdir)/fixtures/integer-permitted-set-t8.asn1\" -DT9_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-empty-typed.asn1\"
check_asn1typed_extract_LDADD = \
	libasn1typed_extract.la libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

EXTRA_DIST = check_asn1typed_render_cpp.sh check_asn1typed_owned_slice.sh fixtures/ordinary-t2.asn1 fixtures/cpp-aper-s1.asn1 fixtures/cpp-aper-s1-int64.asn1 fixtures/ioc-t3.asn1 fixtures/ioc-multimodule.asn1 fixtures/parameterized-reference-b7a.asn1 fixtures/visible-string-size-t6.asn1 fixtures/integer-value-range.asn1 fixtures/integer-permitted-set-t8.asn1 fixtures/ioc-empty-typed.asn1

# Only the integration test links extraction/parser/fixer with the renderer.
check_asn1typed_ioc_cpp_SOURCES = check_asn1typed_ioc_cpp.c
check_asn1typed_ioc_cpp_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_ioc_cpp_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += check_asn1typed_ioc_cpp.sh

check_asn1typed_owned_slice_SOURCES = check_asn1typed_owned_slice.c
check_asn1typed_owned_slice_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DS1_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-s1.asn1\" -DS1_INT64_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-s1-int64.asn1\"
check_asn1typed_owned_slice_LDADD = $(check_asn1typed_extract_LDADD)
