# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ====================
# Version informations
# ====================
m4_define([xinetd_version_major],[2])
m4_define([xinetd_version_minor],[3])
m4_define([xinetd_version_micro],[15])
m4_define([xinetd_version_nano],[1])
m4_define([xinetd_version],[xinetd_version_major.xinetd_version_minor.xinetd_version_micro.xinetd_version_nano])

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
AC_CHECK_LIB([m], [log10], [
	LIBM_LIBS="-lm"
], [
	AC_MSG_ERROR([Unable to find working libm.so])
])
AC_SUBST([LIBM_LIBS])
PKG_CHECK_MODULES([TIRPC],[libtirpc], [
	AC_DEFINE([HAVE_RPCENT_H], [1], [Have <rpc/rpcent.h>.])
], [:])

# ======================================
# Check for various headers and settings
# ======================================
AC_FUNC_MMAP
AC_CHECK_HEADER(sys/filio.h, [AC_DEFINE([HAVE_SYS_FILIO_H], [1], ["Have FreeBSD filio.h"])])
AC_CHECK_FUNC(poll, [AC_DEFINE([HAVE_POLL], [1], ["Have available poll function"])])

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
])

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--enable-werror], [Treat all warnings as errors, useful for development])],
	[enable_werror="$enableval"],
	[enable_werror=no]
)
AS_IF([test x"$enable_werror" != "xno"], [
	CFLAGS="$CFLAGS -Werror"
])
CFLAGS="$CFLAGS -Wall -Wextra"

AC_ARG_WITH([loadavg],
    [AS_HELP_STRING([--without-loadavg], [Do not use loadavg for balancing])],
    [with_loadavg="$withval"],
    [with_loadavg=yes]
)
AS_IF([test x"$with_loadavg" != "xno"], [
    AC_DEFINE([HAVE_LOADAVG], [1], ["Have loadavg for balancing"])
])

AC_ARG_WITH([libwrap],
    [AS_HELP_STRING([--without-libwrap], [Do not use libwrap tcp_wrappers support])],
    [with_libwrap="$withval"],
    [with_libwrap=yes]
)
AS_IF([test x"$with_libwrap" != "xno"], [
    AC_CHECK_LIB(wrap, request_init, [
        AC_DEFINE([LIBWRAP], [1], ["Have libwrap TCP wrappers present"])
        WRAP_LIBS="-lwrap"
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
    ],[])
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
	labeled-networking: ${with_labeled_networking}
	libwrap:            ${with_libwrap}
	loadavg:            ${with_loadavg}
	HARDEN_CFLAGS:      ${HARDEN_CFLAGS}
	HARDEN_LDFLAGS:     ${HARDEN_LDFLAGS}
==============================================================================
])
