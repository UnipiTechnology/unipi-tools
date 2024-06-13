### TODO 
# unipi-tools package version number, (as distinct from shared library version)
#
# Take care to update the libtool versioning when required (LIBUNIPICHANNEL_LD_*).
# http://www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html
#
m4_define([unipi_tools_version_major], [2])
m4_define([unipi_tools_version_minor], [18])
m4_define([unipi_tools_version_micro], [0])

m4_define([unipi_tools_version],
    [unipi_tools_version_major.unipi_tools_version_minor.unipi_tools_version_micro])

AC_PREREQ([2.71])
AC_INIT([unipi-tools],
        [unipi_tools_version],
        [https://github.com/UniPi/unipi-tools/issues],
        [unipi-tools],
        [http://unipi.technology/]
)
AC_CONFIG_SRCDIR([lib/unipi_channel.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([check-news foreign 1.11 -Wall -Wno-portability silent-rules tar-pax subdir-objects])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

LIBUNIPICHANNEL_VERSION_MAJOR=unipi_tools_version_major
LIBUNIPICHANNEL_VERSION_MINOR=unipi_tools_version_minor
LIBUNIPICHANNEL_VERSION_MICRO=unipi_tools_version_micro
LIBUNIPICHANNEL_VERSION=unipi_tools_version
AC_SUBST(LIBUNIPICHANNEL_VERSION_MAJOR)
AC_SUBST(LIBUNIPICHANNEL_VERSION_MINOR)
AC_SUBST(LIBUNIPICHANNEL_VERSION_MICRO)
AC_SUBST(LIBUNIPICHANNEL_VERSION)

# ABI version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBUNIPICHANNEL_LD_CURRENT=0
LIBUNIPICHANNEL_LD_REVISION=1
LIBUNIPICHANNEL_LD_AGE=0
LIBUNIPICHANNEL_LT_VERSION_INFO=$LIBUNIPICHANNEL_LD_CURRENT:$LIBUNIPICHANNEL_LD_REVISION:$LIBUNIPICHANNEL_LD_AGE
AC_SUBST(LIBUNIPICHANNEL_LT_VERSION_INFO)

AC_CANONICAL_HOST

# OS check
os_win32="false"
os_cygwin="false"
os_qnx="false"
case "${host_os}" in
      *mingw32*)
    os_win32="true"
  ;;
      *nto-qnx*)
    os_qnx="true"
  ;;
      *cygwin*)
    os_cygwin="true"
  ;;
esac
AM_CONDITIONAL(OS_WIN32, test "$os_win32" = "true")
AM_CONDITIONAL(OS_QNX, test "$os_qnx" = "true")

m4_pattern_forbid([^LT_INIT])dnl
LT_INIT([disable-static win32-dll pic-only])

AC_CHECK_HEADERS([ \
    arpa/inet.h \
    byteswap.h \
    errno.h \
    fcntl.h \
    limits.h \
    linux/serial.h \
    netdb.h \
    netinet/in.h \
    netinet/tcp.h \
    sys/ioctl.h \
    sys/params.h \
    sys/socket.h \
    sys/time.h \
    sys/types.h \
    termios.h \
    time.h \
    unistd.h \
    string.h \
    modbus/modbus-tcp.h \
    systemd/sd-daemon.h \
    math.h \
    mhash.h \
])

# Cygwin defines IPTOS_LOWDELAY but can't handle that flag so it's necessary to
# workaround that problem and Cygwin doesn't define MSG_DONTWAIT.
AC_CHECK_DECLS([__CYGWIN__])

# Checks for library functions.
AC_CHECK_FUNCS([accept4 getaddrinfo gettimeofday inet_pton inet_ntop select socket strerror strlcpy \
 modbus_tcp_listen strchr sd_notify roundf mhash_init \
])

# Required for MinGW with GCC v4.8.1 on Win7
AC_DEFINE(WINVER, 0x0501, _)

# Required for bswap
AC_C_INLINE

# libtool
AC_PROG_CXX

# Various types
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

if test "$os_cygwin" = "false"; then
    # Required for getaddrinfo (TCP IP - IPv6)
    AC_CHECK_HEADERS([winsock2.h], HAVE_WINSOCK2_H=yes)
    if test "x$HAVE_WINSOCK2_H" = "xyes"; then
        LIBS="$LIBS -lws2_32"
    AC_SUBST(LIBS)
    fi
fi


# Wtype-limits is not supported by gcc 4.2 (default on recent Mac OS X)
my_CFLAGS="-Wall \
-Wmissing-declarations -Wmissing-prototypes \
-Wnested-externs -Wpointer-arith \
-Wpointer-arith -Wsign-compare -Wchar-subscripts \
-Wstrict-prototypes -Wshadow \
-Wformat-security"
AC_SUBST([my_CFLAGS])

# Build options
AC_ARG_ENABLE(tests,
	AS_HELP_STRING([--disable-tests],
	[Build tests (default: yes)]),,
	[enable_tests=yes])
AM_CONDITIONAL(BUILD_TESTS, [test $enable_tests != no])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
        Makefile
        lib/Makefile
        lib/unipichannel-version.h
        src/modbus-tools/Makefile
        src/firmware-tools/Makefile
        libunipichannel.pc
])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
     [AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files])],,
     [with_systemdsystemunitdir=auto])
AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"],
    [ def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
      AS_IF([test "x$def_systemdsystemunitdir" = "x"],
          [ AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
                [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
            with_systemdsystemunitdir=no],
          [with_systemdsystemunitdir="$def_systemdsystemunitdir"])])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
      [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        ===============

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}
        libdir:                 ${libdir}
        includedir:             ${includedir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS}
        ldflags:                ${LDFLAGS}

        tests:                  ${enable_tests}
])
