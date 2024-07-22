EXTRA_DIST =
lib_LTLIBRARIES = libunipichannel.la
dist_doc_DATA = COPYING.LESSER

AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/unipichannel

AM_CFLAGS = ${my_CFLAGS}

libunipichannel_la_SOURCES = \
        armspi.c \
        armspi.h \
        armutil.c \
        armutil.h \
        kchannel.h \
        spicrc.c \
        spicrc.h \
        unipi_channel.c \
        unipichannel-version.h

libunipichannel_la_LDFLAGS = -no-undefined \
        -version-info $(LIBUNIPICHANNEL_LT_VERSION_INFO)

# Header files to install
libunipichannelincludedir = $(includedir)/unipichannel
libunipichannelinclude_HEADERS = kchannel.h unipichannel-version.h armutil.h


DISTCLEANFILES = unipichannel-version.h
EXTRA_DIST += unipichannel-version.h.in
CLEANFILES = *~

