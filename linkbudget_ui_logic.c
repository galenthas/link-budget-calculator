/*
 * linkbudget_ui_logic.c — UI state management and calculation dispatcher.
 * Implements show_group(), update_*_visibility(), cycle_unit(),
 * fill_params() (GUI → lb_params_t), and do_calculate() (runs lb_run()
 * and updates every calculated-value display in the window).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "linkbudget_gui_config.h"
#include "linkbudget_core.h"
#include "linkbudget_ui_logic.h"

/* ============================================================
 * All GUI globals referenced here are defined in linkbudget_gui.c
 * ============================================================ */

/* Unit state indices — each is an index into the corresponding UNIT_*_NAMES array */
extern int g_unit_freq;
extern int g_unit_txalt;
extern int g_unit_rxalt;
extern int g_unit_satpow;
extern int g_unit_feedpow;
extern int g_unit_rxpow;
extern int g_unit_minalt;
extern int g_unit_eirp;

/* Margin colour shared with ResultsPanelProc for dynamic colour change */
extern COLORREF g_margin_color;

/* Unit name string tables */
extern const char *UNIT_FREQ_NAMES[];
extern const char *UNIT_ALT_NAMES[];
extern const char *UNIT_POW_NAMES[];
extern const char *UNIT_FEEDPOW_NAMES[];
extern const char *UNIT_RXPOW_NAMES[];
extern const char *UNIT_MINALT_NAMES[];
extern const char *UNIT_EIRP_NAMES[];

/* Unit-to-canonical conversion factors */
extern const double FREQ_TO_GHZ[]; /* multiply displayed freq × FREQ_TO_GHZ[unit] → GHz */
extern const double ALT_TO_M[];    /* multiply displayed alt  × ALT_TO_M[unit]    → metres */

/* Input HWNDs */
extern HWND h_freq, h_datarate;
extern HWND h_fec, h_mod, h_ber, h_imploss;
extern HWND h_txalt, h_rxalt, h_slant, h_radius;
extern HWND h_satpow, h_obo, h_feedtx;
extern HWND h_txgain, h_ar_tx, h_ar_wave, h_polangle;
extern HWND h_absloss, h_nonabsloss;
extern HWND h_diameter, h_efficiency, h_trackloss;
extern HWND h_rxnoise, h_feedrx, h_nf;

/* Range mode */
extern HWND h_range_mode;
extern HWND h_horiz_dist;
extern HWND h_cv_slant_calc;
extern HWND h_cv_slant_lbl, h_cv_slant_unit;
extern HWND h_cv_minalt_lbl, h_cv_minalt_unit;
extern HWND h_cv_minalt;

/* Antenna mode */
extern HWND h_tx_mode, h_rx_mode;
extern HWND h_tx_diameter, h_tx_efficiency;
extern HWND h_tx_lbl_gain, h_tx_unit_gain;
extern HWND h_tx_lbl_diam, h_tx_unit_diam;
extern HWND h_tx_lbl_eff,  h_tx_unit_eff;
extern HWND h_txgain;
extern HWND h_rxgain_direct;
extern HWND h_rx_lbl_gain, h_rx_unit_gain;
extern HWND h_rx_lbl_diam, h_rx_unit_diam;
extern HWND h_rx_lbl_eff,  h_rx_unit_eff;
extern HWND h_cv_beamwid, h_cv_beamwid_lbl, h_cv_beamwid_unit;

/* Loss mode */
extern HWND h_loss_mode;
extern HWND h_rainrate;
extern HWND h_rainrate_lbl, h_rainrate_unit;
extern HWND h_cv_gasatt, h_cv_rainatt;
extern HWND h_cv_gasatt_lbl, h_cv_gasatt_unit;
extern HWND h_cv_rainatt_lbl, h_cv_rainatt_unit;

/* Clickable unit label HWNDs */
extern HWND h_unit_freq, h_unit_txalt, h_unit_rxalt, h_unit_satpow;
extern HWND h_unit_feedpow, h_unit_rxpow, h_unit_minalt, h_unit_eirp;

/* Calc display HWNDs */
extern HWND h_cv_reqebn0;
extern HWND h_cv_txel, h_cv_rxel, h_cv_central;
extern HWND h_cv_feedpow;
extern HWND h_cv_polloss, h_cv_eirp;
extern HWND h_cv_fspl, h_cv_totloss;
extern HWND h_cv_rxgain, h_cv_rxflux, h_cv_rxpow;
extern HWND h_cv_tsys, h_cv_gt, h_cv_cn0;

/* Result display HWNDs */
extern HWND h_rv_req, h_rv_ebn0, h_rv_margin, h_rv_status;

/* ============================================================
 * get_double — read an edit control's text as a double
 * ============================================================ */
double get_double(HWND hw)
{
    char buf[64];
    GetWindowText(hw, buf, sizeof(buf));
    return atof(buf);
}

/* ============================================================
 * get_combo_index — wraps CB_GETCURSEL for conciseness
 * ============================================================ */
int get_combo_index(HWND h)
{
    return (int)SendMessage(h, CB_GETCURSEL, 0, 0);
}

/* ============================================================
 * set_calc — format a double into a read-only STATIC control
 * ============================================================ */
static void set_calc(HWND hw, const char *fmt, double val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, val);
    SetWindowText(hw, buf);
}

/* ============================================================
 * Visibility helpers
 * These functions show or hide groups of related controls when
 * the user switches between modes (range, antenna type, loss model).
 * ============================================================ */

/* Show or hide an array of HWNDs in a single call */
void show_group(HWND *arr, int n, BOOL show)
{
    int sw = show ? SW_SHOW : SW_HIDE;
    for (int i = 0; i < n; i++) ShowWindow(arr[i], sw);
}

/* Sec 3: toggle between manual Slant Range input and Horiz. Distance input.
 * When HORIZ mode is active, slant range is calculated and shown as a display. */
void update_range_visibility(void)
{
    int from_dist = (get_combo_index(h_range_mode) == RANGE_MODE_HORIZ);
    /* Manual mode controls: h_slant (input) + min-alt display */
    HWND manual[] = { h_slant, h_cv_minalt, h_cv_minalt_lbl, h_cv_minalt_unit };
    /* Horiz. Distance mode controls: h_horiz_dist (input) + slant calc display */
    HWND horiz[]  = { h_horiz_dist, h_cv_slant_calc, h_cv_slant_lbl, h_cv_slant_unit };
    show_group(manual, NELEM(manual), !from_dist);
    show_group(horiz,  NELEM(horiz),   from_dist);
}

/* Sec 5 & 7: toggle between direct antenna gain input and parabolic dish inputs */
void update_ant_visibility(void)
{
    int tx_dish = (get_combo_index(h_tx_mode) == ANT_MODE_DISH);
    int rx_dish = (get_combo_index(h_rx_mode) == ANT_MODE_DISH);

    HWND tx_gain[]   = { h_txgain, h_tx_lbl_gain, h_tx_unit_gain };
    HWND tx_dish_w[] = { h_tx_diameter, h_tx_lbl_diam, h_tx_unit_diam,
                         h_tx_efficiency, h_tx_lbl_eff, h_tx_unit_eff };
    show_group(tx_gain,   NELEM(tx_gain),   !tx_dish);
    show_group(tx_dish_w, NELEM(tx_dish_w),  tx_dish);

    HWND rx_gain[]   = { h_rxgain_direct, h_rx_lbl_gain, h_rx_unit_gain };
    /* Dish mode also shows the beamwidth calculated display */
    HWND rx_dish_w[] = { h_diameter, h_rx_lbl_diam, h_rx_unit_diam,
                         h_efficiency, h_rx_lbl_eff, h_rx_unit_eff,
                         h_cv_beamwid, h_cv_beamwid_lbl, h_cv_beamwid_unit };
    show_group(rx_gain,   NELEM(rx_gain),   !rx_dish);
    show_group(rx_dish_w, NELEM(rx_dish_w),  rx_dish);
}

/* Sec 6: show/hide ITU-R specific inputs and output rows */
void update_loss_visibility(void)
{
    int itu = (get_combo_index(h_loss_mode) == LOSS_MODE_ITU);
    HWND itu_rows[] = {
        h_rainrate_lbl,   h_rainrate,       h_rainrate_unit,
        h_cv_gasatt_lbl,  h_cv_gasatt,      h_cv_gasatt_unit,
        h_cv_rainatt_lbl, h_cv_rainatt,     h_cv_rainatt_unit,
    };
    show_group(itu_rows, NELEM(itu_rows), itu);
}

/* Master visibility update — call this whenever a mode-selector combo changes */
void update_visibility(void)
{
    update_range_visibility();
    update_ant_visibility();
    update_loss_visibility();
}

/* ============================================================
 * cycle_unit — advance displayed unit for a clickable unit label.
 * For input-field units (freq, altitude, TX power) the field value
 * is converted to preserve the physical quantity.
 * For output-display units (feed power, EIRP, etc.) only the state
 * index advances; do_calculate() will re-render the converted value.
 * ============================================================ */
void cycle_unit(int id)
{
    char buf[32];
    switch (id) {
    case ID_UNIT_FREQ: {
        /* Convert current displayed value to GHz, then to the new unit */
        double val_ghz = get_double(h_freq) * FREQ_TO_GHZ[g_unit_freq];
        g_unit_freq = (g_unit_freq + 1) % 3;
        snprintf(buf, sizeof(buf), "%.6g", val_ghz / FREQ_TO_GHZ[g_unit_freq]);
        SetWindowText(h_freq,      buf);
        SetWindowText(h_unit_freq, UNIT_FREQ_NAMES[g_unit_freq]);
        break;
    }
    case ID_UNIT_TXALT: {
        double val_m = get_double(h_txalt) * ALT_TO_M[g_unit_txalt];
        g_unit_txalt = (g_unit_txalt + 1) % 4;
        snprintf(buf, sizeof(buf), "%.6g", val_m / ALT_TO_M[g_unit_txalt]);
        SetWindowText(h_txalt,      buf);
        SetWindowText(h_unit_txalt, UNIT_ALT_NAMES[g_unit_txalt]);
        break;
    }
    case ID_UNIT_RXALT: {
        double val_m = get_double(h_rxalt) * ALT_TO_M[g_unit_rxalt];
        g_unit_rxalt = (g_unit_rxalt + 1) % 4;
        snprintf(buf, sizeof(buf), "%.6g", val_m / ALT_TO_M[g_unit_rxalt]);
        SetWindowText(h_rxalt,      buf);
        SetWindowText(h_unit_rxalt, UNIT_ALT_NAMES[g_unit_rxalt]);
        break;
    }
    case ID_UNIT_SATPOW: {
        /* TX power is a 2-state toggle (W ↔ dBm) rather than a 3-state cycle */
        if (g_unit_satpow == 0) {   /* W → dBm: P_dBm = 10·log10(P_W) + 30 */
            double w = get_double(h_satpow);
            if (w <= 0) w = 1e-30; /* guard against log(0) */
            snprintf(buf, sizeof(buf), "%.2f", 10.0*log10(w) + 30.0);
            g_unit_satpow = 1;
        } else {                     /* dBm → W: P_W = 10^((P_dBm-30)/10) */
            double dbm = get_double(h_satpow);
            snprintf(buf, sizeof(buf), "%.4g", pow(10.0, (dbm-30.0)/10.0));
            g_unit_satpow = 0;
        }
        SetWindowText(h_satpow,      buf);
        SetWindowText(h_unit_satpow, UNIT_POW_NAMES[g_unit_satpow]);
        break;
    }
    /* Output-display-only units: just advance the index; do_calculate() handles re-rendering */
    case ID_UNIT_FEEDPOW:
        g_unit_feedpow = (g_unit_feedpow + 1) % 3;
        SetWindowText(h_unit_feedpow, UNIT_FEEDPOW_NAMES[g_unit_feedpow]);
        break;
    case ID_UNIT_RXPOW:
        g_unit_rxpow = (g_unit_rxpow + 1) % 3;
        SetWindowText(h_unit_rxpow, UNIT_RXPOW_NAMES[g_unit_rxpow]);
        break;
    case ID_UNIT_MINALT:
        g_unit_minalt = (g_unit_minalt + 1) % 3;
        SetWindowText(h_unit_minalt, UNIT_MINALT_NAMES[g_unit_minalt]);
        break;
    case ID_UNIT_EIRP:
        g_unit_eirp = (g_unit_eirp + 1) % 3;
        SetWindowText(h_unit_eirp, UNIT_EIRP_NAMES[g_unit_eirp]);
        break;
    }
}

/* ============================================================
 * fill_params — read GUI state into lb_params_t.
 * Returns 1 if all critical fields are valid, 0 on any error.
 * Called by do_calculate(), export_csv(), print_report(), and sweep_run().
 * ============================================================ */
int fill_params(lb_params_t *p)
{
    /* Convert displayed frequency to GHz using the current unit factor */
    p->freq_GHz       = get_double(h_freq) * FREQ_TO_GHZ[g_unit_freq];
    p->data_rate_Mbps = get_double(h_datarate);
    if (p->freq_GHz <= 0 || p->data_rate_Mbps <= 0) return 0; /* mandatory positive values */

    int fec_idx = get_combo_index(h_fec);
    int mod_idx = get_combo_index(h_mod);
    int ber_idx = get_combo_index(h_ber);
    if (fec_idx < 0 || mod_idx < 0 || ber_idx < 0) return 0; /* no selection in combo */
    p->fec_decoder  = (lb_fec_t)fec_idx;
    p->modulation   = (lb_mod_t)mod_idx;
    p->ber          = (lb_ber_t)ber_idx;
    p->impl_loss_dB = get_double(h_imploss);

    /* Convert altitudes to km (internal unit) */
    p->tx_alt_km     = get_double(h_txalt) * ALT_TO_M[g_unit_txalt] / 1000.0;
    p->rx_alt_km     = get_double(h_rxalt) * ALT_TO_M[g_unit_rxalt] / 1000.0;
    p->radius_factor = get_double(h_radius);
    if (p->radius_factor <= 0) return 0;

    int from_dist = (get_combo_index(h_range_mode) == RANGE_MODE_HORIZ);
    if (from_dist) {
        /* Horiz. Distance mode: derive slant range as hypotenuse of horizontal
         * distance and altitude difference (planar approximation) */
        double d  = get_double(h_horiz_dist);
        double dh = fabs(p->tx_alt_km - p->rx_alt_km);
        p->slant_range_km = sqrt(d*d + dh*dh);
        if (p->slant_range_km < 0.001) p->slant_range_km = 0.001; /* minimum 1 m */
    } else {
        p->slant_range_km = get_double(h_slant);
    }

    /* Convert TX power to watts for the dBW conversion below */
    double sat_w = (g_unit_satpow == 1)
        ? pow(10.0, (get_double(h_satpow) - 30.0) / 10.0) /* dBm → W */
        : get_double(h_satpow);                             /* W (already) */
    if (sat_w <= 0) return 0;
    p->sat_power_dBW   = 10.0 * log10(sat_w);
    p->obo_dB          = get_double(h_obo);
    p->feed_loss_tx_dB = get_double(h_feedtx);

    /* TX antenna: either direct gain or compute from dish parameters */
    if (get_combo_index(h_tx_mode) == ANT_MODE_DISH) {
        double d = get_double(h_tx_diameter), e = get_double(h_tx_efficiency) / 100.0;
        if (d <= 0 || e <= 0) return 0;
        p->tx_ant_gain_dBi = lb_rx_ant_gain_dBi(d, e, p->freq_GHz);
    } else {
        p->tx_ant_gain_dBi = get_double(h_txgain);
    }
    /* Polarisation loss computed directly from the three pol parameters */
    p->pol_loss_dB = lb_pol_loss_dB(get_double(h_ar_tx),
                                     get_double(h_ar_wave),
                                     get_double(h_polangle));

    p->absorptive_loss_dB     = get_double(h_absloss);
    /* Tracking loss is a non-absorptive loss and is folded in here */
    p->non_absorptive_loss_dB = get_double(h_nonabsloss) + get_double(h_trackloss);

    /* RX antenna: direct gain or dish (same pattern as TX) */
    if (get_combo_index(h_rx_mode) == ANT_MODE_DISH) {
        double d = get_double(h_diameter), e = get_double(h_efficiency) / 100.0;
        if (d <= 0 || e <= 0) return 0;
        p->rx_ant_gain_dBi = lb_rx_ant_gain_dBi(d, e, p->freq_GHz);
    } else {
        p->rx_ant_gain_dBi = get_double(h_rxgain_direct);
    }

    /* Convert RX noise temperature from dBW/Hz to Kelvin:
     * T_K = 10^((T_dBWHz - LB_BOLTZMANN_DB) / 10)
     * because T_dBWHz = 10·log10(k·T), so T = 10^(T_dBWHz/10) / k */
    double rn = get_double(h_rxnoise);
    p->rx_noise_K       = pow(10.0, (rn - LB_BOLTZMANN_DB) / 10.0);
    p->feedline_loss_dB = get_double(h_feedrx);
    p->noise_figure_dB  = get_double(h_nf);
    return 1;
}

/* ============================================================
 * do_calculate — read params, run lb_run once, update all displays.
 * This is the central recalculation routine; it is called:
 *   • on startup (initial display)
 *   • after every input change (via WM_USER+1)
 *   • directly when the Calculate button is clicked
 * ============================================================ */
void do_calculate(void)
{
    lb_params_t p = {0};
    if (!fill_params(&p)) return; /* silently ignore invalid state during editing */

    int from_dist = (get_combo_index(h_range_mode) == RANGE_MODE_HORIZ);

    /* Geometry pass: compute angles using the slant range derived by fill_params
     * so the ITU-R elevation angle is available before the single lb_run() call. */
    lb_geometry_t geo = lb_geometry_angles(
        p.tx_alt_km, p.rx_alt_km, p.slant_range_km, p.radius_factor);

    /* ITU-R atmospheric losses (only in ITU-R P.676+P.838 mode) */
    double el_deg = (geo.rx_el_deg > 5.0) ? geo.rx_el_deg : 5.0; /* 5° elevation floor */
    int use_itu = (get_combo_index(h_loss_mode) == LOSS_MODE_ITU);
    double gas_dB = 0.0, rain_dB = 0.0;
    if (use_itu) {
        gas_dB  = lb_gaseous_atten_dB(p.freq_GHz, el_deg,
                                       ITU_STD_P_HPA, ITU_STD_T_C, ITU_STD_RHO_GM3);
        double R_mmh = get_double(h_rainrate);
        /* Effective rain path = horizontal projection through the rain column */
        double L_eff = p.slant_range_km * sin(el_deg * M_PI / 180.0);
        rain_dB = lb_rain_atten_dB(p.freq_GHz, R_mmh, L_eff);
        /* Add atmospheric losses to absorptive loss before the single lb_run() */
        p.absorptive_loss_dB += gas_dB + rain_dB;
    }

    /* Single lb_run() call with all losses fully accounted for */
    lb_result_t r = lb_run(&p);

    /* Recover TX power in watts for the feed-power display calculation */
    double sat_w = (g_unit_satpow == 1)
        ? pow(10.0, (get_double(h_satpow) - 30.0) / 10.0)
        : get_double(h_satpow);
    /* RX dish diameter needed for the beamwidth display; 0 = not in dish mode */
    double diam = (get_combo_index(h_rx_mode) == ANT_MODE_DISH)
        ? get_double(h_diameter) : 0.0;

    /* ---- Update section calc displays ---- */

    /* Sec 2 — required Eb/N0 */
    set_calc(h_cv_reqebn0, "%.2f", r.req_ebn0_dB);

    /* Sec 3 — geometry angles; recompute with the final slant range used by lb_run */
    geo = lb_geometry_angles(p.tx_alt_km, p.rx_alt_km,
                              r.slant_range_km_used, p.radius_factor);
    set_calc(h_cv_txel,    "%.2f", geo.tx_el_deg);
    set_calc(h_cv_rxel,    "%.2f", geo.rx_el_deg);
    set_calc(h_cv_central, "%.2f", geo.central_angle_deg);
    if (from_dist)
        /* Horiz. Distance mode: show the calculated slant range */
        set_calc(h_cv_slant_calc, "%.3f", r.slant_range_km_used);
    else {
        /* Manual slant-range mode: show minimum endpoint altitude in selected unit */
        double ma = geo.min_alt_ft;
        if      (g_unit_minalt == 1) ma *= 0.3048;          /* ft → m */
        else if (g_unit_minalt == 2) ma *= 0.3048 / 1000.0; /* ft → km */
        set_calc(h_cv_minalt, "%.3g", ma);
    }

    /* Sec 4 — feed power: P_feed = P_sat × 10^(-(OBO + feed_loss)/10) */
    double feed_pow_w = sat_w * pow(10.0, -(p.obo_dB + p.feed_loss_tx_dB) / 10.0);
    if (feed_pow_w <= 0) feed_pow_w = 1e-30;
    if      (g_unit_feedpow == 0) set_calc(h_cv_feedpow, "%.4g",  feed_pow_w);         /* W */
    else if (g_unit_feedpow == 1) set_calc(h_cv_feedpow, "%.2f",  10.0*log10(feed_pow_w)+30.0); /* dBm */
    else                          set_calc(h_cv_feedpow, "%.2f",  10.0*log10(feed_pow_w));      /* dBW */

    /* Sec 5 — polarisation loss and net EIRP */
    set_calc(h_cv_polloss, "%.2f", p.pol_loss_dB);
    if      (g_unit_eirp == 0) set_calc(h_cv_eirp, "%.2f", r.eirp_dBW);                 /* dBW */
    else if (g_unit_eirp == 1) set_calc(h_cv_eirp, "%.2f", r.eirp_dBW + 30.0);          /* dBm */
    else                       set_calc(h_cv_eirp, "%.3g", pow(10.0, r.eirp_dBW/10.0)); /* W */

    /* Sec 6 — path losses */
    double total_loss = r.fspl_dB + p.absorptive_loss_dB + p.non_absorptive_loss_dB;
    set_calc(h_cv_fspl,    "%.2f", r.fspl_dB);
    set_calc(h_cv_gasatt,  "%.3f", gas_dB);   /* 0.000 when Simple FSPL mode */
    set_calc(h_cv_rainatt, "%.3f", rain_dB);
    set_calc(h_cv_totloss, "%.2f", total_loss);

    /* Sec 7 — RX antenna derived quantities */
    double r_m     = r.slant_range_km_used * 1e3; /* slant range in metres */
    /* Power flux density at RX: EIRP spread over a sphere of radius r_m,
     * then subtract absorptive loss (non-absorptive doesn't affect flux) */
    double rx_flux = r.eirp_dBW
                   - 10.0*log10(4.0*M_PI*r_m*r_m)
                   - p.absorptive_loss_dB;
    /* Received power = EIRP - total_loss + G_rx; +30 dB for dBW → dBm */
    double rx_pow_dbm = r.eirp_dBW - total_loss + p.rx_ant_gain_dBi + 30.0;
    set_calc(h_cv_rxgain, "%.2f", p.rx_ant_gain_dBi);
    if (diam > 0)
        set_calc(h_cv_beamwid, "%.2f", lb_rx_ant_beamwidth_deg(diam, p.freq_GHz));
    set_calc(h_cv_rxflux, "%.2f", rx_flux);
    if      (g_unit_rxpow == 0) set_calc(h_cv_rxpow, "%.2f", rx_pow_dbm);                     /* dBm */
    else if (g_unit_rxpow == 1) set_calc(h_cv_rxpow, "%.2f", rx_pow_dbm - 30.0);             /* dBW */
    else                        set_calc(h_cv_rxpow, "%.3g", pow(10.0,(rx_pow_dbm-30.0)/10.0)); /* W */

    /* Sec 8 — noise chain */
    set_calc(h_cv_tsys, "%.2f", 10.0*log10(r.t_sys_K)); /* T_sys in dBK */
    set_calc(h_cv_gt,   "%.2f", r.gt_dBK);
    set_calc(h_cv_cn0,  "%.2f", r.cn0_dBHz);

    /* ---- Update results panel ---- */
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", r.req_ebn0_dB);
    SetWindowText(h_rv_req,  buf);
    snprintf(buf, sizeof(buf), "%.2f", r.ebn0_dB);
    SetWindowText(h_rv_ebn0, buf);
    /* %+.2f forces a leading '+' sign for positive margins */
    snprintf(buf, sizeof(buf), "%+.2f", r.link_margin_dB);
    SetWindowText(h_rv_margin, buf);

    /* Update the dynamic colour used by ResultsPanelProc's WM_CTLCOLORSTATIC */
    g_margin_color = (r.link_margin_dB >= 0.0) ? C_POS : C_NEG;
    /* \x95 is the bullet character (•) in CP1252 */
    SetWindowText(h_rv_status,
        r.closed ? "  \x95 CLOSED" : "  \x95 OPEN");

    /* InvalidateRect forces an immediate WM_CTLCOLORSTATIC repaint so the
     * margin and status colours update in sync with the new values */
    InvalidateRect(h_rv_margin, NULL, TRUE);
    InvalidateRect(h_rv_status, NULL, TRUE);
}
