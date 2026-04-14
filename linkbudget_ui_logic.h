#ifndef LINKBUDGET_UI_LOGIC_H
#define LINKBUDGET_UI_LOGIC_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "linkbudget_core.h"

/* ============================================================
 * Extern globals needed by logic routines
 * ============================================================ */
extern HWND g_hwnd;
extern BOOL g_ui_ready;

/* ============================================================
 * Helper: read a double from an edit control
 * ============================================================ */
double get_double(HWND hw);

/* ============================================================
 * get_combo_index helper — wraps CB_GETCURSEL
 * ============================================================ */
int get_combo_index(HWND h);

/* ============================================================
 * Visibility helpers
 * ============================================================ */
void show_group(HWND *arr, int n, BOOL show);
void update_range_visibility(void);
void update_ant_visibility(void);
void update_loss_visibility(void);
void update_visibility(void);

/* ============================================================
 * Unit cycling
 * ============================================================ */
void cycle_unit(int id);

/* ============================================================
 * Core calculation
 * ============================================================ */
int  fill_params(lb_params_t *p);
void do_calculate(void);

#endif /* LINKBUDGET_UI_LOGIC_H */
