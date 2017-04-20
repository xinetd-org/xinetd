ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	contrib/empty.conf \
	contrib/xinetd \
	contrib/xinetd.service \
	README.md \
	CHANGELOG \
	$(bin_SCRIPTS) \
	$(man_MANS) \
	$(sysconf_DATA) \
	$(xinetconf_DATA)

sysconf_DATA = \
	contrib/xinetd.conf

xinetconfdir=$(sysconfdir)/xinet.d
xinetconf_DATA = \
	contrib/xinetd.d/daytime \
	contrib/xinetd.d/daytime-udp \
	contrib/xinetd.d/discard \
	contrib/xinetd.d/discard-udp \
	contrib/xinetd.d/echo \
	contrib/xinetd.d/echo-udp \
	contrib/xinetd.d/chargen \
	contrib/xinetd.d/chargen-udp \
	contrib/xinetd.d/servers \
	contrib/xinetd.d/services \
	contrib/xinetd.d/time \
	contrib/xinetd.d/time-udp

man_MANS = \
	man/itox.8 \
	man/m_env.3 \
	man/misc.3 \
	man/pset.3 \
	man/psi.3 \
	man/sio.3 \
	man/Sprint.3 \
	man/strparse.3 \
	man/strprint.3 \
	man/strutil.3 \
	man/xconv.pl.8 \
	man/xinetd.conf.5 \
	man/xinetd.log.5 \
	man/xinetd.8 \
	man/xlog.3

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
	src/portable/cvt.c \
	src/portable/libportable.h \
	src/portable/strcasecmp.c
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
	$(HARDEN_CFLAGS) \
	$(SELINUX_CFLAGS) \
	$(TIRPC_CFLAGS)
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
	$(WRAP_LIBS) \
	$(SELINUX_LIBS) \
	$(TIRPC_LIBS)

distclean-local:
	rm -rf *.cache *~
