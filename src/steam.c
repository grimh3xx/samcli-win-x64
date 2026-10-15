/*
 * steam.c - carrega a steam_api64.dll (Windows) ou libsteam_api.so (Linux)
 * em tempo de execucao e expoe o
 * minimo de ISteamUserStats que o samcli precisa.
 *
 * Funciona com DLLs de SDK antigo (SteamAPI_Init + RequestCurrentStats)
 * e novo (>= 1.61: SteamAPI_InitFlat, stats sincronizadas sozinhas).
 */
#include "steam.h"

typedef void *iface;
typedef int32_t HSteamUser;

typedef bool        (*fn_init)(void);
typedef int         (*fn_init_flat)(char *err1024);
typedef void        (*fn_void)(void);
typedef bool        (*fn_bool)(void);
typedef HSteamUser  (*fn_huser)(void);
typedef iface       (*fn_accessor)(void);
typedef iface       (*fn_find_iface)(HSteamUser, const char *);

typedef bool        (*fn_us_bool)(iface);
typedef uint32_t    (*fn_us_count)(iface);
typedef const char *(*fn_us_name)(iface, uint32_t);
typedef bool        (*fn_us_get)(iface, const char *, bool *);
typedef bool        (*fn_us_get_time)(iface, const char *, bool *, uint32_t *);
typedef bool        (*fn_us_set)(iface, const char *);
typedef const char *(*fn_us_attr)(iface, const char *, const char *);
typedef bool        (*fn_apps_owned)(iface, uint32_t);

static struct {
    void *dll;
    fn_init_flat  InitFlat;
    fn_init       Init;
    fn_void       Shutdown, RunCallbacks;
    fn_bool       IsSteamRunning;
    fn_huser      GetHSteamUser;
    fn_find_iface FindOrCreateUserInterface;

    fn_us_bool     RequestCurrentStats;   /* opcional */
    fn_us_count    GetNumAchievements;
    fn_us_name     GetAchievementName;
    fn_us_get      GetAchievement;
    fn_us_get_time GetAchievementAndUnlockTime; /* opcional */
    fn_us_set      SetAchievement, ClearAchievement;
    fn_us_bool     StoreStats;
    fn_us_attr     GetDisplayAttr;
    fn_apps_owned  BIsSubscribedApp;      /* opcional */

    iface stats, apps;
    bool  initialized;
    char  detail[1024];
} S;

const char *steam_error_detail(void) { return S.detail; }

static void *sym(const char *name) { return plat_lib_sym(S.dll, name); }

#define NEED(field, name) do { if (!(S.field = sym(name))) { \
        snprintf(S.detail, sizeof S.detail, "%s", name); return STEAM_ERR_EXPORT; } } while (0)

steam_status steam_load(void)
{
    char path[PLAT_PATH + 32];
    if (plat_exe_dir(path, PLAT_PATH)) {
        strcat(path, STEAM_API_LIB);
        S.dll = plat_lib_open(path);
    }
    if (!S.dll) S.dll = plat_lib_open(STEAM_API_LIB);     /* PATH / LD_LIBRARY_PATH */
    if (!S.dll) return STEAM_ERR_DLL;

    S.InitFlat = sym("SteamAPI_InitFlat");
    if (!S.InitFlat && !(S.Init = sym("SteamAPI_Init")) && !(S.Init = sym("SteamAPI_InitSafe"))) {
        snprintf(S.detail, sizeof S.detail, "SteamAPI_Init*");
        return STEAM_ERR_EXPORT;
    }
    NEED(Shutdown,           "SteamAPI_Shutdown");
    NEED(RunCallbacks,       "SteamAPI_RunCallbacks");
    NEED(GetNumAchievements, "SteamAPI_ISteamUserStats_GetNumAchievements");
    NEED(GetAchievementName, "SteamAPI_ISteamUserStats_GetAchievementName");
    NEED(GetAchievement,     "SteamAPI_ISteamUserStats_GetAchievement");
    NEED(SetAchievement,     "SteamAPI_ISteamUserStats_SetAchievement");
    NEED(ClearAchievement,   "SteamAPI_ISteamUserStats_ClearAchievement");
    NEED(StoreStats,         "SteamAPI_ISteamUserStats_StoreStats");
    NEED(GetDisplayAttr,     "SteamAPI_ISteamUserStats_GetAchievementDisplayAttribute");

    S.IsSteamRunning              = sym("SteamAPI_IsSteamRunning");
    S.GetHSteamUser               = sym("SteamAPI_GetHSteamUser");
    S.FindOrCreateUserInterface   = sym("SteamInternal_FindOrCreateUserInterface");
    S.RequestCurrentStats         = sym("SteamAPI_ISteamUserStats_RequestCurrentStats");
    S.GetAchievementAndUnlockTime = sym("SteamAPI_ISteamUserStats_GetAchievementAndUnlockTime");
    S.BIsSubscribedApp            = sym("SteamAPI_ISteamApps_BIsSubscribedApp");
    return STEAM_OK;
}

/* Tenta os acessores versionados e cai pro FindOrCreateUserInterface. */
static iface get_iface(const char *const *accessors, const char *const *versions)
{
    for (int i = 0; accessors[i]; ++i) {
        fn_accessor f = (fn_accessor)sym(accessors[i]);
        iface p = f ? f() : NULL;
        if (p) return p;
    }
    if (S.FindOrCreateUserInterface && S.GetHSteamUser) {
        HSteamUser u = S.GetHSteamUser();
        for (int i = 0; versions[i]; ++i) {
            iface p = S.FindOrCreateUserInterface(u, versions[i]);
            if (p) return p;
        }
    }
    return NULL;
}

steam_status steam_init(uint32_t appid)
{
    char id[16];
    snprintf(id, sizeof id, "%u", (unsigned)appid);
    plat_setenv("SteamAppId", id);
    plat_setenv("SteamGameId", id);

    if (S.IsSteamRunning && !S.IsSteamRunning()) return STEAM_ERR_NOT_RUNNING;

    S.detail[0] = '\0';
    if (S.InitFlat) {
        int r = S.InitFlat(S.detail);
        if (r != 0) return STEAM_ERR_INIT;
    } else if (!S.Init()) {
        return STEAM_ERR_INIT;
    }
    S.initialized = true;

    static const char *const us_acc[] = { "SteamAPI_SteamUserStats_v013",
        "SteamAPI_SteamUserStats_v012", "SteamAPI_SteamUserStats_v011", NULL };
    static const char *const us_ver[] = { "STEAMUSERSTATS_INTERFACE_VERSION013",
        "STEAMUSERSTATS_INTERFACE_VERSION012", "STEAMUSERSTATS_INTERFACE_VERSION011", NULL };
    static const char *const ap_acc[] = { "SteamAPI_SteamApps_v009", "SteamAPI_SteamApps_v008", NULL };
    static const char *const ap_ver[] = { "STEAMAPPS_INTERFACE_VERSION009",
        "STEAMAPPS_INTERFACE_VERSION008", NULL };

    S.stats = get_iface(us_acc, us_ver);
    S.apps  = get_iface(ap_acc, ap_ver);
    if (!S.stats) return STEAM_ERR_IFACE;
    if (S.apps && S.BIsSubscribedApp && !S.BIsSubscribedApp(S.apps, appid)) return STEAM_ERR_NOT_OWNED;
    return STEAM_OK;
}

steam_status steam_wait_stats(int timeout_ms, void (*tick)(void))
{
    if (S.RequestCurrentStats) S.RequestCurrentStats(S.stats);   /* SDK antigo */

    uint32_t start = plat_ticks_ms();
    do {
        S.RunCallbacks();
        if (S.GetNumAchievements(S.stats) > 0) {
            bool a;
            const char *first = S.GetAchievementName(S.stats, 0);
            if (first && S.GetAchievement(S.stats, first, &a)) return STEAM_OK;
        }
        if (tick) tick();
        plat_sleep_ms(80);
    } while ((int)(plat_ticks_ms() - start) < timeout_ms);
    return STEAM_ERR_NO_STATS;
}

uint32_t steam_count(void) { return S.GetNumAchievements(S.stats); }

bool steam_get(uint32_t index, achievement *out)
{
    const char *api = S.GetAchievementName(S.stats, index);
    if (!api) return false;

    memset(out, 0, sizeof *out);
    out->api = api;
    if (S.GetAchievementAndUnlockTime)
        S.GetAchievementAndUnlockTime(S.stats, api, &out->achieved, &out->unlock_time);
    else
        S.GetAchievement(S.stats, api, &out->achieved);

    out->name = S.GetDisplayAttr(S.stats, api, "name");
    out->desc = S.GetDisplayAttr(S.stats, api, "desc");
    const char *h = S.GetDisplayAttr(S.stats, api, "hidden");
    out->hidden = h && h[0] == '1';
    if (!out->name || !*out->name) out->name = api;
    if (!out->desc) out->desc = "";
    return true;
}

int steam_set(const char *api, bool unlock)
{
    bool a = false;
    if (!S.GetAchievement(S.stats, api, &a)) return -1;
    if (a == unlock) return 2;
    return (unlock ? S.SetAchievement(S.stats, api) : S.ClearAchievement(S.stats, api)) ? 1 : 0;
}

bool steam_store(void) { return S.StoreStats(S.stats); }

void steam_pump(unsigned ms)
{
    uint32_t start = plat_ticks_ms();
    do { S.RunCallbacks(); plat_sleep_ms(40); } while (plat_ticks_ms() - start < ms);
}

void steam_shutdown(void)
{
    if (S.initialized) S.Shutdown();
    S.initialized = false;
}

/* ------------------------------------------------------------------ */
/* Nome do jogo via appmanifest_<id>.acf nas bibliotecas da Steam      */
/* ------------------------------------------------------------------ */

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (size > 0 && size < (16L << 20)) ? (char *)malloc((size_t)size + 1) : NULL;
    if (buf) {
        size_t got = fread(buf, 1, (size_t)size, f);
        buf[got] = '\0';
    }
    fclose(f);
    return buf;
}

/* Le o valor entre aspas depois de `key` (formato VDF), tirando os \\ escapados. */
static const char *vdf_value(const char *p, const char *key, char *out, size_t cap)
{
    const char *k = strstr(p, key);
    if (!k) return NULL;
    const char *q = strchr(k + strlen(key), '"');
    if (!q) return NULL;
    size_t n = 0;
    for (++q; *q && *q != '"' && n + 1 < cap; ++q) {
        if (q[0] == '\\' && q[1]) ++q;
        out[n++] = *q;
    }
    out[n] = '\0';
    return q;
}

static bool manifest_name(const char *lib, uint32_t appid, char *out, size_t cap)
{
    char path[PLAT_PATH + 64];
    snprintf(path, sizeof path, "%.*s%csteamapps%cappmanifest_%u.acf", PLAT_PATH - 1, lib, PLAT_SEP, PLAT_SEP, (unsigned)appid);
    char *txt = read_file(path);
    if (!txt) return false;
    bool ok = vdf_value(txt, "\"name\"", out, cap) != NULL && out[0];
    free(txt);
    return ok;
}

bool steam_game_name(uint32_t appid, char *out, size_t cap)
{
    static char roots[4][PLAT_PATH];
    int nroots = plat_steam_roots(roots, 4);

    for (int r = 0; r < nroots; ++r) {
        if (manifest_name(roots[r], appid, out, cap)) return true;

        char vdf[PLAT_PATH + 64];
        snprintf(vdf, sizeof vdf, "%.*s%csteamapps%clibraryfolders.vdf", PLAT_PATH - 1, roots[r], PLAT_SEP, PLAT_SEP);
        char *txt = read_file(vdf);
        if (!txt) continue;

        bool found = false;
        char lib[PLAT_PATH];
        for (const char *p = txt; !found && (p = vdf_value(p, "\"path\"", lib, sizeof lib)) != NULL; )
            found = manifest_name(lib, appid, out, cap);
        free(txt);
        if (found) return true;
    }
    return false;
}
