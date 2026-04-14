/*
 * linkbudget_gui.c — Top-level Win32 GUI module.
 * Owns WinMain, WndProc, all global HWNDs/fonts/brushes, the sweep analysis
 * window, tooltip registration, and owner-draw buttons.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "version.h"
#include <stdarg.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "linkbudget_core.h"
#include "linkbudget_gui_config.h"
#include "linkbudget_ui_controls.h"
#include "linkbudget_ui_logic.h"
#include "linkbudget_ui_io.h"
#include "linkbudget_pdf.h"

/* ============================================================
 * ============================================================ */

/* ============================================================
 * FEC / Modulation / BER / unit string arrays  (definitions)
 * These arrays are extern-declared by ui_controls, ui_logic, and ui_io
 * so they are defined once here and shared across all modules.
 * ============================================================ */
const char *FEC_NAMES[LB_FEC_COUNT] = {
    "Uncoded",
    "VA 1/2 (K=7)", "VA 3/4 (K=7)", "VA 7/8 (K=7)",
    "RS(255,223) + VA 1/2",
    "Turbo r=1/6", "Turbo r=1/3", "Turbo r=1/2", "Turbo r=3/4",
    "LDPC r=1/2", "LDPC r=2/3", "LDPC r=3/4", "LDPC r=5/6",
    "DVB-S2 r=1/2", "DVB-S2 r=3/4", "DVB-S2 r=2/3", "DVB-S2 r=3/4+",
    "CCSDS CC r=1/2 K=7", "CCSDS Turbo r=1/6"
};
const char *MOD_NAMES[LB_MOD_COUNT] = {
    "BPSK","QPSK","OQPSK","8PSK","16APSK","32APSK",
    "16QAM","64QAM","128QAM","256QAM"
};
const char *BER_NAMES[LB_BER_COUNT] = {
    "1e-2","1e-3","1e-4","1e-5","1e-6","1e-7","1e-8"
};
const char *ANT_MODE_NAMES[]   = { "Antenna Gain", "Parabolic Dish" };
const char *RANGE_MODE_NAMES[] = { "Slant Range",  "Horiz. Dist." };
const char *LOSS_MODE_NAMES[]  = { "Simple FSPL",  "ITU-R (P.676 + P.838)" };

/* Clickable unit label strings and their conversion factors to canonical units */
const char  *UNIT_FREQ_NAMES[]    = { "GHz", "MHz", "kHz" };
const double FREQ_TO_GHZ[]        = { 1.0, 1e-3, 1e-6 };  /* multiply displayed value by this to get GHz */
const char  *UNIT_ALT_NAMES[]     = { "m", "ft", "km", "NM" };
const double ALT_TO_M[]           = { 1.0, 0.3048, 1000.0, 1852.0 }; /* multiply displayed value by this to get metres */
const char  *UNIT_POW_NAMES[]     = { "W", "dBm" };
const char  *UNIT_FEEDPOW_NAMES[] = { "W", "dBm", "dBW" };
const char  *UNIT_RXPOW_NAMES[]   = { "dBm", "dBW", "W" };
const char  *UNIT_MINALT_NAMES[]  = { "ft", "m", "km" };
const char  *UNIT_EIRP_NAMES[]    = { "dBW", "dBm", "W" };

/* ============================================================
 * Globals
 * ============================================================ */
HINSTANCE g_hinst;
HWND      g_hwnd;
BOOL      g_ui_ready    = FALSE; /* set TRUE after WM_CREATE completes; guards against spurious EN_CHANGE */
COLORREF  g_margin_color = C_NEG; /* updated by do_calculate() to reflect current margin sign */

/* Fonts */
HFONT g_fnt_label, g_fnt_entry, g_fnt_hdr, g_fnt_rval, g_fnt_margin;
/* Brushes */
HBRUSH g_br_bg, g_br_panel, g_br_hdr, g_br_entry, g_br_res;

/* --- Input HWNDs — one per user-editable field --- */
HWND h_freq, h_datarate;
HWND h_fec, h_mod, h_ber, h_imploss;
HWND h_txalt, h_rxalt, h_slant, h_radius;
HWND h_satpow, h_obo, h_feedtx;
HWND h_txgain, h_ar_tx, h_ar_wave, h_polangle;
HWND h_absloss, h_nonabsloss;
HWND h_diameter, h_efficiency, h_trackloss;
HWND h_rxnoise, h_feedrx, h_nf;

/* --- Calculated value display HWNDs (read-only STATIC controls) --- */
HWND h_cv_reqebn0;
HWND h_cv_txel, h_cv_rxel, h_cv_central, h_cv_minalt;
HWND h_cv_feedpow;
HWND h_cv_polloss, h_cv_eirp;
HWND h_cv_fspl, h_cv_totloss;
HWND h_cv_rxgain, h_cv_beamwid, h_cv_rxflux, h_cv_rxpow;
HWND h_cv_tsys, h_cv_gt, h_cv_cn0;

/* --- Final result display HWNDs --- */
HWND h_rv_req, h_rv_ebn0, h_rv_margin, h_rv_status;

/* --- Range mode (Sec 3) — mutually exclusive input set --- */
HWND h_range_mode;
HWND h_horiz_dist;       /* visible in HORIZ mode */
HWND h_cv_slant_calc;    /* calculated slant range shown when HORIZ mode active */
HWND h_cv_slant_lbl, h_cv_slant_unit;
HWND h_cv_minalt_lbl, h_cv_minalt_unit;

/* --- Antenna mode selectors — swap between direct-gain and dish inputs --- */
HWND h_tx_mode, h_rx_mode;
HWND h_tx_diameter, h_tx_efficiency;
HWND h_tx_lbl_gain, h_tx_unit_gain;
HWND h_tx_lbl_diam, h_tx_unit_diam;
HWND h_tx_lbl_eff,  h_tx_unit_eff;
HWND h_rxgain_direct;
HWND h_rx_lbl_gain, h_rx_unit_gain;
HWND h_rx_lbl_diam, h_rx_unit_diam;
HWND h_rx_lbl_eff,  h_rx_unit_eff;
HWND h_cv_beamwid_lbl, h_cv_beamwid_unit;

/* --- Clickable unit state — index into the corresponding UNIT_*_NAMES array --- */
int  g_unit_freq    = 1;  /* 0=GHz, 1=MHz, 2=kHz  — default MHz */
int  g_unit_txalt   = 1;  /* 0=m, 1=ft, 2=km, 3=NM — default ft */
int  g_unit_rxalt   = 1;
int  g_unit_satpow  = 1;  /* 0=W, 1=dBm — default dBm */
int  g_unit_feedpow = 0;  /* 0=W, 1=dBm, 2=dBW */
int  g_unit_rxpow   = 0;  /* 0=dBm, 1=dBW, 2=W */
int  g_unit_minalt  = 0;  /* 0=ft, 1=m, 2=km */
int  g_unit_eirp    = 0;  /* 0=dBW, 1=dBm, 2=W */
/* HWNDs of the clickable unit labels (SS_NOTIFY STATICs that receive STN_CLICKED) */
HWND h_unit_freq, h_unit_txalt, h_unit_rxalt, h_unit_satpow;
HWND h_unit_feedpow, h_unit_rxpow, h_unit_minalt, h_unit_eirp;

/* Utility button HWNDs */
HWND h_btn_save, h_btn_load, h_btn_sweep, h_btn_export, h_btn_print, h_btn_about;

/* Sec 6 — path loss model + ITU-R fields */
HWND h_loss_mode;
HWND h_rainrate;
HWND h_rainrate_lbl, h_rainrate_unit;
HWND h_cv_gasatt, h_cv_rainatt;
HWND h_cv_gasatt_lbl, h_cv_gasatt_unit;
HWND h_cv_rainatt_lbl, h_cv_rainatt_unit;

/* Panel HWNDs — one per section card (0–7) */
HWND h_sec[8];
HWND h_resbar;  /* results bar across the bottom */
HWND h_btn;     /* main calculate button */

/* Section title / separator data (passed as create-params to SectionPanelProc) */
SecData g_sd[8];

/* ============================================================
 * Font / Brush helpers
 * ============================================================ */

/* MulDiv(pt, 96, 72) converts point size to logical pixels at 96 DPI;
 * negative height tells GDI to match the em-square (not cell height). */
static HFONT make_font(const char *face, int pt, BOOL bold)
{
    return CreateFont(
        -MulDiv(pt, 96, 72), 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, face);
}

static void create_fonts_brushes(void)
{
    g_fnt_label  = make_font("Segoe UI",  9, FALSE);
    g_fnt_entry  = make_font("Consolas", 10, FALSE); /* monospace for numeric fields */
    g_fnt_hdr    = make_font("Segoe UI", 10, TRUE);
    g_fnt_rval   = make_font("Consolas", 11, TRUE);  /* bold result values */
    g_fnt_margin = make_font("Consolas", 14, TRUE);  /* large margin display */

    g_br_bg    = CreateSolidBrush(C_BG);
    g_br_panel = CreateSolidBrush(C_PANEL);
    g_br_hdr   = CreateSolidBrush(C_HDR_BG);
    g_br_entry = CreateSolidBrush(C_ENTRY_BG);
    g_br_res   = CreateSolidBrush(C_RES_BG);
}

static void delete_fonts_brushes(void)
{
    DeleteObject(g_fnt_label);  DeleteObject(g_fnt_entry);
    DeleteObject(g_fnt_hdr);    DeleteObject(g_fnt_rval);
    DeleteObject(g_fnt_margin);
    DeleteObject(g_br_bg);    DeleteObject(g_br_panel);
    DeleteObject(g_br_hdr);   DeleteObject(g_br_entry);
    DeleteObject(g_br_res);
}

/* ============================================================
 * Sweep window
 * Performs a 1-D parameter sweep and plots link margin vs. the
 * swept parameter.  Runs as a modeless child of the main window.
 * ============================================================ */
#define SW_PARAM_COUNT 6
static const char *SW_PARAM_NAMES[SW_PARAM_COUNT] = {
    "Horiz. Distance (km)",
    "Tx Altitude (m)",
    "Rx Altitude (m)",
    "Saturated Power (W)",
    "Frequency (GHz)",
    "Absorptive Loss (dB)"
};
#define SW_MAX_PTS 200  /* hard cap on sweep resolution to keep plotting fast */
static double g_sw_x[SW_MAX_PTS], g_sw_y[SW_MAX_PTS]; /* sweep results */
static int    g_sw_n  = 0;
static int    g_sw_cx = -1;           /* crosshair pixel-x, -1 = hidden */
static double g_sw_cv_x, g_sw_cv_y;  /* interpolated data values at crosshair */
static HWND   g_sw_wnd = NULL;        /* NULL when the sweep window is closed */
static HWND   h_sw_combo, h_sw_min, h_sw_max, h_sw_steps, h_sw_plot, h_sw_info;

static void sweep_run(void)
{
    int param = get_combo_index(h_sw_combo);
    double vmin = 0.0, vmax = 1.0;
    { char b[64]; GetWindowTextA(h_sw_min,   b, 64); vmin = atof(b); }
    { char b[64]; GetWindowTextA(h_sw_max,   b, 64); vmax = atof(b); }
    int npts = 100;
    { char b[32]; GetWindowTextA(h_sw_steps, b, 32); npts = atoi(b); }
    if (npts < 2)        npts = 2;
    if (npts > SW_MAX_PTS) npts = SW_MAX_PTS;
    if (vmax <= vmin) return; /* degenerate range — nothing to sweep */

    lb_params_t base = {0};
    if (!fill_params(&base)) return; /* abort if GUI state is invalid */

    g_sw_n = npts;
    for (int i = 0; i < npts; i++) {
        double v = vmin + (vmax - vmin) * i / (npts - 1); /* linearly spaced point */
        lb_params_t p = base; /* start from the current GUI snapshot each iteration */
        switch (param) {
        case 0: { /* Horiz. Distance: derive slant range from horizontal dist + altitude diff */
                  double dh = fabs(p.tx_alt_km - p.rx_alt_km);
                  p.slant_range_km = sqrt(v*v + dh*dh);
                  if (p.slant_range_km < 0.001) p.slant_range_km = 0.001; } break;
        case 1: p.tx_alt_km = v / 1000.0; break; /* m → km */
        case 2: p.rx_alt_km = v / 1000.0; break;
        case 3: p.sat_power_dBW = (v > 0) ? 10.0*log10(v) : -99.0; break; /* W → dBW */
        case 4: p.freq_GHz = v;           break;
        case 5: p.absorptive_loss_dB = v; break;
        }
        /* First pass: derive geometry so we can compute ITU-R elevation-dependent losses */
        lb_result_t r0 = lb_run(&p);
        lb_geometry_t geo = lb_geometry_angles(p.tx_alt_km, p.rx_alt_km,
                                                r0.slant_range_km_used, p.radius_factor);
        double el = (geo.rx_el_deg > 5.0) ? geo.rx_el_deg : 5.0; /* 5° floor for cosecant law */
        if (get_combo_index(h_loss_mode) == LOSS_MODE_ITU) {
            /* Add ITU-R gaseous and rain attenuation to the absorptive loss
             * and re-run so the sweep accounts for frequency/elevation dependence */
            p.absorptive_loss_dB +=
                lb_gaseous_atten_dB(p.freq_GHz, el,
                                    ITU_STD_P_HPA, ITU_STD_T_C, ITU_STD_RHO_GM3)
              + lb_rain_atten_dB(p.freq_GHz, get_double(h_rainrate),
                                  r0.slant_range_km_used * sin(el * M_PI / 180.0));
        }
        lb_result_t r = lb_run(&p);
        g_sw_x[i] = v;
        g_sw_y[i] = r.link_margin_dB;
    }
    InvalidateRect(h_sw_plot, NULL, TRUE); /* trigger repaint of the plot area */

    /* Find and report the zero-crossing (boundary where link just closes) */
    char info[120] = "No link closure in range.";
    for (int i = 0; i < npts-1; i++) {
        if (g_sw_y[i] >= 0.0 && g_sw_y[i+1] < 0.0) {
            /* Linear interpolation to find the exact zero crossing */
            double frac = g_sw_y[i] / (g_sw_y[i] - g_sw_y[i+1]);
            double xc   = g_sw_x[i] + frac * (g_sw_x[i+1] - g_sw_x[i]);
            snprintf(info, sizeof(info),
                "Link closes up to: %.4g %s  (margin = 0 dB)",
                xc, SW_PARAM_NAMES[param]);
            break;
        }
    }
    SetWindowTextA(h_sw_info, info);
}

/* Window procedure for the custom plot area child window */
static LRESULT CALLBACK SweepPlotProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_ERASEBKGND) {
        /* Fill background here to prevent flicker; return 1 to skip default erase */
        FillRect((HDC)wParam, &(RECT){0,0,9999,9999}, g_br_bg);
        return 1;
    }
    if (msg == WM_MOUSEMOVE) {
        RECT rc; GetClientRect(hwnd, &rc);
        /* Plot margins: left=52, top=14, right=14, bottom=32 pixels */
        int pmx = 52, pmy = 14, pmr = 14, pmb = 32;
        int ppw = rc.right - pmx - pmr;   /* plot area width in pixels */
        int pph = rc.bottom - pmy - pmb;  /* plot area height in pixels */
        /* Extract mouse coordinates from lParam using signed short cast to
         * handle negative coordinates correctly on multi-monitor setups */
        int cx  = (int)(short)LOWORD(lParam);
        int cy  = (int)(short)HIWORD(lParam);
        if (g_sw_n >= 2 && ppw > 0 && pph > 0 &&
            cx >= pmx && cx <= pmx + ppw &&
            cy >= pmy && cy <= pmy + pph) {
            double xmin  = g_sw_x[0], xmax = g_sw_x[g_sw_n - 1];
            double xspan = xmax - xmin; if (xspan <= 0) xspan = 1.0;
            /* Convert pixel position to data x-value */
            double xv    = xmin + (double)(cx - pmx) / ppw * xspan;
            if (xv < xmin) xv = xmin;
            if (xv > xmax) xv = xmax;
            /* Linearly interpolate the margin at xv between adjacent data points */
            double yv = g_sw_y[g_sw_n - 1];
            for (int i = 0; i < g_sw_n - 1; i++) {
                if (g_sw_x[i] <= xv && xv <= g_sw_x[i + 1]) {
                    double dx = g_sw_x[i + 1] - g_sw_x[i];
                    double t  = (dx > 0) ? (xv - g_sw_x[i]) / dx : 0.0;
                    yv = g_sw_y[i] + t * (g_sw_y[i + 1] - g_sw_y[i]);
                    break;
                }
            }
            g_sw_cx   = cx;
            g_sw_cv_x = xv;
            g_sw_cv_y = yv;
            /* Request WM_MOUSELEAVE notification so we can hide the crosshair
             * when the cursor exits the plot area */
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        } else {
            g_sw_cx = -1; /* cursor outside plot area — hide crosshair */
        }
        InvalidateRect(hwnd, NULL, FALSE); /* repaint without background erase */
        return 0;
    }
    if (msg == WM_MOUSELEAVE) {
        g_sw_cx = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    if (msg != WM_PAINT) return DefWindowProc(hwnd, msg, wParam, lParam);

    /* --- WM_PAINT: draw the sweep plot --- */
    PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, g_br_bg);

    if (g_sw_n < 2) { EndPaint(hwnd, &ps); return 0; }

    /* Plot area margins (pixels) */
    int mx = 52, my = 14, mr = 14, mb = 32;
    int pw = rc.right  - mx - mr;
    int ph = rc.bottom - my - mb;
    if (pw < 10 || ph < 10) { EndPaint(hwnd, &ps); return 0; }

    /* Compute y-axis range: always include 0 so the zero margin line is visible,
     * then pad by 8% on each side for breathing room */
    double ymin = g_sw_y[0], ymax = g_sw_y[0];
    for (int i = 1; i < g_sw_n; i++) {
        if (g_sw_y[i] < ymin) ymin = g_sw_y[i];
        if (g_sw_y[i] > ymax) ymax = g_sw_y[i];
    }
    if (ymax < 0) ymax = 0; /* keep zero in view even if all margins are negative */
    if (ymin > 0) ymin = 0; /* keep zero in view even if all margins are positive */
    double yspan = ymax - ymin; if (yspan < 1.0) yspan = 1.0;
    ymin -= yspan*0.08; ymax += yspan*0.08; yspan = ymax - ymin;

    double xmin = g_sw_x[0], xmax = g_sw_x[g_sw_n-1];
    double xspan = xmax - xmin; if (xspan <= 0) xspan = 1.0;

/* Map data coordinates to pixel coordinates */
#define PX(x) (mx + (int)((x-xmin)/xspan * pw))
#define PY(y) (my + ph - (int)((y-ymin)/yspan * ph))

    /* Horizontal grid lines (5 evenly spaced, including top and bottom) */
    HPEN gpen = CreatePen(PS_SOLID, 1, RGB(0x38,0x3E,0x4A));
    SelectObject(hdc, gpen);
    for (int i = 0; i <= 4; i++) {
        int gy = my + ph*i/4;
        MoveToEx(hdc, mx, gy, NULL); LineTo(hdc, mx+pw, gy);
    }
    DeleteObject(gpen);

    /* Dotted zero-margin reference line */
    int y0 = PY(0.0);
    HPEN zpen = CreatePen(PS_DOT, 1, RGB(0x80,0x80,0x80));
    SelectObject(hdc, zpen);
    MoveToEx(hdc, mx, y0, NULL); LineTo(hdc, mx+pw, y0);
    DeleteObject(zpen);

    /* Data curve: green segments where margin ≥ 0, red where < 0,
     * amber for segments that straddle the zero line */
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < g_sw_n-1; i++) {
        COLORREF col = (g_sw_y[i]>=0 && g_sw_y[i+1]>=0) ? C_POS :
                       (g_sw_y[i]< 0 && g_sw_y[i+1]< 0) ? C_NEG :
                       RGB(0xFF,0xD0,0x40); /* amber for zero-crossing segment */
        HPEN lpen = CreatePen(PS_SOLID, 2, col);
        HPEN op   = (HPEN)SelectObject(hdc, lpen);
        MoveToEx(hdc, PX(g_sw_x[i]),   PY(g_sw_y[i]),   NULL);
        LineTo  (hdc, PX(g_sw_x[i+1]), PY(g_sw_y[i+1]));
        SelectObject(hdc, op); DeleteObject(lpen);
    }

    /* Plot axes */
    HPEN apen = CreatePen(PS_SOLID, 1, C_LABEL);
    SelectObject(hdc, apen);
    MoveToEx(hdc, mx, my,    NULL); LineTo(hdc, mx,    my+ph); /* Y axis */
    MoveToEx(hdc, mx, my+ph, NULL); LineTo(hdc, mx+pw, my+ph); /* X axis */
    DeleteObject(apen);

    /* Axis tick labels */
    SetTextColor(hdc, C_LABEL);
    HFONT old = (HFONT)SelectObject(hdc, g_fnt_label);
    char lbl[32];
    for (int i = 0; i <= 4; i++) {
        double yv = ymin + yspan * (4-i) / 4.0; /* top label first (i=0) */
        snprintf(lbl, sizeof(lbl), "%.1f", yv);
        RECT lr = {0, my+ph*i/4-8, mx-4, my+ph*i/4+8};
        DrawTextA(hdc, lbl, -1, &lr, DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    }
    for (int i = 0; i <= 4; i++) {
        double xv = xmin + xspan * i / 4.0;
        snprintf(lbl, sizeof(lbl), "%.3g", xv);
        RECT lr = {PX(xv)-30, my+ph+2, PX(xv)+30, my+ph+18};
        DrawTextA(hdc, lbl, -1, &lr, DT_CENTER|DT_TOP|DT_SINGLELINE);
    }
    /* Vertical Y-axis title */
    snprintf(lbl, sizeof(lbl), "Margin (dB)");
    RECT yr = {2, my, mx-6, my+ph};
    DrawTextA(hdc, lbl, -1, &yr, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_WORDBREAK);
    SelectObject(hdc, old);

    /* Crosshair: vertical dotted line with a data-readout label */
    if (g_sw_cx >= mx && g_sw_cx <= mx + pw) {
        HPEN cpen = CreatePen(PS_DOT, 1, RGB(0xFF, 0xFF, 0x80));
        HPEN op2  = (HPEN)SelectObject(hdc, cpen);
        MoveToEx(hdc, g_sw_cx, my,      NULL);
        LineTo  (hdc, g_sw_cx, my + ph);
        SelectObject(hdc, op2); DeleteObject(cpen);

        char clbl[80];
        snprintf(clbl, sizeof(clbl), "X: %.4g   Margin: %.2f dB",
                 g_sw_cv_x, g_sw_cv_y);
        SelectObject(hdc, g_fnt_label);
        SetTextColor(hdc, RGB(0xFF, 0xFF, 0x80));
        /* OPAQUE background to make the label readable over the plot */
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(0x22, 0x28, 0x30));
        RECT lr = { mx + 6, my + 3, mx + pw - 4, my + 17 };
        DrawTextA(hdc, clbl, -1, &lr, DT_LEFT | DT_TOP | DT_SINGLELINE);
        SetBkMode(hdc, TRANSPARENT);
    }

#undef PX
#undef PY
    EndPaint(hwnd, &ps);
    return 0;
}

static LRESULT CALLBACK SweepWndProc(HWND hwnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        int cw = 760, cy = 10, rh = 24;
        CreateWindowA("STATIC", "Parameter:", WS_CHILD|WS_VISIBLE|SS_LEFT,
                      8, cy+4, 70, 18, hwnd, (HMENU)0, g_hinst, NULL);
        /* CBS_DROPDOWNLIST prevents user from typing into the combo box */
        h_sw_combo = CreateWindowA("COMBOBOX", "",
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
            82, cy, 180, 200, hwnd, (HMENU)0, g_hinst, NULL);
        for (int i = 0; i < SW_PARAM_COUNT; i++)
            SendMessageA(h_sw_combo, CB_ADDSTRING, 0, (LPARAM)SW_PARAM_NAMES[i]);
        SendMessage(h_sw_combo, CB_SETCURSEL, 0, 0);
        CreateWindowA("STATIC","Min:",WS_CHILD|WS_VISIBLE|SS_LEFT,
                      268,cy+4,28,18,hwnd,(HMENU)0,g_hinst,NULL);
        h_sw_min = CreateWindowA("EDIT","1",WS_CHILD|WS_VISIBLE|WS_BORDER,
                                  298,cy,70,rh,hwnd,(HMENU)0,g_hinst,NULL);
        CreateWindowA("STATIC","Max:",WS_CHILD|WS_VISIBLE|SS_LEFT,
                      374,cy+4,28,18,hwnd,(HMENU)0,g_hinst,NULL);
        h_sw_max = CreateWindowA("EDIT","100",WS_CHILD|WS_VISIBLE|WS_BORDER,
                                  404,cy,70,rh,hwnd,(HMENU)0,g_hinst,NULL);
        CreateWindowA("STATIC","Pts:",WS_CHILD|WS_VISIBLE|SS_LEFT,
                      480,cy+4,28,18,hwnd,(HMENU)0,g_hinst,NULL);
        h_sw_steps = CreateWindowA("EDIT","100",WS_CHILD|WS_VISIBLE|WS_BORDER,
                                    510,cy,44,rh,hwnd,(HMENU)0,g_hinst,NULL);
        /* "Run Sweep" button sends WM_COMMAND with ID=10 when clicked */
        HWND hrun = CreateWindowA("BUTTON","Run Sweep",
                                   WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                                   560,cy,90,rh,hwnd,(HMENU)10,g_hinst,NULL);
        (void)hrun;
        h_sw_info = CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE|SS_LEFT,
                                   8,42,cw-16,18,hwnd,(HMENU)0,g_hinst,NULL);
        /* Register the custom plot window class locally — only needed once */
        WNDCLASSA wcp = {0};
        wcp.lpfnWndProc   = SweepPlotProc;
        wcp.hInstance     = g_hinst;
        wcp.lpszClassName = "SweepPlotClass";
        RegisterClassA(&wcp);
        h_sw_plot = CreateWindowA("SweepPlotClass","",WS_CHILD|WS_VISIBLE,
                                   0,64,cw,430,hwnd,(HMENU)0,g_hinst,NULL);
        /* Apply app fonts to all controls so they match the main window */
        SendMessage(h_sw_combo, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
        SendMessage(h_sw_min,   WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
        SendMessage(h_sw_max,   WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
        SendMessage(h_sw_steps, WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
        SendMessage(h_sw_info,  WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
        return 0;
    }
    case WM_COMMAND:
        /* ID=10 is the "Run Sweep" button */
        if (LOWORD(wParam) == 10 && HIWORD(wParam) == BN_CLICKED) sweep_run();
        return 0;
    case WM_ERASEBKGND:
        FillRect((HDC)wParam, &(RECT){0,0,9999,9999}, g_br_bg);
        return 1;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        /* Return our brush so child controls inherit the dark background */
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc,   C_BG);
        SetTextColor(hdc, C_LABEL);
        return (LRESULT)g_br_bg;
    }
    case WM_DESTROY:
        g_sw_wnd = NULL; /* allow a new sweep window to be created next time */
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void open_sweep_wnd(void)
{
    /* Bring existing sweep window to the front rather than opening a second one */
    if (g_sw_wnd) { SetForegroundWindow(g_sw_wnd); return; }
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = SweepWndProc;
    wc.hInstance     = g_hinst;
    wc.lpszClassName = "SweepWndClass";
    wc.hbrBackground = NULL; /* we paint the background ourselves in WM_ERASEBKGND */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    /* Fixed-size overlay window (no resize/maximize) */
    g_sw_wnd = CreateWindowA("SweepWndClass","Sweep Analysis",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 776, 534,
        g_hwnd, NULL, g_hinst, NULL);
}

/* ============================================================
 * Tooltips
 * ============================================================ */

/* Register a single tooltip string for the given tool HWND.
 * TTF_IDISHWND: uId is a HWND, not a control ID.
 * TTF_SUBCLASS: tooltip subclasses the tool so it can watch mouse messages. */
static void add_tip(HWND tt, HWND tool, const char *text)
{
    TOOLINFOA ti = {0};
    ti.cbSize   = sizeof(ti);
    ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd     = GetParent(tool);
    ti.uId      = (UINT_PTR)tool;
    ti.lpszText = (char*)text;
    SendMessageA(tt, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

/* Create a balloon-style tooltip window attached to a given parent */
static HWND make_tooltip(HWND parent)
{
    HWND tt = CreateWindowExA(0, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        parent, NULL, g_hinst, NULL);
    /* Wrap long tooltips at 260 px rather than one very long line */
    SendMessage(tt, TTM_SETMAXTIPWIDTH, 0, 260);
    return tt;
}

static void create_tooltips(void)
{
    /* One tooltip control per section panel; each tt is a child of that panel
     * so TTF_IDISHWND resolution finds the correct parent HWND. */
    HWND tt1 = make_tooltip(h_sec[0]);
    add_tip(tt1, h_freq,     "Carrier frequency. Click unit label to cycle GHz/MHz/kHz.");
    add_tip(tt1, h_datarate, "Information bit rate (Mb/s).");

    HWND tt2 = make_tooltip(h_sec[1]);
    add_tip(tt2, h_fec,      "Forward Error Correction scheme. Sets required Eb/N0.");
    add_tip(tt2, h_mod,      "Modulation order. Higher order = more bits/symbol but needs more Eb/N0.");
    add_tip(tt2, h_ber,      "Target Bit Error Rate.");
    add_tip(tt2, h_imploss,  "Implementation loss: accounts for real hardware vs. ideal (dB).");

    HWND tt3 = make_tooltip(h_sec[2]);
    add_tip(tt3, h_txalt,     "Transmitter altitude. Click unit label to cycle m/ft/km/NM.");
    add_tip(tt3, h_rxalt,     "Receiver altitude. Click unit label to cycle m/ft/km/NM.");
    add_tip(tt3, h_horiz_dist,"Horizontal ground distance between TX and RX (km).");
    add_tip(tt3, h_slant,     "Direct slant-range distance between TX and RX (km).");
    add_tip(tt3, h_radius,    "Earth radius factor for geometry. 1.333 = 4/3 effective Earth.");

    HWND tt4 = make_tooltip(h_sec[3]);
    add_tip(tt4, h_satpow,  "Transmitter saturated output power. Click unit to toggle W/dBm.");
    add_tip(tt4, h_obo,     "Output Back-Off from saturation point (dB). Reduces IM distortion.");
    add_tip(tt4, h_feedtx,  "TX feed-line / waveguide losses between PA and antenna (dB).");

    HWND tt5 = make_tooltip(h_sec[4]);
    add_tip(tt5, h_txgain,   "TX antenna gain (dBi). For rod/omni antennas use ~0-2 dBi.");
    add_tip(tt5, h_ar_tx,    "TX antenna axial ratio (dB). 0 = perfect circular polarisation.");
    add_tip(tt5, h_ar_wave,  "Wave axial ratio after propagation (dB).");
    add_tip(tt5, h_polangle, "Polarisation tilt angle between TX and RX (degrees).");

    HWND tt6 = make_tooltip(h_sec[5]);
    add_tip(tt6, h_absloss,   "Absorptive losses: gaseous, rain, foliage etc. (dB).");
    add_tip(tt6, h_nonabsloss,"Non-absorptive losses: multipath, scintillation etc. (dB).");
    add_tip(tt6, h_rainrate,  "Rain rate mm/h for ITU-R P.838 rain attenuation model.");

    HWND tt7 = make_tooltip(h_sec[6]);
    add_tip(tt7, h_rxgain_direct,"RX antenna gain (dBi). For rod/omni use ~0 dBi + attitude margin.");
    add_tip(tt7, h_trackloss,    "Antenna tracking/pointing loss (dB).");
    add_tip(tt7, h_diameter,     "RX parabolic dish diameter (m).");
    add_tip(tt7, h_efficiency,   "Dish aperture efficiency (%). Typical: 55-65%.");

    HWND tt8 = make_tooltip(h_sec[7]);
    add_tip(tt8, h_rxnoise, "Rx antenna noise temperature in dBW/Hz (= kT in dBW/Hz).\n-203.98 dBW/Hz = 290 K (room temp).");
    add_tip(tt8, h_feedrx,  "RX feed-line loss (dB). Increases effective noise temperature.");
    add_tip(tt8, h_nf,      "LNA/receiver noise figure (dB).");
}

/* ============================================================
 * Main window — paint / drawitem helpers
 * ============================================================ */

/* Paint the top banner: dark header strip with title text and GDI+ logo.
 * Called from WM_PAINT; BeginPaint/EndPaint pair validates the update region. */
static void on_paint_banner(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rb = {0, 0, WIN_W, BANNER_H};
    FillRect(hdc, &rb, g_br_hdr);
    SetBkMode(hdc, TRANSPARENT); /* text background is transparent over the header brush */

    HFONT old = (HFONT)SelectObject(hdc, g_fnt_hdr);
    SetTextColor(hdc, C_HDR_FG);
    RECT rt = {10, 0, 500, BANNER_H};
    DrawText(hdc, "  " APP_NAME "  v" VERSION_STRING, -1, &rt,
             DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc, old);

    EndPaint(hwnd, &ps);
}

/* Owner-draw handler for all BS_OWNERDRAW buttons.
 * Windows sends WM_DRAWITEM whenever a button needs repainting;
 * the caller returns the BOOL result to the message loop. */
static BOOL on_drawitem_btn(LPARAM lParam)
{
    DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
    HDC  hdc = dis->hDC;
    RECT rc  = dis->rcItem;
    BOOL sel = (dis->itemState & ODS_SELECTED) != 0; /* true while mouse button held */
    const char *txt = "";
    COLORREF bg = RGB(0x35,0x3D,0x4A), fg = C_HDR_FG;
    HFONT fnt = g_fnt_label;

    switch (dis->CtlID) {
    case ID_BTN:
        bg  = sel ? C_BTN_HOT : C_BTN_BG; /* highlight when pressed */
        fg  = C_BTN_FG;
        fnt = g_fnt_hdr;
        txt = "  CALCULATE LINK BUDGET  ";
        break;
    case ID_BTN_SAVE:   txt = "Save";       goto util_btn;
    case ID_BTN_LOAD:   txt = "Load";       goto util_btn;
    case ID_BTN_SWEEP:  txt = "Sweep";      goto util_btn;
    case ID_BTN_EXPORT: txt = "Export";     goto util_btn;
    case ID_BTN_PRINT:  txt = "PDF Report"; goto util_btn;
    case ID_BTN_ABOUT:  txt = "?";
    util_btn: /* shared styling for all utility buttons */
        bg  = sel ? RGB(0x4A,0x5A,0x6A) : RGB(0x35,0x3D,0x4A);
        break;
    default: break;
    }

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(hdc, &rc, br);
    DeleteObject(br);

    /* Draw a 1-pixel border using a null brush so only the outline is painted */
    HPEN pen = CreatePen(PS_SOLID, 1, C_BORDER);
    HPEN op  = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, op);
    DeleteObject(pen);

    SetTextColor(hdc, fg);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, fnt);
    DrawText(hdc, txt, -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc, old);
    return TRUE;
}

/* ============================================================
 * About dialog
 * ============================================================ */
static void show_about(void)
{
    char msg[512];
    snprintf(msg, sizeof(msg),
        APP_NAME "\n"
        "Version " VERSION_STRING "\n"
        "Built " __DATE__ "\n"
        "\n"
        "RF/Satellite Link Budget Calculator\n"
        "\n"
        "Physics models:\n"
        "  \x95 ITU-R P.676-12 (Gaseous attenuation)\n"
        "  \x95 ITU-R P.838-3  (Rain attenuation)\n"
        "\n"
        "Keyboard shortcuts:\n"
        "  F5        Recalculate\n"
        "  Ctrl+S    Save scenario\n"
        "  Ctrl+O    Load scenario\n"
        "  Ctrl+E    Export CSV\n"
        "  Ctrl+P    PDF Report\n"
        "  Ctrl+W    Sweep plot"
    );
    MessageBoxA(g_hwnd, msg, "About " APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* ============================================================
 * Main window procedure
 * ============================================================ */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                  WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE:
        g_hwnd = hwnd;
        create_fonts_brushes();
        /* Build each of the 8 section panels and the results/button areas */
        build_sec1(); build_sec2(); build_sec3(); build_sec4();
        build_sec5(); build_sec6(); build_sec7(); build_sec8();
        build_button();
        build_results();
        g_ui_ready = TRUE; /* allow EN_CHANGE / CBN_SELCHANGE to trigger recalculation */
        update_visibility();
        create_tooltips();
        do_calculate(); /* populate all calc displays with default values on startup */
        return 0;

    case WM_ERASEBKGND: {
        /* Paint the main window background ourselves to prevent white flash */
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_br_bg);
        return 1; /* non-zero return skips the default erase */
    }

    case WM_PAINT:
        on_paint_banner(hwnd); /* only the banner needs custom painting */
        return 0;

    case WM_DRAWITEM:
        /* All owner-draw buttons route through the single helper */
        return on_drawitem_btn(lParam);

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN:        if (HIWORD(wParam)==BN_CLICKED) do_calculate();   break;
        case ID_BTN_SAVE:   if (HIWORD(wParam)==BN_CLICKED) save_scenario();  break;
        case ID_BTN_LOAD:   if (HIWORD(wParam)==BN_CLICKED) load_scenario();  break;
        case ID_BTN_SWEEP:  if (HIWORD(wParam)==BN_CLICKED) open_sweep_wnd(); break;
        case ID_BTN_EXPORT: if (HIWORD(wParam)==BN_CLICKED) export_csv();     break;
        case ID_BTN_PRINT:  if (HIWORD(wParam)==BN_CLICKED) print_report();   break;
        case ID_BTN_ABOUT:  if (HIWORD(wParam)==BN_CLICKED) show_about();    break;
        }
        return 0;

    case WM_KEYDOWN: {
        /* Global keyboard shortcuts — active whenever the main window has focus */
        BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == VK_F5)                  { do_calculate();   return 0; }
        if (ctrl && wParam == 'S')            { save_scenario();  return 0; }
        if (ctrl && wParam == 'O')            { load_scenario();  return 0; }
        if (ctrl && wParam == 'E')            { export_csv();     return 0; }
        if (ctrl && wParam == 'P')            { print_report();   return 0; }
        if (ctrl && wParam == 'W')            { open_sweep_wnd(); return 0; }
        break;
    }

    case WM_USER+1:
        /* Posted by SectionPanelProc whenever any input changes (EN_CHANGE,
         * CBN_SELCHANGE, or a unit click).  Using PostMessage instead of
         * SendMessage breaks any reentrancy risk during combo dropdown. */
        if (g_ui_ready) {
            update_visibility();
            do_calculate();
        }
        return 0;

    case WM_DESTROY:
        delete_fonts_brushes();
        PostQuitMessage(0); /* exit the message loop with wParam = 0 */
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* ============================================================
 * WinMain
 * ============================================================ */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nCmdShow)
{
    (void)hPrev; (void)lpCmd; /* unused in a single-instance Win32 app */
    g_hinst = hInst;

    /* Register section panel class */
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = SectionPanelProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "SectionPanelClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    /* Register results panel class */
    wc.lpfnWndProc   = ResultsPanelProc;
    wc.lpszClassName = "ResultsPanelClass";
    RegisterClass(&wc);

    /* Register main window class (no background brush — we handle WM_ERASEBKGND) */
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = "LinkBudgetMain";
    wc.hbrBackground = NULL;
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(1));
    RegisterClass(&wc);

    /* AdjustWindowRect inflates the desired client rect to account for the
     * title bar, borders, etc., giving us the correct CreateWindow dimensions. */
    DWORD style = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT rc = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&rc, style, FALSE);

    int win_w = rc.right  - rc.left;
    int win_h = rc.bottom - rc.top;

    /* Center on primary monitor */
    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (scr_w - win_w) / 2;  if (pos_x < 0) pos_x = 0;
    int pos_y = (scr_h - win_h) / 2;  if (pos_y < 0) pos_y = 0;

    g_hwnd = CreateWindow(
        "LinkBudgetMain", APP_TITLE,
        style,
        pos_x, pos_y, win_w, win_h,
        NULL, NULL, hInst, NULL);

    /* On some DPI configurations AdjustWindowRect can be slightly off;
     * correct the window size so the client area is exactly WIN_W × WIN_H. */
    {
        RECT cr;
        GetClientRect(g_hwnd, &cr);
        if (cr.right != WIN_W || cr.bottom != WIN_H) {
            RECT wr2;
            GetWindowRect(g_hwnd, &wr2);
            int extra_w = (wr2.right  - wr2.left) - cr.right;
            int extra_h = (wr2.bottom - wr2.top)  - cr.bottom;
            int new_w = WIN_W + extra_w;
            int new_h = WIN_H + extra_h;
            int new_x = (scr_w - new_w) / 2;  if (new_x < 0) new_x = 0;
            int new_y = (scr_h - new_h) / 2;  if (new_y < 0) new_y = 0;
            /* SWP_NOZORDER | SWP_NOACTIVATE: resize only, no Z-order or focus change */
            SetWindowPos(g_hwnd, NULL, new_x, new_y, new_w, new_h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }


    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd); /* force an immediate WM_PAINT before entering the loop */

    /* Standard Win32 message loop */
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg); /* convert WM_KEYDOWN to WM_CHAR where applicable */
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
