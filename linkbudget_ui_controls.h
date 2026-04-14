#ifndef LINKBUDGET_UI_CONTROLS_H
#define LINKBUDGET_UI_CONTROLS_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

/* ============================================================
 * Extern globals needed by control-building routines
 * ============================================================ */
extern HINSTANCE g_hinst;
extern HWND      g_hwnd;

/* Fonts */
extern HFONT g_fnt_label, g_fnt_entry, g_fnt_hdr, g_fnt_rval, g_fnt_margin;
/* Brushes */
extern HBRUSH g_br_bg, g_br_panel, g_br_hdr, g_br_entry, g_br_res;

/* Section data */
typedef struct { char title[64]; int sep_y; } SecData;
extern SecData g_sd[8];
extern HWND h_sec[8];
extern HWND h_resbar;
extern HWND h_btn;

/* ============================================================
 * Low-level control creators
 * ============================================================ */
HWND mk_label(HWND p, const char *t, int x, int y, int w, int h);
HWND mk_unit(HWND p, const char *t, int x, int y, int w);
HWND mk_edit(HWND p, const char *def, int x, int y, int w, int h);
HWND mk_combo(HWND p, const char **items, int n, int x, int y, int w);
HWND mk_calc(HWND p, int x, int y, int id);
HWND mk_unit_link(HWND p, const char *t, int x, int y, int w, int id);

/* ============================================================
 * Section / Results / Button builders
 * ============================================================ */
HWND make_sec(int idx, int x, int y, int w, int h,
              const char *title, int sep_y);

void build_sec1(void);
void build_sec2(void);
void build_sec3(void);
void build_sec4(void);
void build_sec5(void);
void build_sec6(void);
void build_sec7(void);
void build_sec8(void);
void build_results(void);
void build_button(void);

/* ============================================================
 * Window procedures
 * ============================================================ */
LRESULT CALLBACK SectionPanelProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ResultsPanelProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);

#endif /* LINKBUDGET_UI_CONTROLS_H */
