noinst_LTLIBRARIES = libuper.la
libuper_la_SOURCES = runtime.cpp runtime.hpp
AM_CXXFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion $(APER_CXXFLAGS)
override CXXFLAGS += -std=c++20
check_PROGRAMS = check_uper_runtime
TESTS = check_uper_runtime
check_uper_runtime_SOURCES = check_runtime.cpp
check_uper_runtime_CPPFLAGS = -I$(srcdir) -DNDEBUG
check_uper_runtime_LDADD = libuper.la
check_PROGRAMS += check_uper_coexistence
TESTS += check_uper_coexistence
check_uper_coexistence_SOURCES = check_coexistence.cpp
check_uper_coexistence_LDADD = libuper.la ../libaper/libaper.la
