/*
 * RXDK-DotNet — hand-written eglib-config.h for i686-pc-windows-gnu (clang).
 *
 * glib.h includes <eglib-config.hw> under _MSC_VER, else <eglib-config.h> (this file). Our clang
 * uses the gnu (mingw) triple, so we take this path. Values are the eglib-config.h.in template
 * (@...@ placeholders) resolved for the Xbox target: Win32 platform conventions, little-endian
 * 32-bit x86, GCC/clang attribute spellings.
 */
#ifndef __EGLIB_CONFIG_H
#define __EGLIB_CONFIG_H

#define G_GNUC_PRETTY_FUNCTION   __PRETTY_FUNCTION__
#define G_GNUC_UNUSED            __attribute__((__unused__))
#define G_GNUC_NORETURN          __attribute__((__noreturn__))
#define G_BYTE_ORDER             1234           /* little-endian x86 */

/* Windows path conventions (the Xbox kernel uses NT-style '\' paths). */
#define G_DIR_SEPARATOR          '\\'
#define G_DIR_SEPARATOR_S        "\\"
#define G_SEARCHPATH_SEPARATOR   ';'
#define G_SEARCHPATH_SEPARATOR_S ";"

#define G_OS_WIN32 1

#define G_BREAKPOINT()           __builtin_trap()

/* picolibc provides these. */
#define G_HAVE_UNISTD_H
/* #undef G_HAVE_ALLOCA_H  — use the builtin/stdlib alloca */

typedef size_t    gsize;
typedef ptrdiff_t gssize;

#define G_GSIZE_FORMAT   "u"      /* 32-bit */

typedef void * GPid;             /* Win32 process handle */

#endif /* __EGLIB_CONFIG_H */
