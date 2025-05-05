
AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/src/modbus-tools \
    -I${top_srcdir}/lib

AM_CFLAGS = ${my_CFLAGS}

bin_PROGRAMS = r2k
sbin_PROGRAMS = unipitcp
common_ldflags = \
    $(top_builddir)/lib/libunipichannel.la

unipitcp_SOURCES = \
    unipitcp.c \
    nb_modbus.c \
    nb_modbus.h \
    virtual_regs.c \
    virtual_regs.h \
    virtual_leds.c \
    virtual_leds.h

unipitcp_LDADD = -lmodbus -lm $(common_ldflags)

r2k_SOURCES = r2k.c

r2k_LDADD = -lmodbus -lmhash

SERVICE_SUBS = \
    s,[@]sbindir[@],$(sbindir),g; \
    s,[@]sysconfdir[@],$(sysconfdir),g


etcdefaultdir=$(sysconfdir)/default
etcdefault_DATA = .default/unipitcp

.default/unipitcp: $(srcdir)/unipitcp.default
	mkdir -p .default && cp $(srcdir)/unipitcp.default .default/unipitcp

if HAVE_SYSTEMD
unipitcp_LDADD += -lsystemd
systemdsystemunit_DATA = unipitcp.service

unipitcp.service: $(srcdir)/unipitcp.service.in
	$(SED) -e '$(SERVICE_SUBS)' < $< > $@

else
init_ddir = $(sysconfdir)/init.d
init_d_SCRIPTS = .init/unipitcp

.init/unipitcp: $(srcdir)/unipitcp.init.in
	mkdir -p .init && $(SED) -e '$(SERVICE_SUBS)' < $< > $@
	chmod +x $@

endif

CLEANFILES = *~ $(systemdsystemunit_DATA) $(etcdefault_DATA) $(init_d_SCRIPTS)
