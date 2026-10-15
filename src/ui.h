/* ui.h - interface de terminal CentralTech (cores 24-bit, caixas, barras). */
#ifndef UI_H
#define UI_H

#include "compat.h"

/* Glifos UTF-8 escritos em bytes pra nao depender do encoding do compilador. */
#define G_H      "\xE2\x94\x80"   /* ─ */
#define G_V      "\xE2\x94\x82"   /* │ */
#define G_TL     "\xE2\x94\x8C"   /* ┌ */
#define G_TR     "\xE2\x94\x90"   /* ┐ */
#define G_BL     "\xE2\x94\x94"   /* └ */
#define G_BR     "\xE2\x94\x98"   /* ┘ */
#define G_TT     "\xE2\x94\xAC"   /* ┬ */
#define G_BT     "\xE2\x94\xB4"   /* ┴ */
#define G_LT     "\xE2\x94\x9C"   /* ├ */
#define G_RT     "\xE2\x94\xA4"   /* ┤ */
#define G_X      "\xE2\x94\xBC"   /* ┼ */
#define G_ON     "\xE2\x97\x8F"   /* ● */
#define G_OFF    "\xE2\x97\x8B"   /* ○ */
#define G_ARROW  "\xE2\x96\xBA"   /* ► */
#define G_FULL   "\xE2\x96\x88"   /* █ */
#define G_LIGHT  "\xE2\x96\x91"   /* ░ */
#define G_SQ     "\xE2\x96\xA0"   /* ■ */
#define G_DOT    "\xC2\xB7"       /* · */
#define G_DASH   "\xE2\x80\x94"   /* — */
#define G_ELLIP  "\xE2\x80\xA6"   /* … */
#define G_TIMES  "\xC3\x97"       /* × */

enum {
    C_RESET, C_BOLD, C_WHITE, C_TEXT, C_GRAY, C_DIM,
    C_BLUE, C_BLUE2, C_CYAN, C_GREEN, C_RED, C_YELLOW, C__COUNT
};

void        ui_init(bool plain);
bool        ui_has_color(void);
int         ui_width(void);
const char *ui_c(int color);

void ui_banner(void);                       /* logo grande */
void ui_header(void);                       /* cabecalho compacto de 2 linhas */
void ui_game(const char *name, uint32_t appid);
void ui_clear(void);

void ui_ok(const char *fmt, ...);
void ui_info(const char *fmt, ...);
void ui_warn(const char *fmt, ...);
void ui_err(const char *fmt, ...);

void ui_spinner_tick(void);                 /* usar como callback */
void ui_spinner_start(const char *msg);
void ui_spinner_stop(void);

void ui_progress(int done, int total, const char *suffix);   /* sobrescreve a linha */
void ui_progress_end(void);

/* tabela */
void ui_cell(const char *s, int width);     /* imprime truncando/preenchendo por codepoint */
void ui_hrule(const char *l, const char *mid, const char *r, const int *w, int n);
void ui_repeat(const char *glyph, int n);
void ui_pad(int n);
int  ui_strwidth(const char *s);

int  ui_read_line(char *buf, int cap);      /* le uma linha do stdin (sem \n) */
void ui_prompt(const char *label);
void ui_wait_enter(const char *msg);

void ui_link(const char *url, const char *text, int color);
void ui_badge(const char *text, int r, int g, int b);
void ui_flush(void);

#endif
