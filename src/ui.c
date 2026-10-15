/*
 * ui.c - interface de terminal do samcli com a identidade CentralTech.
 *
 * Usa sequencias VT (cor RGB 24-bit, hyperlinks OSC 8) quando o console
 * suporta; senao cai pra um modo texto simples (tambem com --plain ou
 * a variavel NO_COLOR).
 */
#include "ui.h"
#include "platform.h"
#include "logo_data.h"

static bool g_color;
static int  g_width = 80;
static char g_esc[C__COUNT][32];

static const unsigned char RGB[C__COUNT][3] = {
    [C_WHITE]  = {245, 248, 255},
    [C_TEXT]   = {200, 208, 225},
    [C_GRAY]   = {130, 140, 160},
    [C_DIM]    = { 75,  85, 105},
    [C_BLUE]   = { 30,  90, 255},
    [C_BLUE2]  = { 10, 140, 255},
    [C_CYAN]   = {  0, 200, 255},
    [C_GREEN]  = { 40, 215, 130},
    [C_RED]    = {255,  85,  95},
    [C_YELLOW] = {255, 196,  40},
};

/* degradê da marca: azul profundo -> ciano */
static const int GRAD_A[3] = { 24,  72, 255};
static const int GRAD_B[3] = {  0, 205, 255};

static const char SPACES[] =
    "                                                                "
    "                                                                ";

void ui_flush(void) { fflush(stdout); }
bool ui_has_color(void) { return g_color; }
int  ui_width(void) { return g_width; }
const char *ui_c(int c) { return g_color ? g_esc[c] : ""; }

void ui_init(bool plain)
{
    bool vt = plat_term_init(&g_width);
    if (plat_env_set("NO_COLOR")) plain = true;
    g_color = vt && !plain;

    strcpy(g_esc[C_RESET], "\x1b[0m");
    strcpy(g_esc[C_BOLD], "\x1b[1m");
    for (int i = C_WHITE; i < C__COUNT; ++i)
        snprintf(g_esc[i], sizeof g_esc[i], "\x1b[38;2;%d;%d;%dm", RGB[i][0], RGB[i][1], RGB[i][2]);
}

static void fg(int r, int g, int b)
{
    if (g_color) printf("\x1b[38;2;%d;%d;%dm", r, g, b);
}

static void grad(int i, int n)
{
    int c[3];
    for (int k = 0; k < 3; ++k)
        c[k] = GRAD_A[k] + (GRAD_B[k] - GRAD_A[k]) * i / (n > 1 ? n - 1 : 1);
    fg(c[0], c[1], c[2]);
}

void ui_pad(int n)
{
    while (n > 0) {
        int k = n < (int)sizeof SPACES - 1 ? n : (int)sizeof SPACES - 1;
        printf("%.*s", k, SPACES);
        n -= k;
    }
}

void ui_repeat(const char *glyph, int n)
{
    for (int i = 0; i < n; ++i) printf("%s", glyph);
}

static int cp_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

int ui_strwidth(const char *s)
{
    int n = 0;
    while (*s) {
        int l = cp_len((unsigned char)*s);
        for (int i = 0; i < l && *s; ++i) ++s;
        ++n;
    }
    return n;
}

void ui_cell(const char *s, int width)
{
    if (width <= 0) return;
    int n = ui_strwidth(s);
    if (n <= width) {
        printf("%s", s);
        ui_pad(width - n);
        return;
    }
    const char *p = s;
    for (int i = 0; i < width - 1 && *p; ++i) {
        int l = cp_len((unsigned char)*p);
        for (int k = 0; k < l && *p; ++k) ++p;
    }
    printf("%.*s%s", (int)(p - s), s, G_ELLIP);
}

void ui_link(const char *url, const char *text, int color)
{
    if (g_color) printf("\x1b]8;;%s\x1b\\%s%s%s\x1b]8;;\x1b\\", url, g_esc[color], text, g_esc[C_RESET]);
    else printf("%s", text);
}

void ui_badge(const char *text, int r, int g, int b)
{
    if (g_color) printf("\x1b[48;2;%d;%d;%dm\x1b[38;2;8;10;20m\x1b[1m %s \x1b[0m", r, g, b, text);
    else printf("[%s]", text);
}

void ui_clear(void)
{
    if (g_color) printf("\x1b[2J\x1b[3J\x1b[H");
}

/* ------------------------------------------------------------------ */
/* Logo                                                                */
/* ------------------------------------------------------------------ */

static const char *const QUAD[16] = {
    " ",            "\xE2\x96\x98", "\xE2\x96\x9D", "\xE2\x96\x80",
    "\xE2\x96\x96", "\xE2\x96\x8C", "\xE2\x96\x9E", "\xE2\x96\x9B",
    "\xE2\x96\x97", "\xE2\x96\x9A", "\xE2\x96\x90", "\xE2\x96\x9C",
    "\xE2\x96\x84", "\xE2\x96\x99", "\xE2\x96\x9F", "\xE2\x96\x88",
};

static void draw_quad(const quad_image *im, int indent)
{
    for (int r = 0; r < im->rows; ++r) {
        ui_pad(indent);
        int last = -1;
        int pending = 0;                 /* espacos acumulados (evita espaco no fim da linha) */
        for (int c = 0; c < im->cols; ++c) {
            int k = r * im->cols + c;
            unsigned char bits = im->bits[k];
            if (!bits) { ++pending; continue; }
            ui_pad(pending);
            pending = 0;
            int ci = im->color[k];
            if (ci != last) {
                fg(LOGO_PALETTE[ci][0], LOGO_PALETTE[ci][1], LOGO_PALETTE[ci][2]);
                last = ci;
            }
            printf("%s", QUAD[bits]);
        }
        printf("%s\n", ui_c(C_RESET));
    }
}

static void rule(int indent, int w)
{
    ui_pad(indent);
    for (int i = 0; i < w; ++i) { grad(i, w); printf("%s", G_H); }
    printf("%s\n", ui_c(C_RESET));
}

static int center(int w)
{
    int x = (g_width - w) / 2;
    return x > 0 ? x : 0;
}

void ui_banner(void)
{
    if (!g_color || g_width < 72) {
        printf("\n  CentralTech Solutions\n"
               "  samcli - Steam Achievement Manager v" SAMCLI_VERSION "\n"
               "  " BRAND_SITE_TXT "  |  " BRAND_GH_TXT "\n\n");
        return;
    }

    bool big = g_width >= 92;
    const quad_image *icon = big ? &ICON_L : &ICON_S;
    const quad_image *word = big ? &WORD_L : &WORD_S;
    int W = word->cols, base = center(W);

    printf("\n");
    draw_quad(icon, base + (W - icon->cols) / 2);
    printf("\n");
    draw_quad(word, base);

    /* S  O  L  U  T  I  O  N  S  (como na logo) */
    int gap = big ? 5 : 4, sw = 9 + 8 * gap;
    ui_pad(base + (W - sw) / 2);
    fg(10, 160, 255);
    for (const char *s = "SOLUTIONS"; *s; ++s) {
        printf("%c", *s);
        if (s[1]) ui_pad(gap);
    }
    printf("%s\n\n", ui_c(C_RESET));

    rule(base, W);

    /* Steam Achievement Manager  ·  v1.1.0 */
    const char *t1 = "Steam Achievement Manager";
    int w1 = (int)strlen(t1) + 5 + (int)strlen("v" SAMCLI_VERSION);
    ui_pad(center(w1));
    printf("%s%s%s%s  %s%s  %sv%s%s\n", ui_c(C_BOLD), ui_c(C_WHITE), t1, ui_c(C_RESET),
           ui_c(C_DIM), G_DOT, ui_c(C_GRAY), SAMCLI_VERSION, ui_c(C_RESET));

    /* ► centraltechpc.com      ► github.com/grimh3xx */
    int w2 = 2 + (int)strlen(BRAND_SITE_TXT) + 6 + 2 + (int)strlen(BRAND_GH_TXT);
    ui_pad(center(w2));
    printf("%s%s ", ui_c(C_CYAN), G_ARROW);
    ui_link(BRAND_SITE, BRAND_SITE_TXT, C_TEXT);
    ui_pad(6);
    printf("%s%s ", ui_c(C_CYAN), G_ARROW);
    ui_link(BRAND_GH, BRAND_GH_TXT, C_TEXT);
    printf("\n");

    rule(base, W);
    printf("\n");
}

void ui_header(void)
{
    if (!g_color) {
        printf("CentralTech | samcli v" SAMCLI_VERSION " | " BRAND_SITE_TXT "\n\n");
        return;
    }
    printf("\n  ");
    for (int i = 0; i < 3; ++i) { grad(i * 2, 5); printf("%s", G_SQ); }
    printf(" %s%sCentral%s%s%sTech%s", ui_c(C_BOLD), ui_c(C_WHITE), ui_c(C_RESET),
           ui_c(C_BOLD), ui_c(C_BLUE2), ui_c(C_RESET));
    printf("  %s%s%s  samcli %sv%s%s  %s%s%s  ", ui_c(C_DIM), G_V, ui_c(C_RESET),
           ui_c(C_GRAY), SAMCLI_VERSION, ui_c(C_RESET), ui_c(C_DIM), G_V, ui_c(C_RESET));
    ui_link(BRAND_SITE, BRAND_SITE_TXT, C_GRAY);
    if (g_width >= 96) {
        printf("  %s%s%s  ", ui_c(C_DIM), G_V, ui_c(C_RESET));
        ui_link(BRAND_GH, BRAND_GH_TXT, C_GRAY);
    }
    printf("\n");
    int w = g_width - 4;
    rule(2, w > 100 ? 100 : w);
}

void ui_game(const char *name, uint32_t appid)
{
    printf("\n  %s%s%s %s%s", ui_c(C_CYAN), G_ARROW, ui_c(C_RESET), ui_c(C_BOLD), ui_c(C_WHITE));
    printf("%s", name && *name ? name : "Jogo");
    printf("%s  %sAppID %u%s\n\n", ui_c(C_RESET), ui_c(C_GRAY), (unsigned)appid, ui_c(C_RESET));
}

/* ------------------------------------------------------------------ */
/* Mensagens                                                           */
/* ------------------------------------------------------------------ */

static void msg(int color, const char *tag, const char *fmt, va_list ap)
{
    printf("  %s%s%s ", ui_c(color), tag, ui_c(C_RESET));
    vprintf(fmt, ap);
    printf("\n");
    ui_flush();
}

void ui_ok(const char *fmt, ...)   { va_list ap; va_start(ap, fmt); msg(C_GREEN,  G_SQ,    fmt, ap); va_end(ap); }
void ui_info(const char *fmt, ...) { va_list ap; va_start(ap, fmt); msg(C_CYAN,   G_ARROW, fmt, ap); va_end(ap); }
void ui_warn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); msg(C_YELLOW, "!",     fmt, ap); va_end(ap); }
void ui_err(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); msg(C_RED,    G_TIMES, fmt, ap); va_end(ap); }

/* ------------------------------------------------------------------ */
/* Spinner e barra de progresso                                        */
/* ------------------------------------------------------------------ */

static const char *const SPIN[4] = { "\xE2\x96\x96", "\xE2\x96\x98", "\xE2\x96\x9D", "\xE2\x96\x97" };
static const char *g_spin_msg = "";
static int g_spin_frame;

void ui_spinner_start(const char *m)
{
    g_spin_msg = m;
    g_spin_frame = 0;
    if (!g_color) { printf("  ... %s\n", m); ui_flush(); return; }
    ui_spinner_tick();
}

void ui_spinner_tick(void)
{
    if (!g_color) return;
    printf("\r\x1b[2K  %s%s%s %s%s%s", ui_c(C_CYAN), SPIN[g_spin_frame++ & 3], ui_c(C_RESET),
           ui_c(C_TEXT), g_spin_msg, ui_c(C_RESET));
    ui_flush();
}

void ui_spinner_stop(void)
{
    if (g_color) { printf("\r\x1b[2K"); ui_flush(); }
}

void ui_progress(int done, int total, const char *suffix)
{
    const int W = 30;
    int fill = total > 0 ? done * W / total : W;
    int pct = total > 0 ? done * 100 / total : 100;

    if (!g_color) {
        printf("\r  [");
        for (int i = 0; i < W; ++i) printf("%c", i < fill ? '#' : '-');
        printf("] %d/%d %3d%%  ", done, total, pct);
        ui_cell(suffix ? suffix : "", 30);
        ui_flush();
        return;
    }

    printf("\r\x1b[2K  ");
    for (int i = 0; i < W; ++i) {
        if (i < fill) { grad(i, W); printf("%s", G_FULL); }
        else          { printf("%s%s", ui_c(C_DIM), G_LIGHT); }
    }
    printf("%s  %s%s%d%s%s/%d  %3d%%%s", ui_c(C_RESET), ui_c(C_BOLD), ui_c(C_WHITE), done,
           ui_c(C_RESET), ui_c(C_GRAY), total, pct, ui_c(C_RESET));
    if (suffix && *suffix) {
        int room = g_width - (2 + W + 2 + 16 + 4);
        if (room > 8) { printf("  %s", ui_c(C_GRAY)); ui_cell(suffix, room); printf("%s", ui_c(C_RESET)); }
    }
    ui_flush();
}

void ui_progress_end(void) { printf("\n"); ui_flush(); }

/* ------------------------------------------------------------------ */
/* Tabela e entrada                                                    */
/* ------------------------------------------------------------------ */

void ui_hrule(const char *l, const char *mid, const char *r, const int *w, int n)
{
    printf("  %s%s", ui_c(C_DIM), l);
    for (int i = 0; i < n; ++i) {
        ui_repeat(G_H, w[i] + 2);
        printf("%s", i < n - 1 ? mid : r);
    }
    printf("%s\n", ui_c(C_RESET));
}

int ui_read_line(char *buf, int cap)
{
    int n = 0, c;
    ui_flush();
    while ((c = getchar()) != EOF && c != '\n')
        if (c != '\r' && n + 1 < cap) buf[n++] = (char)c;
    buf[n] = '\0';
    return (c == EOF && n == 0) ? -1 : n;
}

void ui_prompt(const char *label)
{
    printf("\n  %s%s%s %s%s%s ", ui_c(C_CYAN), G_ARROW, ui_c(C_RESET), ui_c(C_WHITE), label, ui_c(C_RESET));
    ui_flush();
}

void ui_wait_enter(const char *m)
{
    char tmp[8];
    printf("\n  %s%s%s", ui_c(C_GRAY), m, ui_c(C_RESET));
    ui_read_line(tmp, sizeof tmp);
}
