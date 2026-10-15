/*
 * samcli - Steam Achievement Manager de linha de comando
 * CentralTech Solutions - https://centraltechpc.com/
 *
 *   samcli                                  modo interativo
 *   samcli <appid> list                     lista as conquistas
 *   samcli <appid> unlock-all [--delay 30s] platina
 *   samcli <appid> unlock <API_NAME>...     so as escolhidas
 *   samcli <appid> lock-all | lock <API>... desfaz
 *   samcli <appid> <appid> ... <comando>    varios jogos em sequencia
 *
 * Um processo so pode "ser" um AppID por vez, entao o modo interativo e o
 * modo de varios jogos relancam o proprio exe (--child) uma vez por jogo.
 */
#include "steam.h"
#include "ui.h"
#include "platform.h"

#define MAX_IDS   256
#define MAX_NAMES 1024

static struct {
    bool        plain, child;
    uint32_t    delay_ms;
    uint32_t    ids[MAX_IDS];
    int         nids;
    const char *cmd;
    const char *names[MAX_NAMES];
    int         nnames;
} opt;

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

static bool all_digits(const char *s)
{
    if (!*s) return false;
    for (; *s; ++s) if (*s < '0' || *s > '9') return false;
    return true;
}

/* Aceita "2124490" ou "https://store.steampowered.com/app/2124490/SILENT_HILL_2/". */
static bool parse_appid(const char *s, uint32_t *out)
{
    const char *p = all_digits(s) ? s : strstr(s, "/app/");
    if (!p) return false;
    if (p != s) p += 5;
    unsigned long v = 0;
    int n = 0;
    for (; *p >= '0' && *p <= '9' && n < 10; ++p, ++n) v = v * 10 + (unsigned long)(*p - '0');
    if (n == 0 || v == 0 || v > 0xFFFFFFFFul) return false;
    *out = (uint32_t)v;
    return true;
}

/* "30s", "2m" ou milissegundos puros. */
static uint32_t parse_delay(const char *s)
{
    unsigned long v = strtoul(s, NULL, 10);
    size_t n = strlen(s);
    if (n && (s[n - 1] == 's' || s[n - 1] == 'S')) return (uint32_t)(v * 1000);
    if (n && (s[n - 1] == 'm' || s[n - 1] == 'M')) return (uint32_t)(v * 60000);
    return (uint32_t)v;
}

static void fmt_time(uint32_t t, char *out, size_t cap)
{
    if (!t) { snprintf(out, cap, "%s", G_DASH); return; }
    plat_time l;
    plat_localtime(t, &l);
    snprintf(out, cap, "%02d/%02d/%04d %02d:%02d", l.day, l.month, l.year, l.hour, l.minute);
}

static void count_achieved(uint32_t *got, uint32_t *total)
{
    achievement a;
    *got = 0;
    *total = steam_count();
    for (uint32_t i = 0; i < *total; ++i)
        if (steam_get(i, &a) && a.achieved) ++*got;
}

static void platinum_line(void)
{
    printf("\n  ");
    ui_badge("PLATINA", 255, 196, 40);
    printf("  %s%sTodas as conquistas desbloqueadas!%s\n", ui_c(C_BOLD), ui_c(C_WHITE), ui_c(C_RESET));
}

static void pump_with_spinner(const char *m, unsigned ms)
{
    ui_spinner_start(m);
    for (unsigned t = 0; t < ms; t += 100) { steam_pump(100); ui_spinner_tick(); }
    ui_spinner_stop();
}

/* ------------------------------------------------------------------ */
/* Abrir o jogo                                                        */
/* ------------------------------------------------------------------ */

static int open_game(uint32_t appid)
{
    char name[256];
    if (!steam_game_name(appid, name, sizeof name)) name[0] = '\0';
    ui_game(name, appid);

    steam_status st = steam_load();
    if (st == STEAM_ERR_DLL) {
        ui_err("%s n\xC3\xA3o encontrada", STEAM_API_LIB);
        ui_info("Coloque a %s na mesma pasta do samcli (tem em qualquer jogo instalado).", STEAM_API_LIB);
        return 2;
    }
    if (st == STEAM_ERR_EXPORT) {
        ui_err("A %s n\xC3\xA3o tem %s (vers\xC3\xA3o incompat\xC3\xADvel)", STEAM_API_LIB, steam_error_detail());
        return 2;
    }

    ui_spinner_start("Conectando \xC3\xA0 Steam...");
    st = steam_init(appid);
    if (st == STEAM_OK) {
        ui_spinner_start("Carregando conquistas...");
        st = steam_wait_stats(10000, ui_spinner_tick);
    }
    ui_spinner_stop();

    switch (st) {
    case STEAM_OK:
        return 0;
    case STEAM_ERR_NOT_RUNNING:
        ui_err("A Steam n\xC3\xA3o est\xC3\xA1 aberta.");
        ui_info("Abra a Steam, fa\xC3\xA7" "a login e tente de novo.");
        break;
    case STEAM_ERR_INIT:
        ui_err("N\xC3\xA3o foi poss\xC3\xADvel conectar \xC3\xA0 Steam%s%s",
               *steam_error_detail() ? ": " : ".", steam_error_detail());
        ui_info("Confira se o jogo est\xC3\xA1 na sua conta e se a Steam est\xC3\xA1 logada.");
        break;
    case STEAM_ERR_IFACE:
        ui_err("A Steam n\xC3\xA3o entregou a interface de conquistas (ISteamUserStats).");
        break;
    case STEAM_ERR_NOT_OWNED:
        ui_err("Esse jogo n\xC3\xA3o est\xC3\xA1 na sua biblioteca.");
        break;
    default:
        ui_warn("Esse jogo n\xC3\xA3o tem conquistas (ou a Steam n\xC3\xA3o respondeu a tempo).");
        break;
    }
    steam_shutdown();
    return 2;
}

/* ------------------------------------------------------------------ */
/* Comandos                                                            */
/* ------------------------------------------------------------------ */

static void table_row(const int *w, int n, const char **cells, const int *colors)
{
    printf("  %s%s%s", ui_c(C_DIM), G_V, ui_c(C_RESET));
    for (int i = 0; i < n; ++i) {
        printf(" %s", ui_c(colors[i]));
        ui_cell(cells[i], w[i]);
        printf("%s %s%s%s", ui_c(C_RESET), ui_c(C_DIM), G_V, ui_c(C_RESET));
    }
    printf("\n");
}

static int cmd_list(void)
{
    uint32_t n = steam_count(), got = 0;

    int avail = (ui_width() < 126 ? ui_width() : 126) - 4;
    bool show_api = avail >= 96;
    int ncols = show_api ? 4 : 3;
    int w[4];
    w[0] = 1;
    w[2] = show_api ? 30 : 16;
    w[3] = 16;
    if (!show_api) w[2] = w[3];
    w[1] = avail - 1 - 3 * ncols - w[0] - w[2] - (show_api ? w[3] : 0);
    if (w[1] < 18) w[1] = 18;

    const char *head[4] = { " ", "Conquista", show_api ? "API" : "Desbloqueada", "Desbloqueada" };
    int hc[4] = { C_GRAY, C_GRAY, C_GRAY, C_GRAY };

    ui_hrule(G_TL, G_TT, G_TR, w, ncols);
    printf("%s", ui_c(C_BOLD));
    table_row(w, ncols, head, hc);
    ui_hrule(G_LT, G_X, G_RT, w, ncols);

    achievement a;
    char title[600], when[32];
    for (uint32_t i = 0; i < n; ++i) {
        if (!steam_get(i, &a)) continue;
        if (a.achieved) ++got;
        snprintf(title, sizeof title, "%s%s", a.name, (a.hidden && !a.achieved) ? "  (oculta)" : "");
        if (a.achieved) fmt_time(a.unlock_time, when, sizeof when);
        else snprintf(when, sizeof when, "%s", G_DASH);

        const char *cells[4] = { a.achieved ? G_ON : G_OFF, title, show_api ? a.api : when, when };
        int colors[4] = { a.achieved ? C_GREEN : C_DIM, a.achieved ? C_WHITE : C_GRAY,
                          show_api ? C_DIM : C_GRAY, C_GRAY };
        table_row(w, ncols, cells, colors);
    }
    ui_hrule(G_BL, G_BT, G_BR, w, ncols);

    printf("\n");
    ui_progress((int)got, (int)n, "desbloqueadas");
    ui_progress_end();
    if (n && got == n) platinum_line();
    return 0;
}

/* unlock: true desbloqueia, false trava. Sem nomes = todas. */
static int cmd_set(bool unlock, const char **names, int nnames, uint32_t delay_ms)
{
    uint32_t n = steam_count();

    /* monta a lista de alvos */
    uint32_t *todo = NULL;
    int total = 0;
    if (!names) {
        todo = (uint32_t *)malloc(sizeof(uint32_t) * (n ? n : 1));
        achievement a;
        for (uint32_t i = 0; todo && i < n; ++i)
            if (steam_get(i, &a) && a.achieved != unlock) todo[total++] = i;
    } else {
        total = nnames;
    }

    if (total == 0) {
        free(todo);
        ui_ok(unlock ? "Nada pra fazer: todas j\xC3\xA1 est\xC3\xA3o desbloqueadas."
                     : "Nada pra fazer: nenhuma est\xC3\xA1 desbloqueada.");
        if (unlock && n) platinum_line();
        return 0;
    }

    ui_info("%d conquista(s) para %s", total, unlock ? "desbloquear" : "travar");
    if (delay_ms) ui_info("Intervalo de %lus entre cada uma", (unsigned long)(delay_ms / 1000));
    printf("\n");

    int changed = 0, failed = 0;
    achievement a;
    char suffix[64];
    for (int i = 0; i < total; ++i) {
        const char *api, *label;
        if (names) { api = label = names[i]; }
        else { steam_get(todo[i], &a); api = a.api; label = a.name; }

        int r = steam_set(api, unlock);
        if (ui_has_color()) printf("\r\x1b[2K");

        if (r == 1) {
            ++changed;
            printf("  %s%s%s %s", ui_c(unlock ? C_GREEN : C_YELLOW), unlock ? G_ON : G_OFF, ui_c(C_RESET), ui_c(C_WHITE));
        } else if (r == 2) {
            printf("  %s%s %s", ui_c(C_DIM), G_DASH, ui_c(C_GRAY));
        } else {
            ++failed;
            printf("  %s%s%s %s", ui_c(C_RED), G_TIMES, ui_c(C_RESET), ui_c(C_RED));
        }
        printf("%s%s  %s%s%s%s\n", label, ui_c(C_RESET), ui_c(C_DIM),
               (names || !strcmp(label, api)) ? "" : api,
               r == -1 ? "(n\xC3\xA3o existe nesse jogo)" : r == 0 ? "(falhou)" : r == 2 ? "(j\xC3\xA1 estava assim)" : "",
               ui_c(C_RESET));

        if (ui_has_color()) ui_progress(i + 1, total, NULL);

        if (delay_ms && r == 1 && i + 1 < total) {
            steam_store();                       /* grava agora: horarios espacados */
            for (uint32_t left = delay_ms; left > 0; ) {
                if (ui_has_color()) {
                    snprintf(suffix, sizeof suffix, "pr\xC3\xB3xima em %lus", (unsigned long)((left + 999) / 1000));
                    ui_progress(i + 1, total, suffix);
                }
                uint32_t step = left < 1000 ? left : 1000;
                steam_pump(step);
                left -= step;
            }
        }
    }
    if (ui_has_color()) ui_progress_end();
    free(todo);

    if (changed) {
        bool ok = steam_store();
        pump_with_spinner("Sincronizando com a Steam...", 1500);
        if (!ok) {
            ui_err("StoreStats falhou: nada foi salvo.");
            return 1;
        }
    }

    printf("\n");
    if (failed) ui_warn("%d %s, %d falha(s)", changed, unlock ? "desbloqueada(s)" : "travada(s)", failed);
    else        ui_ok("%d %s com sucesso", changed, unlock ? "desbloqueada(s)" : "travada(s)");

    uint32_t got, all;
    count_achieved(&got, &all);
    printf("\n");
    ui_progress((int)got, (int)all, "no total");
    ui_progress_end();
    if (unlock && all && got == all) platinum_line();
    return failed ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Execucao                                                            */
/* ------------------------------------------------------------------ */

static int run_single(uint32_t appid)
{
    int rc = open_game(appid);
    if (rc) return rc;

    const char *c = opt.cmd ? opt.cmd : "list";
    const char **nm = opt.nnames ? opt.names : NULL;

    if      (!strcmp(c, "list"))                rc = cmd_list();
    else if (!strcmp(c, "unlock-all"))          rc = cmd_set(true,  NULL, 0, opt.delay_ms);
    else if (!strcmp(c, "lock-all"))            rc = cmd_set(false, NULL, 0, opt.delay_ms);
    else if (!strcmp(c, "unlock") && nm)        rc = cmd_set(true,  nm, opt.nnames, opt.delay_ms);
    else if (!strcmp(c, "lock") && nm)          rc = cmd_set(false, nm, opt.nnames, opt.delay_ms);
    else { ui_err("Comando inv\xC3\xA1lido: %s", c); ui_info("Veja: samcli --help"); rc = 1; }

    steam_shutdown();
    printf("\n");
    return rc;
}

/* Roda "samcli --child <appid> <args...>" e espera terminar. */
static int run_child(uint32_t appid, const char **args, int nargs)
{
    static char exe[PLAT_PATH];
    static const char *argv[MAX_NAMES + 8];
    char id[16];
    int n = 0;

    if (!plat_exe_path(exe, sizeof exe)) { ui_err("N\xC3\xA3o achei o caminho do pr\xC3\xB3prio execut\xC3\xA1vel."); return 1; }
    snprintf(id, sizeof id, "%u", (unsigned)appid);
    argv[n++] = exe;
    argv[n++] = "--child";
    if (opt.plain) argv[n++] = "--plain";
    argv[n++] = id;
    for (int i = 0; i < nargs && n < MAX_NAMES + 7; ++i) argv[n++] = args[i];
    argv[n] = NULL;

    ui_flush();
    int rc = plat_spawn_wait(argv, n);
    if (rc < 0) ui_err("N\xC3\xA3o consegui iniciar o processo filho.");
    return rc < 0 ? 1 : rc;
}

/* ------------------------------------------------------------------ */
/* Ajuda e modo interativo                                             */
/* ------------------------------------------------------------------ */

static void help_row(const char *cmd, const char *desc)
{
    printf("    %s", ui_c(C_CYAN));
    ui_cell(cmd, 40);
    printf("%s%s%s\n", ui_c(C_GRAY), desc, ui_c(C_RESET));
}

static void usage(void)
{
    printf("  %s%sUSO%s\n", ui_c(C_BOLD), ui_c(C_WHITE), ui_c(C_RESET));
    help_row("samcli",                             "modo interativo");
    help_row("samcli <appid> list",                "lista as conquistas");
    help_row("samcli <appid> unlock-all",          "desbloqueia todas (platina)");
    help_row("samcli <appid> unlock <API> [...]",  "desbloqueia s\xC3\xB3 as escolhidas");
    help_row("samcli <appid> lock-all",            "trava todas");
    help_row("samcli <appid> lock <API> [...]",    "trava s\xC3\xB3 as escolhidas");
    help_row("samcli <appid> <appid> ... <cmd>",   "v\xC3\xA1rios jogos em sequ\xC3\xAAncia");
    printf("\n  %s%sOP\xC3\x87\xC3\x95" "ES%s\n", ui_c(C_BOLD), ui_c(C_WHITE), ui_c(C_RESET));
    help_row("--delay <tempo>",  "intervalo entre conquistas: 30s, 2m ou ms");
    help_row("--plain",          "sem cores (tamb\xC3\xA9m: vari\xC3\xA1vel NO_COLOR)");
    help_row("--version",        "mostra a vers\xC3\xA3o");
    printf("\n  %sO <appid> pode ser o n\xC3\xBAmero ou o link da loja:%s store.steampowered.com/app/%s<AppID>%s/\n\n",
           ui_c(C_GRAY), ui_c(C_RESET), ui_c(C_CYAN), ui_c(C_RESET));
}

static void menu_line(const char *key, const char *label, int color, int inner)
{
    printf("  %s%s%s  ", ui_c(C_DIM), G_V, ui_c(C_RESET));
    printf("%s%s%s%s  %s", ui_c(C_BOLD), ui_c(C_CYAN), key, ui_c(C_RESET), ui_c(color));
    ui_cell(label, inner - 5);
    printf("%s%s%s%s\n", ui_c(C_RESET), ui_c(C_DIM), G_V, ui_c(C_RESET));
}

static void menu_text(const char *k, const char *v, int vcolor, int inner)
{
    printf("  %s%s%s  %s", ui_c(C_DIM), G_V, ui_c(C_RESET), ui_c(C_GRAY));
    ui_cell(k, 7);
    printf("%s", ui_c(vcolor));
    ui_cell(v, inner - 9);
    printf("%s%s%s%s\n", ui_c(C_RESET), ui_c(C_DIM), G_V, ui_c(C_RESET));
}

static void draw_menu(const char *name, uint32_t appid)
{
    const int inner = 54;
    int w[1] = { inner - 2 };
    char id[32];
    snprintf(id, sizeof id, "%u", (unsigned)appid);

    printf("\n");
    ui_hrule(G_TL, G_H, G_TR, w, 1);
    menu_text("Jogo", *name ? name : "(nome indispon\xC3\xADvel: jogo n\xC3\xA3o instalado)",
              *name ? C_WHITE : C_GRAY, inner);
    menu_text("AppID", id, C_TEXT, inner);
    ui_hrule(G_LT, G_H, G_RT, w, 1);
    menu_line("1", "Ver conquistas",                          C_TEXT,  inner);
    menu_line("2", "Desbloquear todas (platina)",             C_WHITE, inner);
    menu_line("3", "Desbloquear todas com intervalo",         C_TEXT,  inner);
    menu_line("4", "Desbloquear espec\xC3\xAD" "ficas",       C_TEXT,  inner);
    menu_line("5", "Travar todas",                            C_TEXT,  inner);
    menu_line("6", "Trocar de jogo",                          C_TEXT,  inner);
    menu_line("0", "Sair",                                    C_GRAY,  inner);
    ui_hrule(G_BL, G_H, G_BR, w, 1);
}

static int interactive(void)
{
    static char line[2048], name[256];
    static const char *args[MAX_NAMES + 3];
    uint32_t appid = 0;

    ui_banner();

    for (;;) {
        if (!appid) {
            ui_prompt("AppID ou link do jogo na loja (Enter pra sair):");
            if (ui_read_line(line, sizeof line) <= 0) return 0;
            if (!parse_appid(line, &appid)) {
                ui_err("AppID inv\xC3\xA1lido: %s", line);
                appid = 0;
                continue;
            }
            if (!steam_game_name(appid, name, sizeof name)) name[0] = '\0';
            ui_clear();
            ui_header();
        }

        draw_menu(name, appid);
        ui_prompt("Op\xC3\xA7\xC3\xA3o:");
        if (ui_read_line(line, sizeof line) < 0) return 0;

        int nargs = 0;
        char delay[32];

        switch (line[0]) {
        case '1': args[nargs++] = "list"; break;
        case '2': args[nargs++] = "unlock-all"; break;
        case '3': {
            ui_prompt("Segundos entre cada conquista [30]:");
            ui_read_line(line, sizeof line);
            unsigned long s = line[0] ? strtoul(line, NULL, 10) : 30;
            snprintf(delay, sizeof delay, "%lus", s ? s : 30);
            args[nargs++] = "unlock-all";
            args[nargs++] = "--delay";
            args[nargs++] = delay;
            break;
        }
        case '4': {
            ui_info("Use a op\xC3\xA7\xC3\xA3o 1 pra ver os nomes de API (coluna API).");
            ui_prompt("Nomes de API separados por espa\xC3\xA7o:");
            ui_read_line(line, sizeof line);
            args[nargs++] = "unlock";
            for (char *p = strtok(line, " \t"); p && nargs < MAX_NAMES; p = strtok(NULL, " \t"))
                args[nargs++] = p;
            if (nargs == 1) { ui_warn("Nenhum nome informado."); continue; }
            break;
        }
        case '5':
            ui_prompt("Travar TODAS as conquistas desse jogo? (s/N):");
            ui_read_line(line, sizeof line);
            if (line[0] != 's' && line[0] != 'S') { ui_info("Cancelado."); continue; }
            args[nargs++] = "lock-all";
            break;
        case '6':
            appid = 0;
            ui_clear();
            ui_banner();
            continue;
        case '0': case 'q': case 'Q':
            return 0;
        default:
            ui_warn("Op\xC3\xA7\xC3\xA3o inv\xC3\xA1lida.");
            continue;
        }

        run_child(appid, args, nargs);
        ui_wait_enter("Enter pra voltar ao menu...");
        ui_clear();
        ui_header();
    }
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    bool help = false, version = false;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        uint32_t id;
        if      (!strcmp(a, "--plain"))  opt.plain = true;
        else if (!strcmp(a, "--child"))  opt.child = true;
        else if (!strcmp(a, "--delay") && i + 1 < argc) opt.delay_ms = parse_delay(argv[++i]);
        else if (!strcmp(a, "-h") || !strcmp(a, "--help") || !strcmp(a, "help")) help = true;
        else if (!strcmp(a, "-v") || !strcmp(a, "--version")) version = true;
        else if (!opt.cmd && parse_appid(a, &id)) { if (opt.nids < MAX_IDS) opt.ids[opt.nids++] = id; }
        else if (!opt.cmd) opt.cmd = a;
        else if (opt.nnames < MAX_NAMES) opt.names[opt.nnames++] = a;
    }

    ui_init(opt.plain);

    if (version) {
        printf("samcli %s - CentralTech Solutions - %s\n", SAMCLI_VERSION, BRAND_SITE);
        return 0;
    }
    if (help) { ui_banner(); usage(); return 0; }
    if (opt.nids == 0 && !opt.cmd) return interactive();
    if (opt.nids == 0) {
        ui_header();
        ui_err("Faltou o AppID do jogo.");
        printf("\n");
        usage();
        return 1;
    }

    if (opt.child) return run_single(opt.ids[0]);

    ui_header();
    if (opt.nids == 1) return run_single(opt.ids[0]);

    /* varios jogos: um processo por AppID */
    static const char *args[MAX_NAMES + 4];
    int nargs = 0;
    char delay[32];
    args[nargs++] = opt.cmd ? opt.cmd : "list";
    if (opt.delay_ms) {
        snprintf(delay, sizeof delay, "%lu", (unsigned long)opt.delay_ms);
        args[nargs++] = "--delay";
        args[nargs++] = delay;
    }
    for (int i = 0; i < opt.nnames; ++i) args[nargs++] = opt.names[i];

    int fails = 0;
    for (int i = 0; i < opt.nids; ++i)
        if (run_child(opt.ids[i], args, nargs) != 0) ++fails;

    printf("\n");
    if (fails) ui_warn("%d jogo(s), %d com erro", opt.nids, fails);
    else       ui_ok("%d jogo(s) processados", opt.nids);
    printf("\n");
    return fails ? 1 : 0;
}
