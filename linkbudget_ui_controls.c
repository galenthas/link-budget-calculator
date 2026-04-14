/*
 * linkbudget_ui_controls.c — Control creation and section panel procedures.
 * Defines mk_*() creators (label, edit, combo, calc display, unit link),
 * row-builder helpers, SectionPanelProc, ResultsPanelProc, make_sec(),
 * build_sec1-8(), build_results(), and build_button().
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string.h>

#include "linkbudget_gui_config.h"
#include "linkbudget_core.h"
#include "linkbudget_ui_controls.h"
#include "linkbudget_ui_logic.h"   /* cycle_unit, update_visibility, do_calculate */

/* ============================================================
 * Additional globals from linkbudget_gui.c used here
 * ============================================================ */
extern BOOL      g_ui_ready;
extern COLORREF  g_margin_color;

/* Input HWNDs */
extern HWND h_freq, h_datarate;
extern HWND h_fec, h_mod, h_ber, h_imploss;
extern HWND h_txalt, h_rxalt, h_slant, h_radius;
extern HWND h_satpow, h_obo, h_feedtx;
extern HWND h_txgain, h_ar_tx, h_ar_wave, h_polangle;
extern HWND h_absloss, h_nonabsloss;
extern HWND h_diameter, h_efficiency, h_trackloss;
extern HWND h_rxnoise, h_feedrx, h_nf;

/* Calc display HWNDs */
extern HWND h_cv_reqebn0;
extern HWND h_cv_txel, h_cv_rxel, h_cv_central, h_cv_minalt;
extern HWND h_cv_feedpow;
extern HWND h_cv_polloss, h_cv_eirp;
extern HWND h_cv_fspl, h_cv_totloss;
extern HWND h_cv_rxgain, h_cv_beamwid, h_cv_rxflux, h_cv_rxpow;
extern HWND h_cv_tsys, h_cv_gt, h_cv_cn0;

/* Result display HWNDs */
extern HWND h_rv_req, h_rv_ebn0, h_rv_margin, h_rv_status;

/* Range mode (Sec 3) */
extern HWND h_range_mode;
extern HWND h_horiz_dist;
extern HWND h_cv_slant_calc;
extern HWND h_cv_slant_lbl, h_cv_slant_unit;
extern HWND h_cv_minalt_lbl, h_cv_minalt_unit;

/* Antenna mode selectors */
extern HWND h_tx_mode, h_rx_mode;
extern HWND h_tx_diameter, h_tx_efficiency;
extern HWND h_tx_lbl_gain, h_tx_unit_gain;
extern HWND h_tx_lbl_diam, h_tx_unit_diam;
extern HWND h_tx_lbl_eff,  h_tx_unit_eff;
extern HWND h_rxgain_direct;
extern HWND h_rx_lbl_gain, h_rx_unit_gain;
extern HWND h_rx_lbl_diam, h_rx_unit_diam;
extern HWND h_rx_lbl_eff,  h_rx_unit_eff;
extern HWND h_cv_beamwid_lbl, h_cv_beamwid_unit;

/* Clickable unit HWNDs */
extern HWND h_unit_freq, h_unit_txalt, h_unit_rxalt, h_unit_satpow;
extern HWND h_unit_feedpow, h_unit_rxpow, h_unit_minalt, h_unit_eirp;

/* Sec 6 — path loss model + ITU-R fields */
extern HWND h_loss_mode;
extern HWND h_rainrate;
extern HWND h_rainrate_lbl, h_rainrate_unit;
extern HWND h_cv_gasatt, h_cv_rainatt;
extern HWND h_cv_gasatt_lbl, h_cv_gasatt_unit;
extern HWND h_cv_rainatt_lbl, h_cv_rainatt_unit;

/* Utility button HWNDs */
extern HWND h_btn_save, h_btn_load, h_btn_sweep, h_btn_export, h_btn_print;

/* String name tables (defined in linkbudget_gui.c) */
extern const char *FEC_NAMES[];
extern const char *MOD_NAMES[];
extern const char *BER_NAMES[];
extern const char *ANT_MODE_NAMES[];
extern const char *RANGE_MODE_NAMES[];
extern const char *LOSS_MODE_NAMES[];
extern const char *UNIT_FREQ_NAMES[];
extern const char *UNIT_EIRP_NAMES[];
extern const char *UNIT_FEEDPOW_NAMES[];
extern const char *UNIT_RXPOW_NAMES[];
extern const char *UNIT_MINALT_NAMES[];

/* ============================================================
 * Low-level control creators
 * Each mk_* function creates a single Win32 child control,
 * applies the correct app font, and returns the HWND.
 * ============================================================ */

/* Static text label — left-aligned, not clickable */
HWND mk_label(HWND p, const char *t, int x, int y, int w, int h)
{
    HWND hw = CreateWindow("STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, h, p, (HMENU)(INT_PTR)ID_LBL, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

/* Static unit label (non-clickable) — used alongside fixed-unit fields */
HWND mk_unit(HWND p, const char *t, int x, int y, int w)
{
    HWND hw = CreateWindow("STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, p, (HMENU)(INT_PTR)ID_UNIT, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

/* Single-line edit control with ES_AUTOHSCROLL so text scrolls rather than wrapping */
HWND mk_edit(HWND p, const char *def, int x, int y, int w, int h)
{
    HWND hw = CreateWindow("EDIT", def,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, h, p, (HMENU)(INT_PTR)0, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
    return hw;
}

/* Drop-down combo box — CBS_DROPDOWNLIST prevents manual text entry.
 * Height=240 gives the drop-down list enough room to show ~10 items. */
HWND mk_combo(HWND p, const char **items, int n, int x, int y, int w)
{
    HWND hw = CreateWindow("COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, w, 240, p, (HMENU)(INT_PTR)0, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
    for (int i = 0; i < n; i++)
        SendMessage(hw, CB_ADDSTRING, 0, (LPARAM)items[i]);
    return hw;
}

/* Read-only calculated-value display — right-aligned STATIC with ID_CALC
 * so WM_CTLCOLORSTATIC can colour it differently from regular labels */
HWND mk_calc(HWND p, int x, int y, int id)
{
    HWND hw = CreateWindow("STATIC", "---",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        x, y, SP_EDIT_W, 16, p, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_entry, FALSE);
    return hw;
}

/* Clickable unit label — SS_NOTIFY makes the STATIC send STN_CLICKED to
 * its parent via WM_COMMAND, enabling the click-to-cycle-unit behaviour */
HWND mk_unit_link(HWND p, const char *t, int x, int y, int w, int id)
{
    HWND hw = CreateWindow("STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
        x, y, w, 18, p, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

/* ============================================================
 * Row builders (return next y)
 * Each helper lays out a standard label + input + unit row and
 * advances the y coordinate by the appropriate row height.
 * ============================================================ */

/* Input row: label | edit field | static unit string */
static int field_row(HWND sec, int y, const char *lbl, const char *def,
                     const char *unit, HWND *out)
{
    mk_label(sec, lbl,  SP_LBL_X,  y+3, SP_LBL_W, 18);
    *out = mk_edit(sec, def, SP_EDIT_X, y, SP_EDIT_W, 22);
    mk_unit(sec,  unit, SP_UNIT_X, y+3, SP_UNIT_W+8);
    return y + ROW_H_IN;
}

/* Combo row: label | combo box (spanning edit + unit columns) */
static int combo_row(HWND sec, int y, const char *lbl,
                     const char **items, int n, HWND *out)
{
    mk_label(sec, lbl, SP_LBL_X, y+3, SP_LBL_W, 18);
    *out = mk_combo(sec, items, n, SP_EDIT_X, y, SP_EDIT_W + SP_UNIT_W + 2);
    return y + ROW_H_IN;
}

/* Calculated-value row: label | read-only value | static unit
 * Uses ROW_H_CALC (shorter than ROW_H_IN) for tighter spacing */
static int calc_row(HWND sec, int y, const char *lbl,
                    const char *unit, HWND *out)
{
    mk_label(sec, lbl,  SP_LBL_X,  y+1, SP_LBL_W, 16);
    *out = mk_calc(sec, SP_EDIT_X, y+1, ID_CALC);
    mk_unit(sec,  unit, SP_UNIT_X, y+1, SP_UNIT_W+12);
    return y + ROW_H_CALC;
}

/* ============================================================
 * Section panel window procedure
 * Handles painting (header + optional separator line) and routes
 * all child-control notifications up to the main window.
 * ============================================================ */
LRESULT CALLBACK SectionPanelProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_CREATE: {
        /* Store the SecData pointer passed in lpCreateParams so WM_PAINT
         * can read the section title and separator position. */
        CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }

    case WM_ERASEBKGND: {
        /* Paint the panel background before child controls are drawn,
         * preventing the default white flash. */
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_br_panel);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SecData *sd = (SecData *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!sd) { EndPaint(hwnd, &ps); return 0; }
        RECT rc; GetClientRect(hwnd, &rc);

        /* Draw the section header strip */
        RECT rh = {0, 0, rc.right, HDR_H};
        FillRect(hdc, &rh, g_br_hdr);
        SetTextColor(hdc, C_HDR_FG);
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, g_fnt_hdr);
        RECT rt = {8, 0, rc.right-4, HDR_H};
        DrawText(hdc, sd->title, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);

        /* Optional 1-pixel separator between input fields and calc displays.
         * sep_y == -1 means no separator for this section. */
        if (sd->sep_y > 0) {
            HBRUSH br = CreateSolidBrush(C_BORDER);
            RECT rs = {4, sd->sep_y, rc.right-4, sd->sep_y+1};
            FillRect(hdc, &rs, br);
            DeleteObject(br);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        /* Return the entry background brush so edit boxes match the dark theme */
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, C_ENTRY_FG);
        SetBkColor(hdc,   C_ENTRY_BG);
        return (LRESULT)g_br_entry;
    }

    case WM_CTLCOLORLISTBOX: {
        /* Combo box drop-down list background */
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, C_ENTRY_FG);
        SetBkColor(hdc,   C_PANEL);
        return (LRESULT)g_br_panel;
    }

    case WM_CTLCOLORSTATIC: {
        /* Colour static controls by their role (identified by control ID) */
        HDC hdc = (HDC)wParam;
        int id  = GetDlgCtrlID((HWND)lParam);
        SetBkMode(hdc, TRANSPARENT); /* draw text over the panel background */
        if      (id >= ID_UNIT_FREQ && id <= ID_UNIT_EIRP)
                                    SetTextColor(hdc, C_HDR_FG); /* clickable unit links: bright */
        else if (id == ID_UNIT)     SetTextColor(hdc, C_UNIT);   /* fixed unit labels: muted */
        else if (id == ID_CALC)     SetTextColor(hdc, C_CALC_FG);/* calculated values: distinct */
        else                        SetTextColor(hdc, C_LABEL);  /* regular labels */
        return (LRESULT)g_br_panel;
    }

    case WM_SETCURSOR: {
        /* Show a hand cursor only when hovering over a clickable unit label */
        HWND hov = (HWND)wParam;
        if (hov == h_unit_freq    || hov == h_unit_txalt  ||
            hov == h_unit_rxalt   || hov == h_unit_satpow ||
            hov == h_unit_feedpow || hov == h_unit_rxpow  ||
            hov == h_unit_minalt  || hov == h_unit_eirp) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE; /* returning TRUE suppresses the default cursor logic */
        }
        break;
    }

    case WM_COMMAND: {
        int notify = (int)HIWORD(wParam);
        int id     = (int)LOWORD(wParam);
        if (g_ui_ready) {
            if (notify == STN_CLICKED &&
                id >= ID_UNIT_FREQ && id <= ID_UNIT_EIRP) {
                /* Clickable unit label clicked: advance its unit state and
                 * post WM_USER+1 to the main window so do_calculate() runs
                 * after this handler returns (avoids reentrancy). */
                cycle_unit(id);
                PostMessage(g_hwnd, WM_USER+1, 0, 0);
            } else if (notify == EN_CHANGE || notify == CBN_SELCHANGE) {
                /* Any input change triggers a full recalculation */
                PostMessage(g_hwnd, WM_USER+1, 0, 0);
            }
        }
        return 0;
    }

    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* ============================================================
 * Results panel window procedure
 * Simpler than SectionPanelProc: paints header and colours
 * the margin / status controls dynamically via g_margin_color.
 * ============================================================ */
LRESULT CALLBACK ResultsPanelProc(HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_br_res);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        RECT rh = {0, 0, rc.right, HDR_H};
        FillRect(hdc, &rh, g_br_hdr);
        SetTextColor(hdc, C_RES_LABEL);
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, g_fnt_hdr);
        RECT rt = {8, 0, rc.right-4, HDR_H};
        DrawText(hdc, "  LINK BUDGET RESULTS", -1, &rt,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        int id  = GetDlgCtrlID((HWND)lParam);
        SetBkMode(hdc, TRANSPARENT);
        if      (id == ID_MVAL || id == ID_MSTAT)
            /* Link margin and status text use a dynamic colour that changes
             * between C_POS (green) and C_NEG (red) after each calculation */
            SetTextColor(hdc, g_margin_color);
        else if (id == ID_RVAL)
            SetTextColor(hdc, C_CALC_FG); /* Eb/N0 required / available values */
        else   /* ID_RLBL */
            SetTextColor(hdc, C_RES_LABEL);
        return (LRESULT)g_br_res;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

/* ============================================================
 * Section panel helper
 * ============================================================ */

/* Create a section panel window and register it in the global arrays.
 * sep_y is the y-pixel position of the horizontal separator line, or -1
 * for sections that have no separator (pure input sections). */
HWND make_sec(int idx, int x, int y, int w, int h,
              const char *title, int sep_y)
{
    strncpy(g_sd[idx].title, title, sizeof(g_sd[idx].title) - 1);
    g_sd[idx].sep_y = sep_y;
    /* Pass &g_sd[idx] as the create-params so WM_CREATE can store it
     * in GWLP_USERDATA for use during WM_PAINT. */
    HWND hw = CreateWindow("SectionPanelClass", "",
        WS_CHILD | WS_VISIBLE,
        x, y, w, h, g_hwnd, NULL, g_hinst, &g_sd[idx]);
    h_sec[idx] = hw;
    return hw;
}

/* ============================================================
 * Section builders
 * Each build_secN() creates one section panel and populates it
 * with the appropriate input fields and calculated-value displays.
 * ============================================================ */

void build_sec1(void)  /* Frequencies */
{
    HWND s = make_sec(0, COL_X0, GRID_Y, COL_W, ROW0_H,
                      "1 | Frequencies", -1); /* no separator — all rows are inputs */
    int y = HDR_H + SP_PAD_TOP;
    mk_label(s, "Frequency", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_freq      = mk_edit(s, "4675", SP_EDIT_X, y, SP_EDIT_W, 22);
    /* mk_unit_link creates the clickable unit label (STN_CLICKED → cycle_unit) */
    h_unit_freq = mk_unit_link(s, "MHz", SP_UNIT_X, y+3, SP_UNIT_W+8, ID_UNIT_FREQ);
    y += ROW_H_IN;
    y = field_row(s, y, "Data Rate",  "10.0", "Mb/s", &h_datarate);
    (void)y;
}

void build_sec2(void)  /* Modulation / Coding */
{
    /* sep_y positions the divider between the 4 input rows and the calc row below */
    int sep = HDR_H + SP_PAD_TOP + 4*ROW_H_IN;
    HWND s = make_sec(1, COL_X1, GRID_Y, COL_W, ROW0_H,
                      "2 | Modulation / Coding", sep);
    int y = HDR_H + SP_PAD_TOP;
    y = combo_row(s, y, "FEC Decoder",  FEC_NAMES, LB_FEC_COUNT, &h_fec);
    y = combo_row(s, y, "Modulation",   MOD_NAMES, LB_MOD_COUNT,  &h_mod);
    y = combo_row(s, y, "BER",          BER_NAMES, LB_BER_COUNT,  &h_ber);
    y = field_row(s, y, "Impl. Loss",   "0.0", "dB",  &h_imploss);
    y += 6; /* small gap before the calc display area */
    y = calc_row(s,  y, "Req. Eb/N\xB0", "dB", &h_cv_reqebn0); /* \xB0 = degree symbol */
    (void)y;

    /* Default selections: VA 1/2 K=7, QPSK, BER 1e-6 */
    SendMessage(h_fec, CB_SETCURSEL, LB_FEC_VA_HALF_K7, 0);
    SendMessage(h_mod, CB_SETCURSEL, LB_MOD_QPSK,       0);
    SendMessage(h_ber, CB_SETCURSEL, LB_BER_1E6,         0);
}

void build_sec3(void)  /* Geometry */
{
    int sep = HDR_H + SP_PAD_TOP + 4*ROW_H_IN;
    HWND s = make_sec(2, COL_X2, GRID_Y, COL_W, ROW0_H,
                      "3 | Geometry", sep);
    int y = HDR_H + SP_PAD_TOP;
    mk_label(s, "Tx Altitude", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_txalt      = mk_edit(s, "10000", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_unit_txalt = mk_unit_link(s, "ft", SP_UNIT_X, y+3, SP_UNIT_W+8, ID_UNIT_TXALT);
    y += ROW_H_IN;
    mk_label(s, "Rx Altitude", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_rxalt      = mk_edit(s, "0", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_unit_rxalt = mk_unit_link(s, "ft", SP_UNIT_X, y+3, SP_UNIT_W+8, ID_UNIT_RXALT);
    y += ROW_H_IN;

    /* h_slant and h_horiz_dist occupy the same screen position;
     * update_range_visibility() shows only the relevant one. */
    h_range_mode = mk_combo(s, RANGE_MODE_NAMES, 2,
                             SP_LBL_X, y, SP_LBL_W);
    h_slant      = mk_edit(s, "200.0", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_horiz_dist = mk_edit(s, "1",     SP_EDIT_X, y, SP_EDIT_W, 22);
    mk_unit(s, "km", SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    y = field_row(s, y, "Radius Factor", "1.333", "", &h_radius);
    /* 1.333 = 4/3 effective Earth radius for standard refraction (ITU-R P.526) */
    y += 6;

    y = calc_row(s, y, "Tx EL Angle",   "\xB0", &h_cv_txel);
    y = calc_row(s, y, "Rx EL Angle",   "\xB0", &h_cv_rxel);
    y = calc_row(s, y, "Central Angle", "\xB0", &h_cv_central);

    /* h_cv_minalt and h_cv_slant_calc also occupy the same row;
     * one is shown and the other hidden depending on range mode. */
    h_cv_minalt_lbl  = mk_label(s, "Minimum Alt",  SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_minalt      = mk_calc (s, SP_EDIT_X, y+1, ID_CALC);
    h_cv_minalt_unit = mk_unit_link(s, "ft", SP_UNIT_X, y+1, SP_UNIT_W+12, ID_UNIT_MINALT);
    h_unit_minalt    = h_cv_minalt_unit; /* alias so cycle_unit() can update the label */

    h_cv_slant_lbl  = mk_label(s, "Slant Range",  SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_slant_calc = mk_calc (s, SP_EDIT_X, y+1, ID_CALC);
    h_cv_slant_unit = mk_unit (s, "km", SP_UNIT_X, y+1, SP_UNIT_W+12);
    y += ROW_H_CALC;
    (void)y;

    SendMessage(h_range_mode, CB_SETCURSEL, 1, 0);  /* default: Horiz. Dist. mode */
}

void build_sec4(void)  /* Transmitter */
{
    int sep = HDR_H + SP_PAD_TOP + 3*ROW_H_IN;
    HWND s = make_sec(3, COL_X3, GRID_Y, COL_W3, ROW0_H,
                      "4 | Transmitter", sep);
    int y = HDR_H + SP_PAD_TOP;
    mk_label(s, "Saturated Power", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_satpow      = mk_edit(s, "40", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_unit_satpow = mk_unit_link(s, "dBm", SP_UNIT_X, y+3, SP_UNIT_W+8, ID_UNIT_SATPOW);
    y += ROW_H_IN;
    y = field_row(s, y, "Output Backoff",   "0",   "dB", &h_obo);
    y = field_row(s, y, "Feed Losses",      "3",   "dB", &h_feedtx);
    y += 6;
    /* Feed Power: calculated display with clickable unit (W / dBm / dBW) */
    mk_label(s, "Feed Power", SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_feedpow   = mk_calc(s, SP_EDIT_X, y+1, ID_CALC);
    h_unit_feedpow = mk_unit_link(s, "W", SP_UNIT_X, y+1, SP_UNIT_W+12, ID_UNIT_FEEDPOW);
    y += ROW_H_CALC;
    (void)y;
}

void build_sec5(void)  /* Tx Antenna */
{
    int sep = HDR_H + SP_PAD_TOP + 6*ROW_H_IN;
    HWND s = make_sec(4, COL_X0, ROW1_Y, COL_W, ROW1_H,
                      "5 | Tx Antenna", sep);
    int y = HDR_H + SP_PAD_TOP;

    mk_label(s, "Antenna Type", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_tx_mode = mk_combo(s, ANT_MODE_NAMES, 2, SP_EDIT_X, y,
                         SP_EDIT_W + SP_UNIT_W + 2);
    y += ROW_H_IN;

    /* The gain row and the dish (diameter/efficiency) rows share the same y
     * position; update_ant_visibility() shows only the relevant set. */
    h_tx_lbl_gain  = mk_label(s, "Antenna Gain", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_txgain       = mk_edit (s, "-19", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_tx_unit_gain = mk_unit (s, "dBi", SP_UNIT_X, y+3, SP_UNIT_W+8);

    h_tx_lbl_diam  = mk_label(s, "Diameter",  SP_LBL_X, y+3, SP_LBL_W, 18);
    h_tx_diameter  = mk_edit (s, "0.5", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_tx_unit_diam = mk_unit (s, "m",   SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    h_tx_lbl_eff    = mk_label(s, "Efficiency", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_tx_efficiency = mk_edit(s, "55.0", SP_EDIT_X, y, SP_EDIT_W, 22); /* 55% is a typical dish efficiency */
    h_tx_unit_eff   = mk_unit (s, "%",   SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    y = field_row(s, y, "Ant. Axial Ratio", "0",   "dB",   &h_ar_tx);
    y = field_row(s, y, "Wave Axial Ratio", "0",   "dB",   &h_ar_wave);
    y = field_row(s, y, "Pol. Angle",       "0",   "\xB0", &h_polangle);
    y += 6;
    y = calc_row(s, y, "Pol. Loss", "dB", &h_cv_polloss);
    /* Net EIRP has a clickable unit for dBW/dBm/W display */
    mk_label(s, "Net EIRP", SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_eirp  = mk_calc(s, SP_EDIT_X, y+1, ID_CALC);
    h_unit_eirp = mk_unit_link(s, "dBW", SP_UNIT_X, y+1, SP_UNIT_W+12, ID_UNIT_EIRP);
    y += ROW_H_CALC;
    (void)y;

    SendMessage(h_tx_mode, CB_SETCURSEL, 0, 0);  /* default: Antenna Gain (not dish) */
}

void build_sec6(void)  /* Transmission Path */
{
    int sep = HDR_H + SP_PAD_TOP + 4*ROW_H_IN;
    HWND s = make_sec(5, COL_X1, ROW1_Y, COL_W, ROW1_H,
                      "6 | Transmission Path", sep);
    int y = HDR_H + SP_PAD_TOP;

    mk_label(s, "Path Loss Model", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_loss_mode = mk_combo(s, LOSS_MODE_NAMES, 2, SP_EDIT_X, y,
                           SP_EDIT_W + SP_UNIT_W + 2);
    y += ROW_H_IN;

    y = field_row(s, y, "Absorptive Loss",     "0",   "dB", &h_absloss);
    y = field_row(s, y, "Non-absorptive Loss", "0.0", "dB", &h_nonabsloss);

    /* Rain rate row — hidden when Simple FSPL mode is selected */
    h_rainrate_lbl  = mk_label(s, "Rain Rate", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_rainrate      = mk_edit(s, "0", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_rainrate_unit = mk_unit(s, "mm/h", SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    y += 6;
    y = calc_row(s, y, "Free-Space PL", "dB", &h_cv_fspl);

    /* Gaseous and rain attenuation rows — only visible in ITU-R mode */
    h_cv_gasatt_lbl  = mk_label(s, "Gaseous Atten.", SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_gasatt      = mk_calc(s, SP_EDIT_X, y+1, ID_CALC);
    h_cv_gasatt_unit = mk_unit(s, "dB", SP_UNIT_X, y+1, SP_UNIT_W+12);
    y += ROW_H_CALC;

    h_cv_rainatt_lbl  = mk_label(s, "Rain Atten.", SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_rainatt      = mk_calc(s, SP_EDIT_X, y+1, ID_CALC);
    h_cv_rainatt_unit = mk_unit(s, "dB", SP_UNIT_X, y+1, SP_UNIT_W+12);
    y += ROW_H_CALC;

    y = calc_row(s, y, "Total Loss", "dB", &h_cv_totloss);
    (void)y;

    SendMessage(h_loss_mode, CB_SETCURSEL, 0, 0);  /* default: Simple FSPL */
}

void build_sec7(void)  /* Rx Antenna */
{
    int sep = HDR_H + SP_PAD_TOP + 4*ROW_H_IN;
    HWND s = make_sec(6, COL_X2, ROW1_Y, COL_W, ROW1_H,
                      "7 | Rx Antenna", sep);
    int y = HDR_H + SP_PAD_TOP;

    mk_label(s, "Antenna Type", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_rx_mode = mk_combo(s, ANT_MODE_NAMES, 2, SP_EDIT_X, y,
                         SP_EDIT_W + SP_UNIT_W + 2);
    y += ROW_H_IN;

    /* Gain row (direct entry) and dish rows (diameter/efficiency) are
     * mutually exclusive; visibility managed by update_ant_visibility(). */
    h_rx_lbl_gain   = mk_label(s, "Antenna Gain", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_rxgain_direct = mk_edit(s, "-0.6", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_rx_unit_gain  = mk_unit(s, "dBi",  SP_UNIT_X, y+3, SP_UNIT_W+8);

    h_rx_lbl_diam  = mk_label(s, "Diameter",   SP_LBL_X, y+3, SP_LBL_W, 18);
    h_diameter     = mk_edit (s, "0.9", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_rx_unit_diam = mk_unit (s, "m",   SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    h_rx_lbl_eff  = mk_label(s, "Efficiency", SP_LBL_X, y+3, SP_LBL_W, 18);
    h_efficiency  = mk_edit (s, "55.0", SP_EDIT_X, y, SP_EDIT_W, 22);
    h_rx_unit_eff = mk_unit (s, "%",    SP_UNIT_X, y+3, SP_UNIT_W+8);
    y += ROW_H_IN;

    y = field_row(s, y, "Tracking Loss", "0", "dB", &h_trackloss);
    y += 6;

    y = calc_row(s, y, "Antenna Gain", "dBi", &h_cv_rxgain);

    /* Beamwidth display — only meaningful in dish mode */
    h_cv_beamwid_lbl  = mk_label(s, "Beamwidth", SP_LBL_X,  y+1, SP_LBL_W, 16);
    h_cv_beamwid      = mk_calc (s, SP_EDIT_X, y+1, ID_CALC);
    h_cv_beamwid_unit = mk_unit (s, "\xB0", SP_UNIT_X, y+1, SP_UNIT_W+12);
    y += ROW_H_CALC;

    y = calc_row(s, y, "Rx Flux", "dBW/m\xB2", &h_cv_rxflux);
    /* Rx Power has a clickable unit (dBm / dBW / W) */
    mk_label(s, "Rx Power", SP_LBL_X, y+1, SP_LBL_W, 16);
    h_cv_rxpow  = mk_calc(s, SP_EDIT_X, y+1, ID_CALC);
    h_unit_rxpow = mk_unit_link(s, "dBm", SP_UNIT_X, y+1, SP_UNIT_W+12, ID_UNIT_RXPOW);
    y += ROW_H_CALC;
    (void)y;

    SendMessage(h_rx_mode, CB_SETCURSEL, 0, 0);  /* default: Antenna Gain (not dish) */
}

void build_sec8(void)  /* Rx System Noise */
{
    int sep = HDR_H + SP_PAD_TOP + 3*ROW_H_IN;
    HWND s = make_sec(7, COL_X3, ROW1_Y, COL_W3, ROW1_H,
                      "8 | Rx System Noise", sep);
    int y = HDR_H + SP_PAD_TOP;
    /* -203.98 dBW/Hz corresponds to 290 K (IEEE reference temperature) */
    y = field_row(s, y, "Rx Noise",       "-203.98", "dBW/Hz", &h_rxnoise);
    y = field_row(s, y, "Feed-Line Loss",    "0.0",  "dB",     &h_feedrx);
    y = field_row(s, y, "Noise Figure",      "2.0",  "dB",     &h_nf);
    y += 6;
    y = calc_row(s, y, "Sys. Temp", "dBK",   &h_cv_tsys);
    y = calc_row(s, y, "G/T",       "dBi/K", &h_cv_gt);
    y = calc_row(s, y, "C/N\xB0",  "dBHz",  &h_cv_cn0); /* \xB0 = degree symbol (C/N₀) */
    (void)y;
}

/* ============================================================
 * Results panel builder
 * ============================================================ */

/* Small helpers for the three display styles inside the results bar */
static HWND mk_rlabel(HWND p, const char *t, int x, int y, int w)
{
    HWND hw = CreateWindow("STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x, y, w, 16, p, (HMENU)(INT_PTR)ID_RLBL, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

/* Large numeric result value (Eb/N0 or margin) */
static HWND mk_rval(HWND p, int id, int x, int y, int w, HFONT fnt)
{
    HWND hw = CreateWindow("STATIC", "---",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x, y, w, 26, p, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)fnt, FALSE);
    return hw;
}

static HWND mk_runit(HWND p, const char *t, int x, int y, int w)
{
    HWND hw = CreateWindow("STATIC", t,
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x, y, w, 14, p, (HMENU)(INT_PTR)ID_RLBL, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

void build_results(void)
{
    int res_w = WIN_W - 2*MARGIN_X;
    h_resbar = CreateWindow("ResultsPanelClass", "",
        WS_CHILD | WS_VISIBLE,
        MARGIN_X, RES_Y, res_w, RES_H,
        g_hwnd, NULL, g_hinst, NULL);

    /* Divide the results bar into 3 equal columns */
    int col_w  = res_w / 3;
    int y_lbl  = HDR_H + 4;  /* row for the column heading labels */
    int y_val  = HDR_H + 20; /* row for the numeric values */
    int y_unit = HDR_H + 46; /* row for the unit strings */

    /* --- Req Eb/N0 (left column) --- */
    int x0 = col_w/2 - 80;
    mk_rlabel(h_resbar, "Eb/N\xB0 Required", x0, y_lbl, 160);
    h_rv_req = mk_rval(h_resbar, ID_RVAL, x0+30, y_val, 100, g_fnt_rval);
    mk_runit (h_resbar, "dB",  x0+70, y_unit, 30);

    /* --- Avail Eb/N0 (centre column) --- */
    int x1 = col_w + col_w/2 - 80;
    mk_rlabel(h_resbar, "Eb/N\xB0 Available", x1, y_lbl, 160);
    h_rv_ebn0 = mk_rval(h_resbar, ID_RVAL, x1+30, y_val, 100, g_fnt_rval);
    mk_runit (h_resbar, "dB",  x1+70, y_unit, 30);

    /* --- Link Margin (right column) — uses larger font and dynamic colour --- */
    int x2 = 2*col_w + col_w/2 - 80;
    mk_rlabel(h_resbar, "Link Margin", x2, y_lbl, 160);
    h_rv_margin = mk_rval(h_resbar, ID_MVAL, x2+20, y_val, 120, g_fnt_margin);
    mk_runit(h_resbar, "dB", x2+50, y_unit, 26);
    /* CLOSED / OPEN status text beside the unit — ID_MSTAT gets same dynamic colour */
    h_rv_status = CreateWindow("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x2+80, y_unit, 120, 14,
        h_resbar, (HMENU)(INT_PTR)ID_MSTAT, g_hinst, NULL);
    SendMessage(h_rv_status, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
}

/* ============================================================
 * Calculate button (owner-draw)
 * ============================================================ */

/* BS_OWNERDRAW buttons send WM_DRAWITEM to the main window, handled
 * by on_drawitem_btn() in linkbudget_gui.c for the custom dark styling. */
static HWND mk_obtn(const char *t, int x, int y, int w, int h, int id)
{
    HWND hw = CreateWindow("BUTTON", t,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, h, g_hwnd, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hw, WM_SETFONT, (WPARAM)g_fnt_label, FALSE);
    return hw;
}

void build_button(void)
{
    int bw = 300, bh = BTN_H;
    int bx = (WIN_W - bw) / 2; /* centre the main calculate button horizontally */
    h_btn = mk_obtn("  CALCULATE LINK BUDGET  ", bx, BTN_Y, bw, bh, ID_BTN);

    /* Utility buttons: Save/Load on the left, Sweep/Export/PDF on the right */
    int sw = 94, sh = BTN_H;
    h_btn_save   = mk_obtn("Save",   MARGIN_X,                BTN_Y, sw, sh, ID_BTN_SAVE);
    h_btn_load   = mk_obtn("Load",   MARGIN_X + sw + 4,       BTN_Y, sw, sh, ID_BTN_LOAD);
    h_btn_sweep  = mk_obtn("Sweep",  WIN_W-MARGIN_X-3*sw-8,   BTN_Y, sw, sh, ID_BTN_SWEEP);
    h_btn_export = mk_obtn("Export", WIN_W-MARGIN_X-2*sw-4,   BTN_Y, sw, sh, ID_BTN_EXPORT);
    h_btn_print  = mk_obtn("Print",  WIN_W-MARGIN_X-sw,       BTN_Y, sw, sh, ID_BTN_PRINT);
}
