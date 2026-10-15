/* platform.h - o pouco que muda entre Windows e Linux. */
#ifndef PLATFORM_H
#define PLATFORM_H

#include "compat.h"

#define PLAT_PATH 1024

#ifdef _WIN32
#  define PLAT_SEP      '\\'
#  define STEAM_API_LIB "steam_api64.dll"
#  define EXE_NAME      "samcli.exe"
#else
#  define PLAT_SEP      '/'
#  define STEAM_API_LIB "libsteam_api.so"
#  define EXE_NAME      "./samcli"
#endif

typedef struct { int year, month, day, hour, minute; } plat_time;

void    *plat_lib_open(const char *path);
void    *plat_lib_sym(void *lib, const char *name);

bool     plat_exe_path(char *out, size_t cap);
bool     plat_exe_dir(char *out, size_t cap);          /* com separador no final */

void     plat_setenv(const char *key, const char *value);
bool     plat_env_set(const char *key);

void     plat_sleep_ms(unsigned ms);
uint32_t plat_ticks_ms(void);

/* Roda argv[0] com os argumentos e espera. Devolve o exit code (-1 se falhou). */
int      plat_spawn_wait(const char *const *argv, int argc);

/* Prepara o terminal (UTF-8, cores). Devolve true se da pra usar sequencias ANSI. */
bool     plat_term_init(int *width);

void     plat_localtime(uint32_t unix_time, plat_time *out);

/* Pastas da Steam candidatas (raiz da instalacao). Devolve quantas achou. */
int      plat_steam_roots(char roots[][PLAT_PATH], int max);

#endif
