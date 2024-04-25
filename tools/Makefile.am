EXTRA_DIST =

AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/tools \
    -I${top_builddir}/unipichannel

AM_CFLAGS = ${my_CFLAGS}

bin_PROGRAMS = unipi_tcp_server r2k
common_ldflags = \
	$(top_builddir)/unipichannel/libunipichannel.la

unipi_tcp_server_SOURCES = \
    unipi_tcp_server.c \
    nb_modbus.c \
    nb_modbus.h \
    virtual_regs.c \
    virtual_regs.h

unipi_tcp_server_LDADD = -lmodbus -lsystemd -lm $(common_ldflags)

r2k_SOURCES = r2k.c

r2k_LDADD = -lmodbus -lmhash


CLEANFILES = *~
