ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	contrib/ \
	README.md \
	CHANGELOG

AM_CFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/misc \
	-I$(top_srcdir)/src/portable \
	-I$(top_srcdir)/src/pset \
	-I$(top_srcdir)/src/sio \
	-I$(top_srcdir)/src/str \
	-I$(top_srcdir)/src/xlog

bin_PROGRAMS = itox
itox_SOURCES = \
	src/itox.c

bin_SCRIPTS = src/xconv.pl

sbin_PROGRAMS = xinetd
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
	src/misc/m_env.c \
	src/misc/m_env.h \
	src/misc/misc.c \
	src/misc/misc.h \
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
	src/portable/strerror.c \
	src/pset/ops.c \
	src/pset/pset.c \
	src/pset/pset.h \
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
	src/sio/impl.h \
	src/sio/sio.c \
	src/sio/sioconf.h \
	src/sio/sio.h \
	src/sio/siosup.c \
	src/sio/sprint.c \
	src/special.c \
	src/special.h \
	src/state.h \
	src/str/str.h \
	src/str/strparse.c \
	src/str/strparse.h \
	src/str/strprint.c \
	src/str/strutil.c \
	src/tcpint.c \
	src/tcpint.h \
	src/time.c \
	src/timex.h \
	src/udpint.c \
	src/udpint.h \
	src/util.c \
	src/util.h \
	src/xconfig.h \
	src/xgetloadavg.c \
	src/xgetloadavg.h \
	src/xlog/filelog.c \
	src/xlog/filelog.h \
	src/xlog/impl.h \
	src/xlog/slog.c \
	src/xlog/slog.h \
	src/xlog/util.c \
	src/xlog/xlog.c \
	src/xlog/xlog.h \
	src/xmdns.c \
	src/xmdns.h \
	src/xtimer.c \
	src/xtimer.h

distclean-local:
	rm -rf *.cache *~
