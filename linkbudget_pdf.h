#ifndef LINKBUDGET_PDF_H
#define LINKBUDGET_PDF_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

/* ============================================================
 * Extern globals needed by PDF routines
 * ============================================================ */
extern HWND g_hwnd;

/* ============================================================
 * Growing byte buffer
 * ============================================================ */
typedef struct { char *d; int n, cap; } PB;

void pb_reserve(PB *b, int extra);
void pb_printf(PB *b, const char *fmt, ...);
void pb_free(PB *b);

/* ============================================================
 * PDF helpers
 * ============================================================ */
void pdf_esc(const char *s, char *out, int outlen);

/* Page layout context */
typedef struct { PB *cs; double y, ml, cw, rh, sh; } PPage;

void ppg_hline(PPage *pg, double r, double g, double b, double lw);
void ppg_section(PPage *pg, const char *title);
void ppg_row(PPage *pg, const char *lbl, const char *val, const char *unit);
void ppg_result(PPage *pg, const char *lbl, const char *val,
                const char *unit, double rv, double gv, double bv);
void combo_sel_text(HWND combo, char *buf, int len);

/* ============================================================
 * Top-level report entry point
 * ============================================================ */
void print_report(void);

#endif /* LINKBUDGET_PDF_H */
