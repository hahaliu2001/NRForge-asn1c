AM_CFLAGS = @ADD_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)/libasn1common

noinst_LTLIBRARIES = libasn1typed.la libasn1typed_extract.la
libasn1typed_la_SOURCES = asn1typed.c asn1typed.h asn1typed_envelope.c asn1typed_envelope.h asn1typed_name.c asn1typed_name.h asn1typed_render_cpp_internal.h \
	asn1typed_render_cpp.c asn1typed_render_cpp_slice.c asn1typed_render_cpp_codec.c asn1typed_render_cpp_enum.c asn1typed_render_cpp_uint.c asn1typed_render_cpp_integer.c asn1typed_render_cpp_inline_enum.c asn1typed_render_cpp_inline_enum_internal.h asn1typed_render_cpp_compound.c asn1typed_render_cpp_ioc.c asn1typed_render_cpp_envelope.c asn1typed_render_cpp_ioc_internal.h asn1typed_render_cpp.h
libasn1typed_extract_la_SOURCES = asn1typed_extract.c asn1typed_extract.h
libasn1typed_extract_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix
libasn1typed_extract_la_LIBADD = \
	libasn1typed.la \
	$(top_builddir)/libasn1fix/libasn1fix.la \
	$(top_builddir)/libasn1parser/libasn1parser.la \
	$(top_builddir)/libasn1common/libasn1common.la

check_PROGRAMS = check_asn1typed_ioc_cpp check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp check_asn1typed_owned_slice check_asn1typed_codec_render check_asn1typed_codec_rejections
check_PROGRAMS += check_asn1typed_envelope
check_asn1typed_envelope_SOURCES = check_asn1typed_envelope.c check_envelope_alloc_core.c check_envelope_alloc_evidence.c check_envelope_alloc_extract.c asn1typed_name.c
check_asn1typed_envelope_CPPFLAGS = $(AM_CPPFLAGS) -DHAVE_CONFIG_H -I$(top_srcdir) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -DNDEBUG -DENVELOPE_EVIDENCE_FIXTURE=\"$(abs_srcdir)/fixtures/envelope-evidence-n11.asn1\"
check_asn1typed_envelope_LDADD = $(top_builddir)/libasn1fix/libasn1fix.la $(top_builddir)/libasn1parser/libasn1parser.la $(top_builddir)/libasn1common/libasn1common.la
TESTS = check_asn1typed_ioc_cpp.sh check_asn1typed check_asn1typed_extract check_asn1typed_name check_asn1typed_render_cpp.sh check_asn1typed_owned_slice.sh check_asn1typed_codec.sh check_asn1typed_codec_variants.sh check_asn1typed_codec_rejections
TESTS += check_asn1typed_envelope
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
EXTRA_DIST += fixtures/envelope-evidence-n11.asn1
check_asn1typed_enum_evidence_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc

check_PROGRAMS += check_asn1typed_enum_render
TESTS += check_asn1typed_enum_generated.sh
check_asn1typed_enum_render_SOURCES = check_asn1typed_enum_render.c
check_asn1typed_enum_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_enum_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_enum_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_enum_generated.sh check_asn1typed_enum_generated.cpp fixtures/enum-generation-n4.asn1 fixtures/enum-generation-renamed-n4.asn1

check_PROGRAMS += check_asn1typed_uint_render
TESTS += check_asn1typed_uint_generated.sh
check_asn1typed_uint_render_SOURCES = check_asn1typed_uint_render.c
check_asn1typed_uint_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_uint_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_uint_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_uint_generated.sh check_asn1typed_uint_generated.cpp fixtures/uint-generation-n5.asn1 fixtures/uint-generation-renamed-n5.asn1

check_PROGRAMS += check_asn1typed_compound_render
TESTS += check_asn1typed_compound_generated.sh
check_asn1typed_compound_render_SOURCES = check_asn1typed_compound_render.c
check_asn1typed_compound_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_compound_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_compound_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_compound_generated.sh check_asn1typed_compound_generated.cpp fixtures/compound-generation-n6.asn1 fixtures/compound-generation-reversed-n6.asn1

check_PROGRAMS += check_asn1typed_sequence_extension
TESTS += check_asn1typed_sequence_extension
check_asn1typed_sequence_extension_SOURCES = check_asn1typed_sequence_extension.c
check_asn1typed_sequence_extension_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DSEQ_EXT_FIXTURE=\"$(abs_srcdir)/fixtures/sequence-extension-evidence-n7.asn1\" -DSEQ_ADD_FIXTURE=\"$(abs_srcdir)/fixtures/sequence-extension-additions-n7.asn1\" -DSEQ_REPEAT_FIXTURE=\"$(abs_srcdir)/fixtures/sequence-extension-repeated-n7.asn1\" -DBOUND_SEQUENCE_FIXTURE=\"$(abs_srcdir)/fixtures/sequence-extension-bound-n7.asn1\" -DIOC_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-t3.asn1\"
check_asn1typed_sequence_extension_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += fixtures/sequence-extension-evidence-n7.asn1 fixtures/sequence-extension-additions-n7.asn1 fixtures/sequence-extension-repeated-n7.asn1

EXTRA_DIST += fixtures/sequence-extension-bound-n7.asn1

check_PROGRAMS += check_asn1typed_sequence_extension_render
TESTS += check_asn1typed_sequence_extension_generated.sh
check_asn1typed_sequence_extension_render_SOURCES = check_asn1typed_sequence_extension_render.c
check_asn1typed_sequence_extension_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_sequence_extension_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_sequence_extension_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_sequence_extension_generated.sh check_asn1typed_sequence_extension_generated.cpp fixtures/sequence-extension-generation-n7.asn1 fixtures/sequence-extension-sender-n7.asn1

check_PROGRAMS += check_asn1typed_collection_size
TESTS += check_asn1typed_collection_size
check_asn1typed_collection_size_SOURCES = check_asn1typed_collection_size.c
check_asn1typed_collection_size_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DCOLLECTION_SIZE_FIXTURE=\"$(abs_srcdir)/fixtures/collection-size-evidence-n8.asn1\" -DNDEBUG
check_asn1typed_collection_size_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += fixtures/collection-size-evidence-n8.asn1

check_PROGRAMS += check_asn1typed_collection_render
TESTS += check_asn1typed_collection_generated.sh
check_asn1typed_collection_render_SOURCES = check_asn1typed_collection_render.c
check_asn1typed_collection_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_collection_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_collection_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_collection_generated.sh check_asn1typed_collection_generated.cpp fixtures/collection-generation-n8.asn1

check_PROGRAMS += check_asn1typed_ioc_evidence
TESTS += check_asn1typed_ioc_evidence
check_asn1typed_ioc_evidence_SOURCES = check_asn1typed_ioc_evidence.c check_ioc_alloc_core.c check_ioc_alloc_extract.c
check_asn1typed_ioc_evidence_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DIOC_EVIDENCE_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-evidence-n9.asn1\" -DNDEBUG
check_asn1typed_ioc_evidence_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += fixtures/ioc-evidence-n9.asn1

check_PROGRAMS += check_asn1typed_ioc_render
TESTS += check_asn1typed_ioc_generated.sh
check_asn1typed_ioc_render_SOURCES = check_asn1typed_ioc_render.c
check_asn1typed_ioc_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_ioc_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_ioc_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_ioc_generated.sh check_asn1typed_ioc_generated.cpp fixtures/ioc-generation-n9.asn1

check_PROGRAMS += check_asn1typed_envelope_render
TESTS += check_asn1typed_envelope_generated.sh
check_asn1typed_envelope_render_SOURCES = check_asn1typed_envelope_render.c ../tools/developer_tree.c ../tools/developer_tree.h
check_asn1typed_envelope_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_envelope_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_envelope_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_envelope_generated.sh check_asn1typed_envelope_generated.cpp check_asn1typed_envelope_outcomes.cpp

check_PROGRAMS += check_asn1typed_octet_render
TESTS += check_asn1typed_octet_generated.sh
check_asn1typed_octet_render_SOURCES = check_asn1typed_octet_render.c
check_asn1typed_octet_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_octet_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_octet_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_octet_generated.sh check_asn1typed_octet_generated.cpp fixtures/octets-generation-n14.asn1

check_PROGRAMS += check_asn1typed_ioc_octets
TESTS += check_asn1typed_ioc_octets check_asn1typed_ioc_octet_generated.sh
check_asn1typed_ioc_octets_SOURCES = check_asn1typed_ioc_octets.c check_ioc_alloc_core.c check_ioc_alloc_extract.c
check_asn1typed_ioc_octets_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DIOC_OCTETS_FIXTURE=\"$(abs_srcdir)/fixtures/ioc-octets-n14.asn1\" -DNDEBUG
check_asn1typed_ioc_octets_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += check_asn1typed_ioc_octet_generated.sh check_asn1typed_ioc_octet_generated.cpp fixtures/ioc-octets-n14.asn1

check_PROGRAMS += check_asn1typed_octet_api
TESTS += check_asn1typed_octet_api
check_asn1typed_octet_api_SOURCES = check_asn1typed_octet_api.cpp
check_asn1typed_octet_api_CPPFLAGS = $(AM_CPPFLAGS)
check_asn1typed_octet_api_LDADD = libasn1typed.la

check_PROGRAMS += check_asn1typed_bit_render
TESTS += check_asn1typed_bit_generated.sh
check_asn1typed_bit_render_SOURCES = check_asn1typed_bit_render.c
check_asn1typed_bit_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_bit_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_bit_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_bit_generated.sh check_asn1typed_bit_generated.cpp fixtures/bits-generation-n15.asn1

check_PROGRAMS += check_asn1typed_bit_api
TESTS += check_asn1typed_bit_api
check_asn1typed_bit_api_SOURCES = check_asn1typed_bit_api.cpp
check_asn1typed_bit_api_CPPFLAGS = $(AM_CPPFLAGS)
check_asn1typed_bit_api_LDADD = libasn1typed.la

check_PROGRAMS += check_asn1typed_bit_use_size
TESTS += check_asn1typed_bit_use_size
check_asn1typed_bit_use_size_SOURCES = check_asn1typed_bit_use_size.c check_ioc_alloc_core.c check_ioc_alloc_extract.c
check_asn1typed_bit_use_size_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DBIT_USE_SIZE_FIXTURE=\"$(abs_srcdir)/fixtures/bit-use-size-n15.asn1\" -DNDEBUG
check_asn1typed_bit_use_size_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += fixtures/bit-use-size-n15.asn1

TESTS += check_asn1typed_ioc_bit_generated.sh
EXTRA_DIST += check_asn1typed_ioc_bit_generated.sh check_asn1typed_ioc_bit_generated.cpp fixtures/ioc-bits-n15.asn1

check_PROGRAMS += check_asn1typed_integer_render check_asn1typed_integer_use_range check_asn1typed_integer_api
TESTS += check_asn1typed_integer_generated.sh check_asn1typed_integer_use_range check_asn1typed_integer_api
check_asn1typed_integer_render_SOURCES = check_asn1typed_integer_render.c
check_asn1typed_integer_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_integer_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_integer_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
check_asn1typed_integer_use_range_SOURCES = check_asn1typed_integer_use_range.c check_ioc_alloc_core.c check_ioc_alloc_extract.c
check_asn1typed_integer_use_range_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DINTEGER_USE_RANGE_FIXTURE=\"$(abs_srcdir)/fixtures/integer-use-range-n16.asn1\" -DNDEBUG
check_asn1typed_integer_use_range_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_integer_api_SOURCES = check_asn1typed_integer_api.cpp
check_asn1typed_integer_api_CPPFLAGS = $(AM_CPPFLAGS)
check_asn1typed_integer_api_LDADD = libasn1typed.la
EXTRA_DIST += check_asn1typed_integer_generated.sh check_asn1typed_integer_generated.cpp fixtures/integers-generation-n16.asn1 fixtures/integer-use-range-n16.asn1

check_PROGRAMS += check_asn1typed_shape_render check_asn1typed_shape_api
TESTS += check_asn1typed_shape_generated.sh check_asn1typed_shape_api
check_asn1typed_shape_render_SOURCES = check_asn1typed_shape_render.c
check_asn1typed_shape_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_shape_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_shape_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
check_asn1typed_shape_api_SOURCES = check_asn1typed_shape_api.cpp
check_asn1typed_shape_api_CPPFLAGS = $(AM_CPPFLAGS)
check_asn1typed_shape_api_LDADD = libasn1typed.la
EXTRA_DIST += check_asn1typed_shape_generated.sh check_asn1typed_shape_generated.cpp fixtures/shape-generation-n17.asn1 fixtures/ioc-shapes-n17.asn1

check_PROGRAMS += check_asn1typed_character_render
TESTS += check_asn1typed_character_generated.sh
check_asn1typed_character_render_SOURCES = check_asn1typed_character_render.c
check_asn1typed_character_render_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libasn1parser -I$(top_srcdir)/libasn1fix -DNDEBUG
check_asn1typed_character_render_LDADD = $(check_asn1typed_extract_LDADD)
EXTRA_DIST += check_asn1typed_character_generated.cpp check_asn1typed_character_generated.sh fixtures/characters-generation-batch.asn1 fixtures/ioc-characters-generation-batch.asn1
check_PROGRAMS += check_asn1typed_null_render
TESTS += check_asn1typed_null_generated.sh
check_asn1typed_null_render_SOURCES = check_asn1typed_null_render.c
check_asn1typed_null_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_null_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_null_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_null_generated.sh check_asn1typed_null_generated.cpp fixtures/null-generation-batch.asn1 fixtures/ioc-null-batch.asn1
check_PROGRAMS += check_asn1typed_size_render
TESTS += check_asn1typed_size_generated.sh
check_asn1typed_size_render_SOURCES = check_asn1typed_size_render.c
check_asn1typed_size_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_size_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_size_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_size_generated.sh fixtures/size-generation-batch.asn1
check_PROGRAMS += check_asn1typed_domain_render
TESTS += check_asn1typed_domain_generated.sh
check_asn1typed_domain_render_SOURCES = check_asn1typed_domain_render.c
check_asn1typed_domain_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_domain_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_domain_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_domain_generated.sh check_asn1typed_domain_generated.cpp fixtures/extensible-integers-batch.asn1
check_PROGRAMS += check_asn1typed_private_render
TESTS += check_asn1typed_private_generated.sh
check_asn1typed_private_render_SOURCES = check_asn1typed_private_render.c
check_asn1typed_private_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_private_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_private_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_private_generated.sh check_asn1typed_private_generated.cpp fixtures/private-generation-batch.asn1

check_PROGRAMS += check_asn1typed_fragment_bits_render
TESTS += check_asn1typed_fragment_bits_generated.sh
check_asn1typed_fragment_bits_render_SOURCES = check_asn1typed_fragment_bits_render.c
check_asn1typed_fragment_bits_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_fragment_bits_render_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_fragment_bits_render_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += check_asn1typed_fragment_bits_generated.sh fixtures/fragment-bits-generation-batch.asn1

check_PROGRAMS += check_asn1typed_inline_constructed
check_asn1typed_inline_constructed_SOURCES = check_asn1typed_inline_constructed.c
check_asn1typed_inline_constructed_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS)
check_asn1typed_inline_constructed_LDADD = $(check_asn1typed_extract_LDADD)
TESTS += check_asn1typed_inline_constructed.sh
EXTRA_DIST += check_asn1typed_inline_constructed.sh check_asn1typed_inline_constructed.cpp fixtures/inline-constructed-f1p2.asn1

check_PROGRAMS += check_asn1typed_unsigned_render
check_asn1typed_unsigned_render_SOURCES = check_asn1typed_unsigned_render.c
check_asn1typed_unsigned_render_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DNDEBUG
check_asn1typed_unsigned_render_LDADD = $(check_asn1typed_extract_LDADD)
TESTS += check_asn1typed_unsigned_generated.sh
EXTRA_DIST += check_asn1typed_unsigned_generated.sh check_asn1typed_unsigned_generated.cpp fixtures/unsigned-integers-e1p2.asn1

check_PROGRAMS += check_asn1typed_root_graph
TESTS += check_asn1typed_root_graph
check_asn1typed_root_graph_SOURCES = check_asn1typed_root_graph.c
check_asn1typed_root_graph_CPPFLAGS = $(check_asn1typed_extract_CPPFLAGS) -DROOT_GRAPH_FIXTURE=\"$(abs_srcdir)/fixtures/ordinary-root-graph.asn1\"
check_asn1typed_root_graph_LDADD = $(check_asn1typed_extract_LDADD)
check_asn1typed_root_graph_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
EXTRA_DIST += fixtures/ordinary-root-graph.asn1
