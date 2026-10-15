/* compat.h - includes de sistema e constantes do projeto. */
#ifndef COMPAT_H
#define COMPAT_H

#if defined(_WIN32) && !defined(SAMCLI_MINI)
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601   /* Windows 7+: RegGetValueA */
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "advapi32.lib")
#  endif
#elif !defined(_WIN32)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE           /* realpath, localtime_r, setenv */
#  endif
#  include <dlfcn.h>
#  include <errno.h>
#  include <sys/ioctl.h>
#  include <sys/wait.h>
#  include <time.h>
#  include <unistd.h>
#endif

#ifndef SAMCLI_MINI
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#  include <stdarg.h>
#  include <stdbool.h>
#  include <stdint.h>
#endif

#define SAMCLI_VERSION  "1.2.0"
#define BRAND_NAME      "CentralTech"
#define BRAND_SITE      "https://centraltechpc.com/"
#define BRAND_SITE_TXT  "centraltechpc.com"
#define BRAND_GH        "https://github.com/grimh3xx"
#define BRAND_GH_TXT    "github.com/grimh3xx"

#endif
