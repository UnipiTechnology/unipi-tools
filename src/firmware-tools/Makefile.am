EXTRA_DIST =

AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/src/firmware-tools \
    -I${top_srcdir}/lib

AM_CFLAGS = ${my_CFLAGS}

common_ldflags = \
	$(top_builddir)/lib/libunipichannel.la

bin_PROGRAMS = fwserial fwspi fwi2c

fwserial_SOURCES = \
    fw.c fwimage.c fwconfig.c fwopt.c fwdriverserial.c fwbin.c unipiimg.c platform.c
fwserial_CFLAGS = -DFWSERIAL
fwserial_LDADD = -lmodbus $(common_ldflags)

fwspi_SOURCES = \
    fw.c fwimage.c fwconfig.c fwopt.c fwdriverspi.c fwbin.c unipiimg.c platform.c
fwspi_CFLAGS = -DFWSPI
fwspi_LDADD = -lmodbus $(common_ldflags)

fwi2c_SOURCES = \
    fw.c fwimage.c fwconfig.c fwopt.c fwdriveri2c.c fwbin.c binary_data.c unipiimg.c platform.c
fwi2c_CFLAGS = -DFWI2C
fwi2c_LDADD = -li2c $(common_ldflags)

DISTCLEANFILES =
EXTRA_DIST += 
CLEANFILES = *~ unipifwupdate.service


SERVICE_SUBS = \
    s,[@]bindir[@],$(bindir),g; \
    s,[@]sysconfdir[@],$(sysconfdir),g


if HAVE_SYSTEMD
systemdsystemunit_DATA = unipifwupdate.service
endif

unipifwupdate.service: unipifwupdate.service.in
	$(SED) -e '$(SERVICE_SUBS)' < $< > $@

