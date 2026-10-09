noinst_LTLIBRARIES = libaper.la
libaper_la_SOURCES = runtime.cpp runtime.hpp
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
