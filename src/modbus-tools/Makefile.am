
AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/src/modbus-tools \
    -I${top_builddir}/lib

AM_CFLAGS = ${my_CFLAGS}

bin_PROGRAMS = r2k
sbin_PROGRAMS = unipi_tcp_server
common_ldflags = \
    $(top_builddir)/lib/libunipichannel.la

unipi_tcp_server_SOURCES = \
    unipi_tcp_server.c \
    nb_modbus.c \
    nb_modbus.h \
    virtual_regs.c \
    virtual_regs.h

unipi_tcp_server_LDADD = -lmodbus -lsystemd -lm $(common_ldflags)

r2k_SOURCES = r2k.c

r2k_LDADD = -lmodbus -lmhash

SERVICE_SUBS = \
    s,[@]sbindir[@],$(sbindir),g; \
    s,[@]sysconfdir[@],$(sysconfdir),g


if HAVE_SYSTEMD
systemdsystemunit_DATA = unipitcp.service
etcdefaultdir=$(sysconfdir)/default
etcdefault_DATA = .default/unipitcp

.default/unipitcp: unipitcp.default
	mkdir -p .default && cp unipitcp.default .default/unipitcp

unipitcp.service: unipitcp.service.in
	$(SED) -e '$(SERVICE_SUBS)' < $< > $@
endif

CLEANFILES = *~ unipitcp.service .default/unipitcp
