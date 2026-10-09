AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/libasn1common

noinst_LTLIBRARIES = libasn1typed.la libasn1typed_extract.la
libasn1typed_la_SOURCES = asn1typed.c asn1typed.h asn1typed_name.c asn1typed_name.h asn1typed_render_cpp_internal.h \
	asn1typed_render_cpp.c asn1typed_render_cpp_slice.c asn1typed_render_cpp_codec.c asn1typed_render_cpp_enum.c asn1typed_render_cpp.h
libasn1typed_extract_la_SOURCES = asn1typed_extract.c asn1typed_extract.h
libasn1typed_extract_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix
libasn1typed_extract_la_LIBADD = \
	libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

check_PROGRAMS = check_asn1typed_ioc_cpp check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp check_asn1typed_owned_slice check_asn1typed_codec_render check_asn1typed_codec_rejections
TESTS = check_asn1typed_ioc_cpp.sh check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp.sh check_asn1typed_owned_slice.sh check_asn1typed_codec.sh check_asn1typed_codec_variants.sh check_asn1typed_codec_rejections
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

EXTRA_DIST = check_asn1typed_render_cpp.sh check_asn1typed_owned_slice.sh check_asn1typed_codec.sh check_asn1typed_codec_variants.sh fixtures/ordinary-t2.asn1 fixtures/cpp-aper-s1.asn1 fixtures/cpp-aper-s1-int64.asn1 fixtures/cpp-aper-reversed-tags.asn1 fixtures/cpp-aper-wrapper-collision.asn1 fixtures/cpp-aper-ordinal-member-zero.asn1 fixtures/cpp-aper-ordinal-member-one.asn1 fixtures/choice-wire-evidence.asn1 fixtures/ioc-t3.asn1 fixtures/ioc-multimodule.asn1 fixtures/parameterized-reference-b7a.asn1 fixtures/visible-string-size-t6.asn1 fixtures/integer-value-range.asn1 fixtures/integer-permitted-set-t8.asn1 fixtures/ioc-empty-typed.asn1 fixtures/s4-renamed-num.asn1 fixtures/s4-reordered.asn1 fixtures/s4-extra-boolean.asn1 fixtures/s4-type-swap.asn1 fixtures/s4-choice-swap.asn1 fixtures/s4-presence.asn1 fixtures/s4-count-boolean.asn1 fixtures/s4-empty-sequence.asn1 fixtures/s4-two-renamed-types.asn1 fixtures/s4-errno-field.asn1 fixtures/s4-errno-alternative.asn1

check_asn1typed_codec_render_SOURCES = check_asn1typed_codec_render.c
check_asn1typed_codec_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_codec_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_codec_rejections_SOURCES = check_asn1typed_codec_rejections.c
check_asn1typed_codec_rejections_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DS1_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-s1.asn1\" -DCOLLISION_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-wrapper-collision.asn1\" -DERRNO_FIELD_FIXTURE=\"$(abs_srcdir)/fixtures/s4-errno-field.asn1\" -DERRNO_ALTERNATIVE_FIXTURE=\"$(abs_srcdir)/fixtures/s4-errno-alternative.asn1\"
check_asn1typed_codec_rejections_LDADD = $(check_asn1typed_extract_LDADD)

# Only the integration test links extraction/parser/fixer with the renderer.
check_asn1typed_ioc_cpp_SOURCES = check_asn1typed_ioc_cpp.c
check_asn1typed_ioc_cpp_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_ioc_cpp_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += check_asn1typed_ioc_cpp.sh

# Opt-in external qualification: keep Python dependencies out of make check.
EXTRA_DIST += qualification/qualify_wire.py qualification/wire_driver.cpp qualification/requirements.txt

check_asn1typed_owned_slice_SOURCES = check_asn1typed_owned_slice.c check_asn1typed_choice_wire.c
check_asn1typed_owned_slice_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DS1_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-s1.asn1\" -DS1_INT64_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-s1-int64.asn1\" -DREVERSED_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-reversed-tags.asn1\" -DCOLLISION_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-wrapper-collision.asn1\" -DORDINAL_ZERO_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-ordinal-member-zero.asn1\" -DORDINAL_ONE_FIXTURE=\"$(abs_srcdir)/fixtures/cpp-aper-ordinal-member-one.asn1\" -DTAG_FIXTURE=\"$(abs_srcdir)/fixtures/choice-wire-evidence.asn1\"
check_asn1typed_owned_slice_LDADD = $(check_asn1typed_extract_LDADD)

check_PROGRAMS += check_asn1typed_enum_evidence
TESTS += check_asn1typed_enum_evidence
check_asn1typed_enum_evidence_SOURCES = check_asn1typed_enum_evidence.c
check_asn1typed_enum_evidence_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DENUM_FIXTURE=\"$(abs_srcdir)/fixtures/enumerated-evidence-n3.asn1\"
check_asn1typed_enum_evidence_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += fixtures/enumerated-evidence-n3.asn1
check_asn1typed_enum_evidence_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc

check_PROGRAMS += check_asn1typed_enum_render
TESTS += check_asn1typed_enum_generated.sh
check_asn1typed_enum_render_SOURCES = check_asn1typed_enum_render.c
check_asn1typed_enum_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_enum_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_enum_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_enum_generated.sh check_asn1typed_enum_generated.cpp fixtures/enum-generation-n4.asn1 fixtures/enum-generation-renamed-n4.asn1
