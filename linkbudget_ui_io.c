/*
 * linkbudget_ui_io.c — Scenario file I/O and CSV export.
 * Implements a simple key=value text format (.lbf) for save/load,
 * and a flat CSV export of all computed results.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "linkbudget_gui_config.h"
#include "linkbudget_core.h"
#include "linkbudget_ui_io.h"
#include "linkbudget_ui_logic.h"   /* update_visibility, do_calculate, fill_params */

/* ============================================================
 * Field table (edit HWNDs referenced via the globals in gui.c)
 * ============================================================ */

/* All edit HWNDs are declared extern in the header chain; they live in
   linkbudget_gui.c.  We re-declare the ones the field table needs here
   so the linker can resolve them without polluting the header with every
   input HWND. */
extern HWND h_freq, h_datarate;
extern HWND h_txalt, h_rxalt, h_slant, h_horiz_dist, h_radius, h_imploss;
extern HWND h_satpow, h_obo, h_feedtx, h_txgain;
extern HWND h_tx_diameter, h_tx_efficiency;
extern HWND h_ar_tx, h_ar_wave, h_polangle;
extern HWND h_absloss, h_nonabsloss, h_rainrate;
extern HWND h_rxgain_direct, h_diameter, h_efficiency, h_trackloss;
extern HWND h_rxnoise, h_feedrx, h_nf;

/* Combo HWNDs needed for save/load of mode selectors */
extern HWND h_fec, h_mod, h_ber;
extern HWND h_range_mode, h_tx_mode, h_rx_mode, h_loss_mode;

/* Unit label HWNDs — text is updated on load to reflect restored state */
extern HWND h_unit_freq, h_unit_txalt, h_unit_rxalt, h_unit_satpow;
extern HWND h_unit_feedpow, h_unit_rxpow, h_unit_minalt, h_unit_eirp;

/* Unit state variables and string tables */
extern int g_unit_freq, g_unit_txalt, g_unit_rxalt, g_unit_satpow;
extern int g_unit_feedpow, g_unit_rxpow, g_unit_minalt, g_unit_eirp;
extern const char *UNIT_FREQ_NAMES[], *UNIT_ALT_NAMES[];
extern const char *UNIT_POW_NAMES[], *UNIT_FEEDPOW_NAMES[];
extern const char *UNIT_RXPOW_NAMES[], *UNIT_MINALT_NAMES[];
extern const char *UNIT_EIRP_NAMES[];

/* g_fields: maps string keys (used in the .lbf file) to the corresponding
 * edit HWND pointers.  This table drives both save and load in a single loop,
 * eliminating the need to keep two separate switch/case blocks in sync. */
static const struct { const char *key; HWND *phwnd; } g_fields[] = {
    {"freq",       &h_freq},        {"datarate",    &h_datarate},
    {"txalt",      &h_txalt},       {"rxalt",       &h_rxalt},
    {"slant",      &h_slant},       {"horiz",       &h_horiz_dist},
    {"radius",     &h_radius},      {"imploss",     &h_imploss},
    {"satpow",     &h_satpow},      {"obo",         &h_obo},
    {"feedtx",     &h_feedtx},      {"txgain",      &h_txgain},
    {"tx_diam",    &h_tx_diameter}, {"tx_eff",      &h_tx_efficiency},
    {"ar_tx",      &h_ar_tx},       {"ar_wave",     &h_ar_wave},
    {"polangle",   &h_polangle},    {"absloss",     &h_absloss},
    {"nonabsloss", &h_nonabsloss},  {"rainrate",    &h_rainrate},
    {"rxgain",     &h_rxgain_direct},{"diam",       &h_diameter},
    {"eff",        &h_efficiency},  {"trackloss",   &h_trackloss},
    {"rxnoise",    &h_rxnoise},     {"feedrx",      &h_feedrx},
    {"nf",         &h_nf},
};

/* ============================================================
 * Low-level helpers
 * ============================================================ */

/* Write a single "key=value\n" line where value is the edit control's text */
void write_field(FILE *f, const char *key, HWND hw)
{
    char buf[128];
    GetWindowTextA(hw, buf, sizeof(buf));
    fprintf(f, "%s=%s\n", key, buf);
}

/* Search the file for a line starting with "key=" and set the HWND's text.
 * rewind() before each search to allow keys to be in any order. */
void read_field(FILE *f, const char *key, HWND hw)
{
    rewind(f);
    char line[256];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *val = line + klen + 1;
            size_t vlen = strlen(val);
            /* Strip trailing CR/LF before setting window text */
            while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r'))
                val[--vlen] = '\0';
            SetWindowTextA(hw, val);
            return;
        }
    }
    /* If the key is not found the control retains its current value */
}

/* Read an integer value from the file, returning def if not found.
 * Used for combo indices and unit state integers. */
int read_int(FILE *f, const char *key, int def)
{
    rewind(f);
    char line[256];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, key, klen) == 0 && line[klen] == '=')
            return atoi(line + klen + 1);
    return def; /* key not present in file: use caller-supplied default */
}

/* ============================================================
 * save_scenario
 * ============================================================ */
void save_scenario(void)
{
    char path[MAX_PATH] = "scenario.lbf";
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = "Link Budget Files (*.lbf)\0*.lbf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;  ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "lbf"; /* auto-append extension if user omits it */
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return; /* user cancelled */

    FILE *f = fopen(path, "w");
    if (!f) {
        MessageBoxA(g_hwnd, "Cannot create file.", "Save", MB_ICONERROR | MB_OK);
        return;
    }

    /* Save unit states first so they are restored before field values on load,
     * ensuring the values are interpreted in the correct units. */
    fprintf(f, "unit_freq=%d\nunit_txalt=%d\nunit_rxalt=%d\nunit_satpow=%d\n",
            g_unit_freq, g_unit_txalt, g_unit_rxalt, g_unit_satpow);
    fprintf(f, "unit_feedpow=%d\nunit_rxpow=%d\nunit_minalt=%d\nunit_eirp=%d\n",
            g_unit_feedpow, g_unit_rxpow, g_unit_minalt, g_unit_eirp);
    /* Combo indices — saved as integers, restored with CB_SETCURSEL */
    fprintf(f, "fec=%d\nmod=%d\nber=%d\n",
            get_combo_index(h_fec),
            get_combo_index(h_mod),
            get_combo_index(h_ber));
    fprintf(f, "range_mode=%d\ntx_mode=%d\nrx_mode=%d\nloss_mode=%d\n",
            get_combo_index(h_range_mode),
            get_combo_index(h_tx_mode),
            get_combo_index(h_rx_mode),
            get_combo_index(h_loss_mode));
    /* Edit fields — saved as the raw text currently displayed */
    for (int i = 0; i < NELEM(g_fields); i++)
        write_field(f, g_fields[i].key, *g_fields[i].phwnd);
    fclose(f);
    MessageBoxA(g_hwnd, "Scenario saved.", "Save", MB_ICONINFORMATION | MB_OK);
}

/* ============================================================
 * load_scenario
 * ============================================================ */
void load_scenario(void)
{
    char path[MAX_PATH] = "";
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = "Link Budget Files (*.lbf)\0*.lbf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;  ofn.nMaxFile = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return; /* user cancelled */

    FILE *f = fopen(path, "r");
    if (!f) {
        MessageBoxA(g_hwnd, "Cannot open file.", "Load", MB_ICONERROR | MB_OK);
        return;
    }

    /* Restore unit states — use existing values as defaults for backward compat */
    g_unit_freq    = read_int(f, "unit_freq",    g_unit_freq);
    g_unit_txalt   = read_int(f, "unit_txalt",   g_unit_txalt);
    g_unit_rxalt   = read_int(f, "unit_rxalt",   g_unit_rxalt);
    g_unit_satpow  = read_int(f, "unit_satpow",  g_unit_satpow);
    g_unit_feedpow = read_int(f, "unit_feedpow", g_unit_feedpow);
    g_unit_rxpow   = read_int(f, "unit_rxpow",   g_unit_rxpow);
    g_unit_minalt  = read_int(f, "unit_minalt",  g_unit_minalt);
    g_unit_eirp    = read_int(f, "unit_eirp",    g_unit_eirp);
    /* Sync clickable unit label text to the restored state */
    SetWindowTextA(h_unit_freq,    UNIT_FREQ_NAMES[g_unit_freq]);
    SetWindowTextA(h_unit_txalt,   UNIT_ALT_NAMES[g_unit_txalt]);
    SetWindowTextA(h_unit_rxalt,   UNIT_ALT_NAMES[g_unit_rxalt]);
    SetWindowTextA(h_unit_satpow,  UNIT_POW_NAMES[g_unit_satpow]);
    SetWindowTextA(h_unit_feedpow, UNIT_FEEDPOW_NAMES[g_unit_feedpow]);
    SetWindowTextA(h_unit_rxpow,   UNIT_RXPOW_NAMES[g_unit_rxpow]);
    SetWindowTextA(h_unit_minalt,  UNIT_MINALT_NAMES[g_unit_minalt]);
    SetWindowTextA(h_unit_eirp,    UNIT_EIRP_NAMES[g_unit_eirp]);
    /* Restore combos — fallback defaults match the application startup defaults */
    SendMessage(h_fec,        CB_SETCURSEL, (WPARAM)read_int(f, "fec",        0), 0);
    SendMessage(h_mod,        CB_SETCURSEL, (WPARAM)read_int(f, "mod",        1), 0);
    SendMessage(h_ber,        CB_SETCURSEL, (WPARAM)read_int(f, "ber",        4), 0);
    SendMessage(h_range_mode, CB_SETCURSEL, (WPARAM)read_int(f, "range_mode", 1), 0);
    SendMessage(h_tx_mode,    CB_SETCURSEL, (WPARAM)read_int(f, "tx_mode",    0), 0);
    SendMessage(h_rx_mode,    CB_SETCURSEL, (WPARAM)read_int(f, "rx_mode",    0), 0);
    SendMessage(h_loss_mode,  CB_SETCURSEL, (WPARAM)read_int(f, "loss_mode",  0), 0);
    /* Restore all edit field values */
    for (int i = 0; i < NELEM(g_fields); i++)
        read_field(f, g_fields[i].key, *g_fields[i].phwnd);
    fclose(f);
    /* Reapply visibility rules and recalculate after restoring all state */
    update_visibility();
    do_calculate();
}

/* ============================================================
 * export_csv
 * ============================================================ */
void export_csv(void)
{
    char path[MAX_PATH] = "linkbudget_export.csv";
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = "CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;  ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "csv";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;

    FILE *f = fopen(path, "w");
    if (!f) {
        MessageBoxA(g_hwnd, "Cannot create file.", "Export", MB_ICONERROR | MB_OK);
        return;
    }

    /* Replicate the same two-pass calculation as do_calculate() to ensure
     * the exported values are consistent with what the GUI shows. */
    lb_params_t p = {0};
    if (!fill_params(&p)) {
        fclose(f);
        MessageBoxA(g_hwnd, "Invalid parameters.", "Export", MB_ICONERROR | MB_OK);
        return;
    }
    lb_result_t r = lb_run(&p);
    lb_geometry_t geo = lb_geometry_angles(p.tx_alt_km, p.rx_alt_km,
                                            r.slant_range_km_used, p.radius_factor);
    double el_deg = (geo.rx_el_deg > 5.0) ? geo.rx_el_deg : 5.0;
    int use_itu = (get_combo_index(h_loss_mode) == LOSS_MODE_ITU);
    double gas_dB = 0.0, rain_dB = 0.0;
    if (use_itu) {
        gas_dB  = lb_gaseous_atten_dB(p.freq_GHz, el_deg,
                                       ITU_STD_P_HPA, ITU_STD_T_C, ITU_STD_RHO_GM3);
        {
            char b[64]; GetWindowTextA(h_rainrate, b, sizeof(b));
            /* Effective rain path: vertical projection of the slant range */
            rain_dB = lb_rain_atten_dB(p.freq_GHz, atof(b),
                                        r.slant_range_km_used * sin(el_deg * M_PI / 180.0));
        }
        p.absorptive_loss_dB += gas_dB + rain_dB;
        r = lb_run(&p); /* re-run with ITU-R losses included */
    }
    double total_loss = r.fspl_dB + p.absorptive_loss_dB + p.non_absorptive_loss_dB;

    /* Write a simple three-column CSV: Parameter, Value, Unit.
     * For display units (freq, altitude, power) we preserve the currently
     * selected unit so the exported file matches what the user sees on screen. */
    fprintf(f, "Parameter,Value,Unit\n");
    { char b[64]; GetWindowTextA(h_freq,    b, sizeof(b));
      fprintf(f, "Frequency,%.6g,%s\n",   atof(b), UNIT_FREQ_NAMES[g_unit_freq]); }
    fprintf(f, "Data Rate,%.4g,Mb/s\n",      p.data_rate_Mbps);
    { char b[64]; GetWindowTextA(h_txalt,   b, sizeof(b));
      fprintf(f, "Tx Altitude,%.6g,%s\n", atof(b), UNIT_ALT_NAMES[g_unit_txalt]); }
    { char b[64]; GetWindowTextA(h_rxalt,   b, sizeof(b));
      fprintf(f, "Rx Altitude,%.6g,%s\n", atof(b), UNIT_ALT_NAMES[g_unit_rxalt]); }
    fprintf(f, "Slant Range,%.3f,km\n",      r.slant_range_km_used);
    fprintf(f, "Tx EL Angle,%.2f,deg\n",     geo.tx_el_deg);
    fprintf(f, "Rx EL Angle,%.2f,deg\n",     geo.rx_el_deg);
    { char b[64]; GetWindowTextA(h_satpow,  b, sizeof(b));
      fprintf(f, "Saturated Power,%.4g,%s\n", atof(b), UNIT_POW_NAMES[g_unit_satpow]); }
    fprintf(f, "Output Backoff,%.4g,dB\n",   p.obo_dB);
    fprintf(f, "Feed Loss TX,%.4g,dB\n",     p.feed_loss_tx_dB);
    fprintf(f, "EIRP,%.2f,dBW\n",            r.eirp_dBW);
    fprintf(f, "Pol. Loss,%.2f,dB\n",        p.pol_loss_dB);
    fprintf(f, "Free-Space PL,%.2f,dB\n",    r.fspl_dB);
    /* ITU-R lines appear in the CSV regardless of mode; values are 0 when inactive */
    fprintf(f, "Gaseous Atten. (ITU-R P.676),%.3f,dB\n", gas_dB);
    fprintf(f, "Rain Atten. (ITU-R P.838),%.3f,dB\n",    rain_dB);
    { char b[64]; GetWindowTextA(h_absloss,    b, sizeof(b));
      fprintf(f, "Absorptive Loss,%.4g,dB\n",     atof(b)); }
    { char b[64]; GetWindowTextA(h_nonabsloss, b, sizeof(b));
      fprintf(f, "Non-absorptive Loss,%.4g,dB\n", atof(b)); }
    fprintf(f, "Total Path Loss,%.2f,dB\n",  total_loss);
    fprintf(f, "Rx Antenna Gain,%.2f,dBi\n", p.rx_ant_gain_dBi);
    fprintf(f, "System Noise Temp,%.2f,K\n", r.t_sys_K);
    fprintf(f, "G/T,%.2f,dB/K\n",            r.gt_dBK);
    fprintf(f, "C/N0,%.2f,dBHz\n",           r.cn0_dBHz);
    fprintf(f, "Eb/N0 Available,%.2f,dB\n",  r.ebn0_dB);
    fprintf(f, "Eb/N0 Required,%.2f,dB\n",   r.req_ebn0_dB);
    fprintf(f, "Link Margin,%.2f,dB\n",      r.link_margin_dB);
    fprintf(f, "Link Status,%s,\n",          r.closed ? "CLOSED" : "OPEN");
    fclose(f);
    MessageBoxA(g_hwnd, "Export complete.", "Export", MB_ICONINFORMATION | MB_OK);
}
