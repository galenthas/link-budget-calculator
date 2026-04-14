/*
 * linkbudget_pdf.c — PDF report generator.
 * Builds a single-page PDF-1.4 document entirely in memory using a
 * growing byte buffer (PB), then writes it to a file chosen by the user.
 * No external PDF library is required; the output uses only three PDF
 * built-in fonts: Helvetica, Helvetica-Bold, and Courier.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "linkbudget_gui_config.h"
#include "linkbudget_core.h"
#include "linkbudget_pdf.h"
#include "linkbudget_ui_logic.h"   /* fill_params, get_double, get_combo_index */

/* ============================================================
 * Additional externs needed inside this file
 * ============================================================ */
extern int  g_unit_freq;
extern int  g_unit_txalt;
extern int  g_unit_rxalt;
extern int  g_unit_satpow;
extern HWND h_freq, h_txalt, h_rxalt, h_satpow;
extern HWND h_fec, h_mod, h_ber;
extern HWND h_tx_mode, h_rx_mode, h_loss_mode;
extern HWND h_rainrate;
extern const char *UNIT_FREQ_NAMES[];
extern const char *UNIT_ALT_NAMES[];
extern const char *UNIT_POW_NAMES[];

/* ============================================================
 * Growing byte buffer (PB)
 * PDF content is accumulated in memory before being flushed to
 * disk in a single fwrite, avoiding repeated file seeks.
 * ============================================================ */

/* Ensure the buffer can hold at least 'extra' additional bytes,
 * doubling capacity plus the request size to amortise reallocations. */
void pb_reserve(PB *b, int extra)
{
    if (b->n + extra >= b->cap) {
        b->cap = (b->n + extra) * 2 + 8192;
        b->d   = (char *)realloc(b->d, (size_t)b->cap);
    }
}

/* Append a printf-formatted string to the buffer.
 * Uses a two-pass vsnprintf to measure the required space before writing. */
void pb_printf(PB *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap); /* measure first */
    va_end(ap);
    pb_reserve(b, need + 1);
    va_start(ap, fmt);
    vsnprintf(b->d + b->n, (size_t)(need + 1), fmt, ap);
    va_end(ap);
    b->n += need; /* advance write cursor (excludes the NUL) */
}

void pb_free(PB *b)
{
    free(b->d);
    b->d   = NULL;
    b->n   = 0;
    b->cap = 0;
}

/* ============================================================
 * PDF helpers
 * ============================================================ */

/* Escape characters that are special inside PDF string literals: '(' ')' '\' */
void pdf_esc(const char *s, char *out, int outlen)
{
    int i = 0, j = 0;
    while (s[i] && j < outlen - 3) {
        char c = s[i++];
        if (c == '(' || c == ')' || c == '\\') out[j++] = '\\';
        out[j++] = c;
    }
    out[j] = '\0';
}

/* Draw a horizontal rule across the full column width.
 * RG = stroke colour; w = line width; m/l = move-to/line-to; S = stroke. */
void ppg_hline(PPage *pg, double r, double g, double b, double lw)
{
    pb_printf(pg->cs,
        "%.3f %.3f %.3f RG %.2f w %.1f %.1f m %.1f %.1f l S\n",
        r, g, b, lw, pg->ml, pg->y, pg->ml + pg->cw, pg->y);
    pg->y -= lw + 3.0; /* advance cursor by line width plus a small gap */
}

/* Draw a shaded section header strip with bold blue title text.
 * 're f' fills a rectangle; BT/ET wraps the text operator block. */
void ppg_section(PPage *pg, const char *title)
{
    char esc[256];
    pg->y -= 5.0; /* extra space before the section header */
    /* Fill a light-blue rectangle for the header background */
    pb_printf(pg->cs,
        "0.859 0.902 0.941 rg %.1f %.1f %.1f %.1f re f\n",
        pg->ml - 2, pg->y - pg->sh + 2.0, pg->cw + 4.0, pg->sh);
    pdf_esc(title, esc, sizeof(esc));
    /* /F2 = Helvetica-Bold; dark-blue text colour (0.102, 0.227, 0.361) */
    pb_printf(pg->cs,
        "BT /F2 9 Tf 0.102 0.227 0.361 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n",
        pg->ml + 4.0, pg->y - pg->sh + 5.5, esc);
    pg->y -= pg->sh + 1.5;
}

/* Emit one data row with three columns: label | value | unit.
 * Column positions are derived from proportional offsets into pg->cw:
 *   label at 0%, value at 55%, unit at 78%.
 * /F1=Helvetica, /F3=Courier (for monospace numeric values). */
void ppg_row(PPage *pg, const char *lbl, const char *val, const char *unit)
{
    char el[256], ev[128], eu[64];
    double ty = pg->y - pg->rh + 3.5; /* baseline y for this row */
    double xl = pg->ml + 4.0;
    double xv = pg->ml + pg->cw * 0.55; /* value column at 55% of content width */
    double xu = pg->ml + pg->cw * 0.78; /* unit column at 78% */
    pdf_esc(lbl,  el, sizeof(el));
    pdf_esc(val,  ev, sizeof(ev));
    pdf_esc(unit, eu, sizeof(eu));
    pb_printf(pg->cs,
        "BT /F1 8.5 Tf 0.2 0.2 0.2 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n"  /* label */
        "BT /F3 8.5 Tf 0.2 0.2 0.2 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n"  /* value (monospace) */
        "BT /F1 8.5 Tf 0.4 0.4 0.4 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n", /* unit (muted) */
        xl, ty, el,  xv, ty, ev,  xu, ty, eu);
    pg->y -= pg->rh;
}

/* Emit a highlighted result row — used for the final Eb/N0 / margin / status rows.
 * Label in Helvetica-Bold blue; value in the caller-supplied colour (rv,gv,bv);
 * 1.3× row height gives extra vertical breathing room. */
void ppg_result(PPage *pg, const char *lbl, const char *val,
                const char *unit, double rv, double gv, double bv)
{
    char el[256], ev[128], eu[64];
    double ty = pg->y - pg->rh + 4.0;
    double xl = pg->ml + 4.0;
    double xv = pg->ml + pg->cw * 0.55;
    double xu = pg->ml + pg->cw * 0.78;
    pdf_esc(lbl,  el, sizeof(el));
    pdf_esc(val,  ev, sizeof(ev));
    pdf_esc(unit, eu, sizeof(eu));
    pb_printf(pg->cs,
        "BT /F2 9 Tf 0.102 0.227 0.361 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n" /* bold label */
        "BT /F2 10 Tf %.3f %.3f %.3f rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n"   /* bold value in supplied colour */
        "BT /F1 9 Tf 0.333 0.333 0.333 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n",/* grey unit */
        xl, ty, el,
        rv, gv, bv, xv, ty, ev,
        xu, ty, eu);
    pg->y -= pg->rh * 1.3; /* 30% taller row for result entries */
}

/* Read the currently selected combo item text into buf */
void combo_sel_text(HWND combo, char *buf, int len)
{
    buf[0] = '\0';
    int sel = get_combo_index(combo);
    if (sel >= 0) SendMessageA(combo, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)buf);
    (void)len; /* buf size guard not strictly enforced; combo text is always short */
}

/* ============================================================
 * print_report — build and save the PDF report
 * ============================================================ */
void print_report(void)
{
    lb_params_t p = {0};
    if (!fill_params(&p)) {
        MessageBoxA(g_hwnd, "Invalid parameters - cannot generate report.",
                    "PDF Report", MB_ICONWARNING | MB_OK);
        return;
    }

    /* --- Two-pass calculation (same logic as do_calculate) ---
     * Pass 1: lb_run() with the base params to get geometry for elevation angle.
     * Pass 2: lb_run() with ITU-R atmospheric losses added to absorptive loss. */
    lb_result_t r0 = lb_run(&p);
    lb_geometry_t geo = lb_geometry_angles(
        p.tx_alt_km, p.rx_alt_km, r0.slant_range_km_used, p.radius_factor);
    double el_deg = (geo.rx_el_deg > 5.0) ? geo.rx_el_deg : 5.0;

    int use_itu = (get_combo_index(h_loss_mode) == LOSS_MODE_ITU);
    double gas_dB = 0.0, rain_dB = 0.0;
    if (use_itu) {
        gas_dB  = lb_gaseous_atten_dB(p.freq_GHz, el_deg,
                                       ITU_STD_P_HPA, ITU_STD_T_C, ITU_STD_RHO_GM3);
        rain_dB = lb_rain_atten_dB(p.freq_GHz, get_double(h_rainrate),
                                    r0.slant_range_km_used * sin(el_deg * M_PI / 180.0));
        p.absorptive_loss_dB += gas_dB + rain_dB;
    }
    lb_result_t r = lb_run(&p); /* final result with all losses */
    /* Recompute geometry with the definitive slant range */
    geo = lb_geometry_angles(p.tx_alt_km, p.rx_alt_km,
                              r.slant_range_km_used, p.radius_factor);
    double total_loss = r.fspl_dB + p.absorptive_loss_dB + p.non_absorptive_loss_dB;
    double rx_pow_dbm = r.eirp_dBW - total_loss + p.rx_ant_gain_dBi + 30.0;

    /* Save As dialog */
    char path[MAX_PATH] = "LinkBudgetReport.pdf";
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = "PDF Files (*.pdf)\0*.pdf\0All Files\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = "pdf";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = "Save PDF Report";
    if (!GetSaveFileNameA(&ofn)) return;

    /* --- Build the PDF content stream (page drawing operations) ---
     * The content stream uses PDF coordinate system: origin at bottom-left,
     * y increases upward.  pg.y starts near the top of an A4 page (842 pt). */
    PB cs = {0};
    PPage pg;
    pg.cs = &cs;
    pg.y  = 800.0;  /* starting y position (points from page bottom) */
    pg.ml = 50.0;   /* left margin */
    pg.cw = 495.0;  /* content width: A4 (595) - 2×50 margins = 495 */
    pg.rh = 13.5;   /* normal row height (points) */
    pg.sh = 15.0;   /* section header height (points) */

    char buf[128], cbuf[128], esc[256];

    /* Title and timestamp in the page header */
    SYSTEMTIME st; GetLocalTime(&st);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    pdf_esc(buf, esc, sizeof(esc));
    /* "1 0 0 1 x y Tm" sets the text matrix (translation only, no rotation/scale) */
    pb_printf(&cs,
        "BT /F2 16 Tf 0.102 0.227 0.361 rg 1 0 0 1 %.1f %.1f Tm"
        " (LINK BUDGET REPORT) Tj ET\n"
        "BT /F1 8 Tf 0.4 0.4 0.4 rg 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n",
        pg.ml, pg.y, pg.ml + pg.cw - 96.0, pg.y + 4.0, esc);
    pg.y -= 20.0;
    ppg_hline(&pg, 0.102, 0.227, 0.361, 1.5); /* thick blue rule under title */
    pg.y -= 2.0;

    /* --- Section: Signal --- */
    ppg_section(&pg, "SIGNAL");
    { char b[64]; GetWindowTextA(h_freq, b, sizeof(b));
      ppg_row(&pg, "Frequency",  b, UNIT_FREQ_NAMES[g_unit_freq]); }
    snprintf(buf, sizeof(buf), "%.4g", p.data_rate_Mbps);
    ppg_row(&pg, "Data Rate",  buf, "Mb/s");
    combo_sel_text(h_fec, cbuf, sizeof(cbuf)); ppg_row(&pg, "FEC Decoder", cbuf, "");
    combo_sel_text(h_mod, cbuf, sizeof(cbuf)); ppg_row(&pg, "Modulation",  cbuf, "");
    combo_sel_text(h_ber, cbuf, sizeof(cbuf)); ppg_row(&pg, "BER Target",  cbuf, "");
    snprintf(buf, sizeof(buf), "%.2f", p.impl_loss_dB);
    ppg_row(&pg, "Impl. Loss", buf, "dB");

    /* --- Section: Geometry --- */
    ppg_section(&pg, "GEOMETRY");
    { char b[64]; GetWindowTextA(h_txalt, b, sizeof(b));
      ppg_row(&pg, "Tx Altitude", b, UNIT_ALT_NAMES[g_unit_txalt]); }
    { char b[64]; GetWindowTextA(h_rxalt, b, sizeof(b));
      ppg_row(&pg, "Rx Altitude", b, UNIT_ALT_NAMES[g_unit_rxalt]); }
    snprintf(buf, sizeof(buf), "%.4g", r.slant_range_km_used);
    ppg_row(&pg, "Slant Range",   buf, "km");
    snprintf(buf, sizeof(buf), "%.2f", geo.tx_el_deg);
    ppg_row(&pg, "Tx EL Angle",   buf, "deg");
    snprintf(buf, sizeof(buf), "%.2f", geo.rx_el_deg);
    ppg_row(&pg, "Rx EL Angle",   buf, "deg");
    snprintf(buf, sizeof(buf), "%.2f", geo.central_angle_deg);
    ppg_row(&pg, "Central Angle", buf, "deg");

    /* --- Section: Transmitter --- */
    ppg_section(&pg, "TRANSMITTER");
    { char b[64]; GetWindowTextA(h_satpow, b, sizeof(b));
      ppg_row(&pg, "Saturated Power", b, UNIT_POW_NAMES[g_unit_satpow]); }
    snprintf(buf, sizeof(buf), "%.2f", p.obo_dB);
    ppg_row(&pg, "Output Back-Off", buf, "dB");
    snprintf(buf, sizeof(buf), "%.2f", p.feed_loss_tx_dB);
    ppg_row(&pg, "Feed Loss TX",    buf, "dB");
    snprintf(buf, sizeof(buf), "%.2f", r.eirp_dBW);
    ppg_row(&pg, "Net EIRP",        buf, "dBW");

    /* --- Section: TX Antenna --- */
    ppg_section(&pg, "TX ANTENNA");
    combo_sel_text(h_tx_mode, cbuf, sizeof(cbuf));
    ppg_row(&pg, "Antenna Type", cbuf, "");
    snprintf(buf, sizeof(buf), "%.2f", p.tx_ant_gain_dBi);
    ppg_row(&pg, "Antenna Gain", buf, "dBi");
    snprintf(buf, sizeof(buf), "%.2f", p.pol_loss_dB);
    ppg_row(&pg, "Pol. Loss",    buf, "dB");

    /* --- Section: Transmission Path --- */
    ppg_section(&pg, "TRANSMISSION PATH");
    combo_sel_text(h_loss_mode, cbuf, sizeof(cbuf));
    ppg_row(&pg, "Path Loss Model",  cbuf, "");
    snprintf(buf, sizeof(buf), "%.2f", r.fspl_dB);
    ppg_row(&pg, "Free-Space PL",    buf, "dB");
    if (use_itu) {
        /* Only print ITU-R rows when ITU-R mode is active */
        snprintf(buf, sizeof(buf), "%.3f", gas_dB);
        ppg_row(&pg, "Gaseous Atten.", buf, "dB");
        snprintf(buf, sizeof(buf), "%.3f", rain_dB);
        ppg_row(&pg, "Rain Atten.",    buf, "dB");
    }
    snprintf(buf, sizeof(buf), "%.2f", p.absorptive_loss_dB);
    ppg_row(&pg, "Absorptive Loss",  buf, "dB");
    snprintf(buf, sizeof(buf), "%.2f", p.non_absorptive_loss_dB);
    ppg_row(&pg, "Non-absorptive",   buf, "dB");
    snprintf(buf, sizeof(buf), "%.2f", total_loss);
    ppg_row(&pg, "Total Path Loss",  buf, "dB");

    /* --- Section: RX Antenna --- */
    ppg_section(&pg, "RX ANTENNA");
    combo_sel_text(h_rx_mode, cbuf, sizeof(cbuf));
    ppg_row(&pg, "Antenna Type", cbuf, "");
    snprintf(buf, sizeof(buf), "%.2f", p.rx_ant_gain_dBi);
    ppg_row(&pg, "Antenna Gain", buf, "dBi");
    snprintf(buf, sizeof(buf), "%.2f", rx_pow_dbm);
    ppg_row(&pg, "Rx Power",     buf, "dBm");

    /* --- Section: RX System Noise --- */
    ppg_section(&pg, "RX SYSTEM NOISE");
    snprintf(buf, sizeof(buf), "%.2f", 10.0 * log10(r.t_sys_K)); /* T_sys in dBK */
    ppg_row(&pg, "System Temp",  buf, "dBK");
    snprintf(buf, sizeof(buf), "%.2f", r.gt_dBK);
    ppg_row(&pg, "G/T",          buf, "dBi/K");
    snprintf(buf, sizeof(buf), "%.2f", r.cn0_dBHz);
    ppg_row(&pg, "C/N0",         buf, "dBHz");
    snprintf(buf, sizeof(buf), "%.2f", p.noise_figure_dB);
    ppg_row(&pg, "Noise Figure", buf, "dB");

    /* --- Results section --- */
    pg.y -= 4.0;
    ppg_hline(&pg, 0.102, 0.227, 0.361, 1.5); /* blue rule before results */
    pg.y -= 2.0;
    ppg_section(&pg, "LINK BUDGET RESULTS");
    pg.rh = 17.0; /* taller rows for the result summary */

    snprintf(buf, sizeof(buf), "%.2f", r.req_ebn0_dB);
    ppg_result(&pg, "Eb/N0 Required",  buf, "dB", 0.133, 0.133, 0.133); /* dark grey */
    snprintf(buf, sizeof(buf), "%.2f", r.ebn0_dB);
    ppg_result(&pg, "Eb/N0 Available", buf, "dB", 0.133, 0.133, 0.133);
    snprintf(buf, sizeof(buf), "%+.2f", r.link_margin_dB); /* '+' prefix for positive margins */
    /* Margin colour: green (0.176, 0.490, 0.133) if positive, red (0.753, 0.188, 0.188) if negative */
    double mr = (r.link_margin_dB >= 0.0) ? 0.176 : 0.753;
    double mg = (r.link_margin_dB >= 0.0) ? 0.490 : 0.188;
    double mb = (r.link_margin_dB >= 0.0) ? 0.133 : 0.188;
    ppg_result(&pg, "Link Margin",  buf, "dB", mr, mg, mb);
    /* Status row uses the same green/red colour convention */
    ppg_result(&pg, "Link Status",  r.closed ? "CLOSED" : "OPEN", "",
               r.closed ? 0.176 : 0.753,
               r.closed ? 0.490 : 0.188,
               r.closed ? 0.133 : 0.188);

    /* Footer */
    pg.y -= 6.0;
    ppg_hline(&pg, 0.667, 0.667, 0.667, 0.5); /* thin grey rule */
    pb_printf(&cs,
        "BT /F1 7 Tf 0.533 0.533 0.533 rg 1 0 0 1 %.1f %.1f Tm"
        " (Link Budget Calculator  -  Savronik) Tj ET\n",
        pg.ml + pg.cw / 2.0 - 58.0, pg.y - 10.0);

    /* ---- Assemble the PDF cross-reference structure ----
     * PDF-1.4 requires an xref table with the byte offset of each object.
     * Objects:
     *   1 = Catalog  2 = Pages  3 = Page  4 = Content stream
     *   5 = Font/Helvetica  6 = Font/Helvetica-Bold  7 = Font/Courier */
    PB pdf = {0};
    long offsets[8]; /* offsets[0] unused; xref entry 0 is always the free-list head */

    /* PDF header: the 4 high-bit bytes after the version line signal that the
     * file contains binary data, helping tools detect it as a PDF correctly. */
    pb_printf(&pdf, "%%PDF-1.4\n%%%c%c%c%c\n",
              (char)0xE2, (char)0xE3, (char)0xCF, (char)0xD3);

    offsets[1] = (long)pdf.n;
    pb_printf(&pdf, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    offsets[2] = (long)pdf.n;
    pb_printf(&pdf, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    offsets[3] = (long)pdf.n;
    /* MediaBox [0 0 595 842] = A4 page in points */
    pb_printf(&pdf,
        "3 0 obj\n"
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842]\n"
        "   /Contents 4 0 R\n"
        "   /Resources << /Font << /F1 5 0 R /F2 6 0 R /F3 7 0 R >> >> >>\n"
        "endobj\n");

    offsets[4] = (long)pdf.n;
    pb_printf(&pdf, "4 0 obj\n<< /Length %d >>\nstream\n", cs.n);
    /* Embed the content stream bytes directly (uncompressed, no filter) */
    pb_reserve(&pdf, cs.n + 20);
    memcpy(pdf.d + pdf.n, cs.d, (size_t)cs.n);
    pdf.n += cs.n;
    pb_printf(&pdf, "\nendstream\nendobj\n");

    /* Font objects — reference PDF built-in Type 1 fonts (no embedding needed) */
    offsets[5] = (long)pdf.n;
    pb_printf(&pdf,
        "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
        " /Encoding /WinAnsiEncoding >>\nendobj\n");

    offsets[6] = (long)pdf.n;
    pb_printf(&pdf,
        "6 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold"
        " /Encoding /WinAnsiEncoding >>\nendobj\n");

    offsets[7] = (long)pdf.n;
    pb_printf(&pdf,
        "7 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier"
        " /Encoding /WinAnsiEncoding >>\nendobj\n");

    /* xref table: 9 entries (0 free + objects 1-7 + trailer) */
    long xref_pos = (long)pdf.n;
    pb_printf(&pdf, "xref\n0 8\n");
    /* Entry 0: free-list head; generation 65535 marks it as permanently free */
    pb_printf(&pdf, "0000000000 65535 f\r\n");
    for (int i = 1; i <= 7; i++)
        /* Each xref entry is exactly 20 bytes: 10-digit offset + space + 5-digit gen + space + 'n' + CRLF */
        pb_printf(&pdf, "%010ld 00000 n\r\n", offsets[i]);
    /* Trailer: Root points to the Catalog (object 1); startxref gives xref byte offset */
    pb_printf(&pdf,
        "trailer\n<< /Size 8 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
        xref_pos);

    /* Write the assembled PDF to disk */
    FILE *fp = fopen(path, "wb"); /* binary mode to preserve the exact byte stream */
    if (fp) {
        fwrite(pdf.d, 1, (size_t)pdf.n, fp);
        fclose(fp);
        MessageBoxA(g_hwnd, "PDF report saved successfully.",
                    "PDF Report", MB_ICONINFORMATION | MB_OK);
    } else {
        MessageBoxA(g_hwnd, "Failed to create PDF file.",
                    "PDF Report", MB_ICONERROR | MB_OK);
    }

    pb_free(&cs);
    pb_free(&pdf);
}
