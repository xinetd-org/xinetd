# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ====================
# Version informations
# ====================
m4_define([xinetd_version_major],[2])
m4_define([xinetd_version_minor],[3])
m4_define([xinetd_version_micro],[16])
m4_define([xinetd_version],[xinetd_version_major.xinetd_version_minor.xinetd_version_micro])

# =============
# Automake init
# =============
AC_INIT([xinetd],[xinetd_version])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 subdir-objects foreign dist-xz dist-bzip2])
AM_SILENT_RULES([yes])
AC_LANG([C])

# ===========================
# Find required base packages
# ===========================
AC_PROG_CPP
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG([0.20])

# ======================================
# Check for various headers and settings
# ======================================
AC_CHECK_LIB(compat, main)
AC_SEARCH_LIBS(gethostbyname, nsl socket resolv)
AC_SEARCH_LIBS(getnameinfo, nsl socket resolv)
AC_SEARCH_LIBS(inet_ntop, nsl socket resolv)
AC_CHECK_FUNCS(getnameinfo inet_ntop)
AC_CHECK_FUNCS(difftime, ,
   AC_CHECK_LIB(m, difftime))
AC_CHECK_FUNCS(fcvt, ,
   AC_CHECK_LIB(m, fcvt))
AC_CHECK_FUNCS(ecvt, ,
   AC_CHECK_LIB(m, ecvt))
AC_CHECK_FUNCS(gcvt, ,
   AC_CHECK_LIB(m, gcvt))
AC_CHECK_FUNC(strerror, [AC_DEFINE(HAVE_STRERROR, 1, "")])
AC_CHECK_FUNC(strcasecmp, [AC_DEFINE(HAVE_STRCASECMP, 1, "")])
AC_CHECK_FUNC(socket, ,
   AC_CHECK_LIB(socket, socket, ,
      AC_CHECK_LIB(nsl, socket)))
AC_CHECK_FUNC(inet_aton, ,
	AC_CHECK_LIB(nsl, inet_aton, ,
		AC_CHECK_LIB(socket, inet_aton, ,
			AC_CHECK_LIB(resolv, inet_aton, ,[AC_DEFINE(NO_INET_ATON, 1, "")]))))
AC_CHECK_FUNC(setenv,[AC_DEFINE(HAVE_SETENV, 1, "")])
AC_CHECK_FUNC(strsignal, [AC_DEFINE(HAVE_STRSIGNAL, 1, "")])
AC_CHECK_LIB(c, sys_siglist, [AC_DEFINE(HAVE_SYS_SIGLIST, 1, "")])
AC_CHECK_FUNC(gai_strerror,[AC_DEFINE(HAVE_GAI_STRERROR, 1, "")])
AC_CHECK_FUNC(freeaddrinfo,[AC_DEFINE(HAVE_FREEADDRINFO, 1, "")])
AC_CHECK_FUNC(getaddrinfo,[AC_DEFINE(HAVE_GETADDRINFO, 1, "")])
AC_CHECK_HEADERS(sys/types.h sys/termios.h termios.h sys/ioctl.h sys/select.h rpc/rpc.h rpc/rpcent.h sys/file.h ftw.h machine/reg.h netdb.h)
AC_CHECK_HEADER(sys/resource.h, [AC_DEFINE(HAVE_SYS_RESOURCE_H, 1, "")])
AC_CHECK_HEADER(arpa/inet.h, [AC_DEFINE(HAVE_ARPA_INET_H, 1, "")])
AC_CHECK_HEADER(grp.h, [AC_DEFINE(HAVE_GRP_H, 1, "")])
AC_CHECK_HEADER(rpc/pmap_clnt.h, [AC_DEFINE(HAVE_RPC_PMAP_CLNT_H, 1, "")])
AC_CHECK_HEADER(sys/socket.h, [AC_DEFINE(HAVE_SYS_SOCKET_H, 1, "")])
AC_CHECK_HEADER(sys/signal.h, [AC_DEFINE(HAVE_SYS_SIGNAL_H, 1, "")])
AC_CHECK_HEADER(crypt.h, [AC_DEFINE(HAVE_CRYPT_H, 1, "")])
AC_CHECK_HEADER(stdint.h, [AC_DEFINE(HAVE_STDINT_H, 1, "")])
AC_CHECK_HEADER(stdbool.h, [AC_DEFINE(HAVE_STDBOOL_H, 1, "")])
AC_CHECK_HEADER(sys/filio.h, [AC_DEFINE(HAVE_SYS_FILIO_H, 1, "")])
AC_CHECK_HEADER(DNSServiceDiscovery/DNSServiceDiscovery.h, [AC_DEFINE(HAVE_DNSREGISTRATION, 1, "") AC_DEFINE(HAVE_MDNS, 1, "")])
XINETD_CHECK_TYPE(uint16_t, unsigned short)
XINETD_CHECK_TYPE(uint32_t, unsigned int)
XINETD_CHECK_TYPE(uint64_t, unsigned long long)
XINETD_CHECK_TYPE(uid_t, unsigned int)
XINETD_CHECK_TYPE(gid_t, unsigned int)
XINETD_CHECK_TYPE(socklen_t, unsigned int)
XINETD_CHECK_TYPE(rlim_t, long long)
AC_FUNC_MMAP
AC_CHECK_FUNCS(isatty)
AC_CHECK_FUNCS(memcpy)
AC_CHECK_FUNCS(waitpid)
AC_CHECK_FUNCS(sigvec)
AC_CHECK_FUNCS(setsid)
AC_CHECK_FUNCS(strftime)
AC_CHECK_LIB(c, crypt, [:], [AC_CHECK_LIB(crypt, crypt, [LIBS="-lcrypt $LIBS" AC_DEFINE(HAVE_LIBCRYPT, 1, "") ], []) ])
AC_CHECK_LIB(m, log10, [ LIBS="-lm $LIBS" ], [])
AC_CHECK_LIB(crypt, main)

# ========
# Addrinfo
# ========
AC_CACHE_CHECK([for struct addrinfo], ac_cv_have_struct_addrinfo, [
        AC_TRY_COMPILE(
                [
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
                ],
                [ struct addrinfo s; s.ai_flags = AI_PASSIVE; ],
                [ ac_cv_have_struct_addrinfo="yes" ],
                [ ac_cv_have_struct_addrinfo="no" ]
        )
])
if test "x$ac_cv_have_struct_addrinfo" = "xyes" ; then
        AC_DEFINE(HAVE_STRUCT_ADDRINFO, 1, "")
fi

AC_CACHE_CHECK([for struct in6_addr], ac_cv_have_struct_in6_addr, [
        AC_TRY_COMPILE(
                [
#include <sys/types.h>
#include <netinet/in.h>
                ],
                [ struct in6_addr s; s.s6_addr[0] = 0; ],
                [ ac_cv_have_struct_in6_addr="yes" ],
                [ ac_cv_have_struct_in6_addr="no" ]
        )
])
if test "x$ac_cv_have_struct_in6_addr" = "xyes" ; then
        AC_DEFINE(HAVE_STRUCT_IN6_ADDR, 1, "")
fi

AC_CACHE_CHECK([for struct sockaddr_in6], ac_cv_have_struct_sockaddr_in6, [
        AC_TRY_COMPILE(
                [
#include <sys/types.h>
#include <netinet/in.h>
                ],
                [ struct sockaddr_in6 s; s.sin6_family = 0; ],
                [ ac_cv_have_struct_sockaddr_in6="yes" ],
                [ ac_cv_have_struct_sockaddr_in6="no" ]
        )
])
if test "x$ac_cv_have_struct_sockaddr_in6" = "xyes" ; then
        AC_DEFINE(HAVE_STRUCT_SOCKADDR_IN6, 1, "")
fi

AC_CACHE_CHECK([for struct sockaddr_storage], ac_cv_have_struct_sockaddr_storage, [
        AC_TRY_COMPILE(
                [
#include <sys/types.h>
#include <sys/socket.h>
                ],
                [ struct sockaddr_storage s; ],
                [ ac_cv_have_struct_sockaddr_storage="yes" ],
                [ ac_cv_have_struct_sockaddr_storage="no" ]
        )
])
if test "x$ac_cv_have_struct_sockaddr_storage" = "xyes" ; then
        AC_DEFINE(HAVE_STRUCT_SOCKADDR_STORAGE, 1, "")
fi

# ======
# Switch
# ======
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [Build with debug symbols.]),
	[enable_debug="$enableval"],
	[enable_debug=no]
)
AS_IF([test "x$enable_debug" != "xno"], [
	CFLAGS="$CFLAGS -g"
], [
	AC_DEFINE([NDEBUG], [], [Disable debugging information])
])

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror], [Treat all warnings as errors, useful for development])],
	[enable_werror="$enableval"],
	[enable_werror=no]
)
AS_IF([test x"$enable_werror" != "xno"], [
	CFLAGS="$CFLAGS -Werror"
])
AS_IF([test x"$GCC" = xyes], [
	# Be tough with warnings and produce less careless code
	CFLAGS="$CFLAGS -Wall -Wextra -pedantic"
])

AC_ARG_WITH([howl],
    [AS_HELP_STRING([--with-howl], [Build with howl support.])],
    [with_howl="$withval"],
    [with_howl=no]
)
AS_IF([test x"$with_howl" != "xno"], [
    PKG_CHECK_MODULES([HOWL],[howl], [
        AC_DEFINE(HAVE_HOWL, 1, "")
        AC_DEFINE(HAVE_MDNS, 1, "")
    ],[])
])

AC_ARG_WITH([loadavg],
    [AS_HELP_STRING([--without-loadavg], [Do not use loadavg for balancing])],
    [with_loadavg="$withval"],
    [with_loadavg=yes]
)
AS_IF([test x"$with_loadavg" != "xno"], [
    AC_DEFINE(HAVE_LOADAVG, 1, "")
])

AC_MSG_CHECKING(whether to use libwrap)
AC_ARG_WITH(libwrap,
[  --with-libwrap[=PATH]   Compile in libwrap (tcp_wrappers) support.],
[ case "$withval" in
	no)
		AC_MSG_RESULT(no)
		;;
	yes)
		AC_MSG_RESULT(yes)
		AC_CHECK_LIB(wrap, request_init, [
			AC_DEFINE(LIBWRAP, 1, "")
			WRAPLIBS="-lwrap"
			AC_DEFINE(HAVE_LIBWRAP, 1, "") ])
		AC_CHECK_LIB(nsl, yp_get_default_domain, [ 
			WRAPLIBS="$WRAPLIBS -lnsl" ])
		LIBS="$WRAPLIBS $LIBS"
		;;
	*)
		AC_MSG_RESULT(yes)
		AC_DEFINE(LIBWRAP, 1, "")
		if test -d "$withval"; then
			WRAPLIBS="-L$withval -lwrap"
		else
			WRAPLIBS="$withval"
		fi
		AC_CHECK_LIB(nsl, yp_get_default_domain, [ 
			WRAPLIBS="$WRAPLIBS -lnsl" ])
		OLDLIBS="$LIBS"
		LIBS="$WRAPLIBS $LIBS"
		AC_TRY_LINK([ int allow_severity; int deny_severity; ], 
			[ hosts_access(); ], [], [ AC_MSG_ERROR(Could not find the $withval library.  You must first install tcp_wrappers.) ])
		;;
	esac ],
	AC_MSG_RESULT(no)
)

AC_MSG_CHECKING(whether to use labeled-networking)
AC_ARG_WITH(labeled-networking,
[  --with-labeled-networking[=PATH]   Compile in labeled networking support.],
[ case "$withval" in
	no)
		AC_MSG_RESULT(no)
		;;
	yes)
		AC_MSG_RESULT(yes)
		AC_CHECK_LIB(selinux, setexeccon, [
			AC_DEFINE(LABELED_NET, 1, "")
			LABELLIBS="-lselinux" ])
		LIBS="$LABELLIBS $LIBS"
		;;
	*)
		AC_MSG_RESULT(yes)
		AC_DEFINE(LABELED_NET, 1, "")
		if test -d "$withval"; then
			LABELLIBS="-L$withval -lselinux"
		else
			LABELLIBS="$withval"
		fi
		LIBS="$LABELLIBS $LIBS"
		;;
	esac ],
	AC_MSG_RESULT(no)
)

# ==============
# Setup config.h
# ==============
AC_CONFIG_HEADERS([config.h])

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
Makefile
])
AC_OUTPUT

# ==============================================
# Display final informations about configuration
# ==============================================
AC_MSG_NOTICE([
==============================================================================
Build configuration:
	debug:              ${enable_debug}
	werror:             ${enable_werror}
	howl:               ${with_howl}
	loadavg:            ${with_loadavg}
==============================================================================
])
