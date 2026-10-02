AM_CFLAGS = @ADD_CFLAGS@

noinst_LTLIBRARIES = libasn1typed.la
libasn1typed_la_SOURCES = asn1typed.c asn1typed.h

check_PROGRAMS = check_asn1typed
TESTS = $(check_PROGRAMS)
check_asn1typed_SOURCES = check_asn1typed.c
check_asn1typed_LDADD = libasn1typed.la
