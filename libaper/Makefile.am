noinst_LTLIBRARIES = libaper.la
libaper_la_SOURCES = runtime.cpp runtime.hpp sequence_extensions.hpp
AM_CXXFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion $(APER_CXXFLAGS)
override CXXFLAGS += -std=c++20

check_PROGRAMS = check_aper_runtime
TESTS = check_aper_runtime
check_aper_runtime_SOURCES = check_runtime.cpp
check_aper_runtime_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_runtime_LDADD = libaper.la

check_PROGRAMS += check_aper_extensions
TESTS += check_aper_extensions
check_aper_extensions_SOURCES = check_extensions.cpp
check_aper_extensions_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_extensions_LDADD = libaper.la

check_PROGRAMS += check_aper_collections
TESTS += check_aper_collections
check_aper_collections_SOURCES = check_collections.cpp
check_aper_collections_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_collections_LDADD = libaper.la

check_PROGRAMS += check_aper_known_open
TESTS += check_aper_known_open
check_aper_known_open_SOURCES = check_known_open.cpp
check_aper_known_open_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_known_open_LDADD = libaper.la

check_PROGRAMS += check_aper_octets
TESTS += check_aper_octets
check_aper_octets_SOURCES = check_octets.cpp
check_aper_octets_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_octets_LDADD = libaper.la

check_PROGRAMS += check_aper_bits
TESTS += check_aper_bits
check_aper_bits_SOURCES = check_bits.cpp
check_aper_bits_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_bits_LDADD = libaper.la

check_PROGRAMS += check_aper_integer
TESTS += check_aper_integer
check_aper_integer_SOURCES = check_integer.cpp
check_aper_integer_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_aper_integer_LDADD = libaper.la
