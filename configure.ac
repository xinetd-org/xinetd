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
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG([0.20])

# ======================================
# Check for various headers and settings
# ======================================
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

# ======
# Switch
# ======
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [Build with debug symbols.]),
	[enable_debug="$enableval"],
	[enable_debug=no]
)
AS_IF([test "x$enable_debug" != "xno"], [
	CFLAGS="$CFLAGS -ggdb"
], [])

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
    ],[AC_MSG_ERROR([Missing how library, probably missing mdns-compat avahi development package])])
])

AC_ARG_WITH([loadavg],
    [AS_HELP_STRING([--without-loadavg], [Do not use loadavg for balancing])],
    [with_loadavg="$withval"],
    [with_loadavg=yes]
)
AS_IF([test x"$with_loadavg" != "xno"], [
    AC_DEFINE(HAVE_LOADAVG, 1, "")
])

AC_ARG_WITH([libwrap],
    [AS_HELP_STRING([--without-libwrap], [Do not use libwrap tcp_wrappers support])],
    [with_libwrap="$withval"],
    [with_libwrap=yes]
)
AS_IF([test x"$with_libwrap" != "xno"], [
    AC_CHECK_LIB(wrap, request_init, [
        AC_DEFINE(LIBWRAP, 1, "")
        WRAP_LIBS="-lwrap"
        AC_DEFINE(HAVE_LIBWRAP, 1, "")
    ],[
        AC_MSG_ERROR([Missing libwrap from tcp_wrappers])
    ])
    AC_CHECK_LIB(nsl, yp_get_default_domain, [WRAP_LIBS="$WRAP_LIBS -lnsl" ])
    AC_SUBST([WRAP_LIBS])
])

AC_ARG_WITH([labeled-networking],
    [AS_HELP_STRING([--without-labeled-networking], [Do not use selinux for labeled networking])],
    [with_labeled_networking="$withval"],
    [with_labeled_networking=yes]
)
AS_IF([test x"$with_labeled_networking" != "xno"], [
    PKG_CHECK_MODULES([SELINUX],[libselinux], [
        AC_DEFINE(LABELED_NET, 1, "")
    ],[AC_MSG_ERROR([Missing selinux libraries])])
])

# =========
# Hardening
# =========
# We use the same hardening flags for C and C++.  We must check that each flag
# is supported by both compilers.
AC_DEFUN([check_cc_cxx_flag],
 [AC_LANG_PUSH(C)
  AX_CHECK_COMPILE_FLAG([$1], [$2], [$3], [-Werror $4])
  AC_LANG_POP(C)])
AC_DEFUN([check_link_flag],
 [AX_CHECK_LINK_FLAG([$1], [$2], [$3], [-Werror $4])])
HARDEN_CFLAGS=""
HARDEN_LDFLAGS=""
check_cc_cxx_flag([-fno-strict-overflow], [HARDEN_CFLAGS="$HARDEN_CFLAGS -fno-strict-overflow"])

# This one will likely succeed, even on platforms where it does nothing.
check_cc_cxx_flag([-D_FORTIFY_SOURCE=2], [HARDEN_CFLAGS="$HARDEN_CFLAGS -D_FORTIFY_SOURCE=2"])

check_cc_cxx_flag([-fstack-protector-all],
[check_link_flag([-fstack-protector-all],
 [HARDEN_CFLAGS="$HARDEN_CFLAGS -fstack-protector-all"
  check_cc_cxx_flag([-Wstack-protector], [HARDEN_CFLAGS="$HARDEN_CFLAGS -Wstack-protector"],
    [], [-fstack-protector-all])
  check_cc_cxx_flag([--param ssp-buffer-size=1], [HARDEN_CFLAGS="$HARDEN_CFLAGS --param ssp-buffer-size=1"],
    [], [-fstack-protector-all])])])

# At the link step, we might want -pie (GCC) or -Wl,-pie (Clang on OS X)
#
# The linker checks also compile code, so we need to include -fPIE as well.
check_cc_cxx_flag([-fPIE],
[check_link_flag([-fPIE -pie],
 [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
  HARDEN_LDFLAGS="$HARDEN_LDFLAGS -pie"],
 [check_link_flag([-fPIE -Wl,-pie],
   [HARDEN_CFLAGS="$HARDEN_CFLAGS -fPIE"
    HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-pie"])])])

check_link_flag([-Wl,-z,relro],
[HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,relro"
check_link_flag([-Wl,-z,now], [HARDEN_LDFLAGS="$HARDEN_LDFLAGS -Wl,-z,now"])])
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])
AC_SUBST([CFLAGS])

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
	labeled-networking: ${with_labeled_networking}
	libwrap:            ${with_libwrap}
	loadavg:            ${with_loadavg}
	HARDEN_CFLAGS:      ${HARDEN_CFLAGS}
	HARDEN_LDFLAGS:     ${HARDEN_LDFLAGS}
==============================================================================
])
