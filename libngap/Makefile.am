noinst_LTLIBRARIES = libngap.la
libngap_la_SOURCES = pdu.cpp pdu.hpp f1ap_pdu.cpp f1ap_pdu.hpp pdu_declarations.inc pdu_implementation.inc
AM_CPPFLAGS = -I$(top_srcdir)/libaper
AM_CXXFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion $(APER_CXXFLAGS)
override CXXFLAGS += -std=c++20
check_PROGRAMS = check_ngap_pdu check_f1ap_pdu
TESTS = check_ngap_pdu check_f1ap_pdu
check_ngap_pdu_SOURCES = check_pdu.cpp
check_ngap_pdu_CPPFLAGS = $(AM_CPPFLAGS) -DNDEBUG
check_ngap_pdu_LDADD = libngap.la $(top_builddir)/libaper/libaper.la
check_f1ap_pdu_SOURCES = check_f1ap_pdu.cpp
check_f1ap_pdu_CPPFLAGS = $(AM_CPPFLAGS) -DNDEBUG
check_f1ap_pdu_LDADD = libngap.la $(top_builddir)/libaper/libaper.la
