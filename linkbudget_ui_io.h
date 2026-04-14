#ifndef LINKBUDGET_UI_IO_H
#define LINKBUDGET_UI_IO_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>

/* ============================================================
 * Extern globals needed by IO routines
 * ============================================================ */
extern HWND g_hwnd;

/* Unit state (read by save/load) */
extern int g_unit_freq;
extern int g_unit_txalt;
extern int g_unit_rxalt;
extern int g_unit_satpow;
extern int g_unit_feedpow;
extern int g_unit_rxpow;
extern int g_unit_minalt;
extern int g_unit_eirp;

/* Unit label HWNDs (written by load) */
extern HWND h_unit_freq;
extern HWND h_unit_txalt;
extern HWND h_unit_rxalt;
extern HWND h_unit_satpow;
extern HWND h_unit_feedpow;
extern HWND h_unit_rxpow;
extern HWND h_unit_minalt;
extern HWND h_unit_eirp;

/* Combo HWNDs (written by load) */
extern HWND h_fec, h_mod, h_ber;
extern HWND h_range_mode, h_tx_mode, h_rx_mode, h_loss_mode;

/* Unit name tables (defined in linkbudget_gui.c) */
extern const char *UNIT_FREQ_NAMES[];
extern const char *UNIT_ALT_NAMES[];
extern const char *UNIT_POW_NAMES[];
extern const char *UNIT_FEEDPOW_NAMES[];
extern const char *UNIT_RXPOW_NAMES[];
extern const char *UNIT_MINALT_NAMES[];
extern const char *UNIT_EIRP_NAMES[];

/* ============================================================
 * File I/O helpers
 * ============================================================ */
void write_field(FILE *f, const char *key, HWND hw);
void read_field(FILE *f, const char *key, HWND hw);
int  read_int(FILE *f, const char *key, int def);

/* ============================================================
 * Scenario / Export / CSV
 * ============================================================ */
void save_scenario(void);
void load_scenario(void);
void export_csv(void);

#endif /* LINKBUDGET_UI_IO_H */
