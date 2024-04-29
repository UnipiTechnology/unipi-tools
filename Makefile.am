CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libunipichannel.pc
EXTRA_DIST = libunipichannel.pc.in
CLEANFILES += libunipichannel.pc

dist_doc_DATA = AUTHORS README.md

SUBDIRS = lib src/modbus-tools src/firmare-tools

#if BUILD_TESTS
#SUBDIRS += tests
#endif
