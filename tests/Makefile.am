AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/test \
    -I${top_builddir}/lib

AM_CFLAGS = ${my_CFLAGS}

noinst_PROGRAMS = test-channel bandwidth-client

common_ldflags = \
    $(top_builddir)/lib/libunipichannel.la

test_channel_SOURCES = \
    test-channel.c

test_channel_LDADD = -lm $(common_ldflags)

bandwidth_client_SOURCES = bandwidth-client.c

bandwidth_client_LDADD = -lmodbus

CLEANFILES = *~
