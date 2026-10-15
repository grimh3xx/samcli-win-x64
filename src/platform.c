/*
 * platform.c - implementacao de platform.h para Windows (Win32) e
 * Linux/POSIX. Todo o resto do samcli e codigo comum.
 */
#include "platform.h"

#ifdef _WIN32
/* ================================================================== */
/* Windows                                                            */
/* ================================================================== */

void *plat_lib_open(const char *path) { return (void *)LoadLibraryA(path); }
void *plat_lib_sym(void *lib, const char *name) { return (void *)GetProcAddress((HMODULE)lib, name); }

bool plat_exe_path(char *out, size_t cap)
{
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    return n > 0 && n < cap;
}

bool plat_exe_dir(char *out, size_t cap)
{
    if (!plat_exe_path(out, cap)) return false;
    char *slash = strrchr(out, '\\');
    if (!slash) return false;
    slash[1] = '\0';
    return true;
}

void plat_setenv(const char *key, const char *value) { SetEnvironmentVariableA(key, value); }

bool plat_env_set(const char *key)
{
    char tmp[4];
    return GetEnvironmentVariableA(key, tmp, sizeof tmp) > 0;
}

void     plat_sleep_ms(unsigned ms) { Sleep(ms); }
uint32_t plat_ticks_ms(void) { return (uint32_t)GetTickCount(); }

static void append_quoted(char *dst, size_t cap, const char *s)
{
    size_t len = strlen(dst);
    if (len + strlen(s) + 4 >= cap) return;
    dst[len++] = '"';
    while (*s) dst[len++] = *s++;
    dst[len++] = '"';
    dst[len++] = ' ';
    dst[len] = '\0';
}

int plat_spawn_wait(const char *const *argv, int argc)
{
    static char cmd[32768];
    cmd[0] = '\0';
    for (int i = 0; i < argc; ++i) append_quoted(cmd, sizeof cmd, argv[i]);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    if (!CreateProcessA(argv[0], cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

bool plat_term_init(int *width)
{
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    SetConsoleTitleA("CentralTech - samcli");

    bool vt = false;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        vt = SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */) != 0;
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(h, &info))
            *width = info.srWindow.Right - info.srWindow.Left + 1;
    }
    return vt;
}

void plat_localtime(uint32_t t, plat_time *out)
{
    uint64_t ft = ((uint64_t)t + 11644473600ull) * 10000000ull;
    FILETIME f;
    f.dwLowDateTime = (DWORD)ft;
    f.dwHighDateTime = (DWORD)(ft >> 32);
    SYSTEMTIME u, l;
    FileTimeToSystemTime(&f, &u);
    if (!SystemTimeToTzSpecificLocalTime(NULL, &u, &l)) l = u;
    out->year = l.wYear; out->month = l.wMonth; out->day = l.wDay;
    out->hour = l.wHour; out->minute = l.wMinute;
}

int plat_steam_roots(char roots[][PLAT_PATH], int max)
{
    DWORD cb = PLAT_PATH;
    if (max < 1 || RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath",
                                RRF_RT_REG_SZ, NULL, roots[0], &cb) != ERROR_SUCCESS)
        return 0;
    for (char *c = roots[0]; *c; ++c) if (*c == '/') *c = '\\';
    return 1;
}

#else
/* ================================================================== */
/* Linux / POSIX                                                      */
/* ================================================================== */

void *plat_lib_open(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
void *plat_lib_sym(void *lib, const char *name) { return dlsym(lib, name); }

bool plat_exe_path(char *out, size_t cap)
{
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    if (n <= 0) return false;
    out[n] = '\0';
    return true;
}

bool plat_exe_dir(char *out, size_t cap)
{
    if (!plat_exe_path(out, cap)) return false;
    char *slash = strrchr(out, '/');
    if (!slash) return false;
    slash[1] = '\0';
    return true;
}

void plat_setenv(const char *key, const char *value) { setenv(key, value, 1); }
bool plat_env_set(const char *key) { const char *v = getenv(key); return v && *v; }

void plat_sleep_ms(unsigned ms)
{
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

uint32_t plat_ticks_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000);
}

int plat_spawn_wait(const char *const *argv, int argc)
{
    (void)argc;
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool plat_term_init(int *width)
{
    if (!isatty(STDOUT_FILENO)) return false;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) *width = ws.ws_col;
    const char *term = getenv("TERM");
    if (term && strcmp(term, "dumb") == 0) return false;
    printf("\x1b]0;CentralTech - samcli\x07");     /* titulo da janela */
    return true;
}

void plat_localtime(uint32_t t, plat_time *out)
{
    time_t tt = (time_t)t;
    struct tm tm;
    localtime_r(&tt, &tm);
    out->year = tm.tm_year + 1900; out->month = tm.tm_mon + 1; out->day = tm.tm_mday;
    out->hour = tm.tm_hour; out->minute = tm.tm_min;
}

int plat_steam_roots(char roots[][PLAT_PATH], int max)
{
    static const char *const rel[] = {
        ".steam/steam",
        ".local/share/Steam",
        ".var/app/com.valvesoftware.Steam/.local/share/Steam",   /* Flatpak */
        "snap/steam/common/.local/share/Steam",                  /* Snap    */
    };
    const char *home = getenv("HOME");
    int n = 0;
    if (!home) return 0;
    for (size_t i = 0; i < sizeof rel / sizeof rel[0] && n < max; ++i) {
        char p[PLAT_PATH];
        snprintf(p, sizeof p, "%s/%s", home, rel[i]);
        char *real = realpath(p, NULL);              /* resolve o symlink ~/.steam/steam */
        if (!real) continue;
        bool dup = strlen(real) >= PLAT_PATH;
        for (int k = 0; k < n && !dup; ++k) if (strcmp(roots[k], real) == 0) dup = true;
        if (!dup) snprintf(roots[n++], PLAT_PATH, "%s", real);
        free(real);
    }
    return n;
}

#endif
