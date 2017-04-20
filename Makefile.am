ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	contrib/ \
	README.md \
	CHANGELOG \
	$(bin_SCRIPTS)

AM_LDFLAGS = $(LDFLAGS) $(HARDEN_LDFLAGS)
AM_CFLAGS = $(CFLAGS) $(HARDEN_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/misc \
	-I$(top_srcdir)/src/portable \
	-I$(top_srcdir)/src/pset \
	-I$(top_srcdir)/src/sio \
	-I$(top_srcdir)/src/str \
	-I$(top_srcdir)/src/xlog

noinst_LTLIBRARIES = \
	libmisc.la \
	libportable.la \
	libpset.la \
	libsio.la \
	libstr.la \
	libxlog.la
libmisc_la_SOURCES = \
	src/misc/m_env.c \
	src/misc/m_env.h \
	src/misc/misc.c \
	src/misc/misc.h
libportable_la_SOURCES = \
	src/portable/compat.h \
	src/portable/cvt.c \
	src/portable/difftime.c \
	src/portable/fake-getaddrinfo.c \
	src/portable/fake-getaddrinfo.h \
	src/portable/fake-getnameinfo.c \
	src/portable/inet_aton.c \
	src/portable/inet_ntop.c \
	src/portable/libportable.h \
	src/portable/strcasecmp.c \
	src/portable/strerror.c
libpset_la_SOURCES = \
	src/pset/ops.c \
	src/pset/pset.c \
	src/pset/pset.h
libsio_la_SOURCES = \
	src/sio/impl.h \
	src/sio/sio.c \
	src/sio/sioconf.h \
	src/sio/sio.h \
	src/sio/siosup.c \
	src/sio/sprint.c
libstr_la_SOURCES = \
	src/str/str.h \
	src/str/strparse.c \
	src/str/strparse.h \
	src/str/strprint.c \
	src/str/strutil.c
libxlog_la_SOURCES = \
	src/xgetloadavg.c \
	src/xgetloadavg.h \
	src/xlog/filelog.c \
	src/xlog/filelog.h \
	src/xlog/impl.h \
	src/xlog/slog.c \
	src/xlog/slog.h \
	src/xlog/util.c \
	src/xlog/xlog.c \
	src/xlog/xlog.h

bin_PROGRAMS = itox
itox_SOURCES = src/itox.c
itox_LDADD = \
	libsio.la \
	libstr.la

bin_SCRIPTS = src/xconv.pl

sbin_PROGRAMS = xinetd
xinetd_CFLAGS = \
	$(HOWL_CFLAGS) \
	$(HARDEN_CFLAGS)
xinetd_SOURCES = \
	src/access.c \
	src/access.h \
	src/addr.c \
	src/addr.h \
	src/attr.h \
	src/builtins.c \
	src/builtins.h \
	src/conf.c \
	src/conf.h \
	src/confparse.c \
	src/confparse.h \
	src/connection.c \
	src/connection.h \
	src/defs.h \
	src/env.c \
	src/env.h \
	src/child.c \
	src/child.h \
	src/ident.c \
	src/ident.h \
	src/includedir.c \
	src/includedir.h \
	src/inet.c \
	src/inet.h \
	src/init.c \
	src/init.h \
	src/int.c \
	src/intcommon.c \
	src/intcommon.h \
	src/internals.c \
	src/internals.h \
	src/int.h \
	src/log.c \
	src/logctl.c \
	src/logctl.h \
	src/log.h \
	src/main.c \
	src/main.h \
	src/mask.h \
	src/msg.c \
	src/msg.h \
	src/nvlists.c \
	src/nvlists.h \
	src/options.c \
	src/options.h \
	src/parse.c \
	src/parse.h \
	src/parsers.c \
	src/parsers.h \
	src/parsesup.c \
	src/parsesup.h \
	src/reconfig.c \
	src/reconfig.h \
	src/redirect.c \
	src/redirect.h \
	src/retry.c \
	src/retry.h \
	src/sconf.c \
	src/sconf.h \
	src/sconst.h \
	src/sensor.c \
	src/sensor.h \
	src/server.c \
	src/server.h \
	src/service.c \
	src/service.h \
	src/signals.c \
	src/signals.h \
	src/special.c \
	src/special.h \
	src/state.h \
	src/tcpint.c \
	src/tcpint.h \
	src/time.c \
	src/timex.h \
	src/udpint.c \
	src/udpint.h \
	src/util.c \
	src/util.h \
	src/xconfig.h \
	src/xmdns.c \
	src/xmdns.h \
	src/xtimer.c \
	src/xtimer.h
xinetd_LDADD = \
	libmisc.la \
	libportable.la \
	libpset.la \
	libsio.la \
	libstr.la \
	libxlog.la \
	$(HOWL_LIBS) \
	$(WRAP_LIBS)

distclean-local:
	rm -rf *.cache *~
