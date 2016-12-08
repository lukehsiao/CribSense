AC_INIT([Cribsense], [1.0])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign subdir-objects])

AC_CANONICAL_HOST

AX_CHECK_ENABLE_DEBUG([yes])
AC_USE_SYSTEM_EXTENSIONS

LT_INIT
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([11], [ext], [mandatory])
AX_COMPILER_FLAGS()

ARCH_CXXFLAGS=
AS_CASE([$host_cpu],
    [*arm*], [ARCH_CXXFLAGS="-mfloat-abi=hard -mtune=cortex-a53 -mfpu=crypto-neon-fp-armv8"],
    []
)
AC_SUBST([ARCH_CXXFLAGS])

PKG_CHECK_MODULES(DEPS,
                  opencv libcanberra)

AC_CONFIG_FILES([
  Makefile
])

AC_OUTPUT
