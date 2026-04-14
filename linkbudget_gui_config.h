#ifndef LINKBUDGET_GUI_CONFIG_H
#define LINKBUDGET_GUI_CONFIG_H

/* ============================================================
 * Layout constants  (all in pixels, client-area coordinates)
 * ============================================================ */
#define WIN_W       1020
#define WIN_H        690
#define BANNER_H      38
#define GRID_Y        44   /* BANNER_H + 6 */
#define MARGIN_X       8
#define SEC_GAP_X      5
#define SEC_GAP_Y      6
#define COL_W        247
/* col x-origins */
#define COL_X0   (MARGIN_X)
#define COL_X1   (MARGIN_X + COL_W + SEC_GAP_X)
#define COL_X2   (MARGIN_X + 2*(COL_W + SEC_GAP_X))
#define COL_X3   (MARGIN_X + 3*(COL_W + SEC_GAP_X))
#define COL_W3   (WIN_W - MARGIN_X - COL_X3)   /* last col slightly wider */
#define ROW0_H   234
#define ROW1_Y   (GRID_Y + ROW0_H + SEC_GAP_Y)
#define ROW1_H   260
#define BTN_Y    (ROW1_Y + ROW1_H + SEC_GAP_Y)
#define BTN_H     36
#define RES_Y    (BTN_Y + BTN_H + SEC_GAP_Y)
#define RES_H     90
#define HDR_H     28   /* section/panel header strip height */
/* within a section panel (client coords, origin = top-left of panel) */
#define SP_PAD_TOP  4
#define SP_LBL_X    8
#define SP_LBL_W  115
#define SP_EDIT_X 126
#define SP_EDIT_W  80
#define SP_UNIT_X 208
#define SP_UNIT_W  36
#define ROW_H_IN   26   /* input row height */
#define ROW_H_CALC 22   /* calculated-value row height */

/* ============================================================
 * Colors
 * ============================================================ */
#define C_BG        RGB(0x1E,0x21,0x27)
#define C_PANEL     RGB(0x28,0x2C,0x34)
#define C_BORDER    RGB(0x3E,0x44,0x51)
#define C_HDR_BG    RGB(0x2C,0x31,0x3A)
#define C_HDR_FG    RGB(0x61,0xAF,0xEF)
#define C_LABEL     RGB(0xAB,0xB2,0xBF)
#define C_ENTRY_BG  RGB(0x1E,0x21,0x27)
#define C_ENTRY_FG  RGB(0xE5,0xC0,0x7B)
#define C_UNIT      RGB(0x5C,0x63,0x70)
#define C_CALC_FG   RGB(0xE0,0x6C,0x75)
#define C_RES_BG    RGB(0x21,0x25,0x2B)
#define C_RES_LABEL RGB(0x56,0xB6,0xC2)
#define C_POS       RGB(0x98,0xC3,0x79)
#define C_NEG       RGB(0xE0,0x6C,0x75)
#define C_BTN_BG    RGB(0x98,0xC3,0x79)
#define C_BTN_FG    RGB(0x1E,0x21,0x27)
#define C_BTN_HOT   RGB(0x7C,0xB7,0x68)

/* ============================================================
 * Control ID categories (used in WM_CTLCOLORSTATIC)
 * ============================================================ */
#define ID_LBL   100   /* section label */
#define ID_UNIT  101   /* unit label    */
#define ID_CALC  200   /* calc display  */
#define ID_RLBL  300   /* result label  */
#define ID_RVAL  301   /* result value (req/ebn0) */
#define ID_MVAL  302   /* margin value (colour changes) */
#define ID_MSTAT 303   /* status text (same colour as margin) */
#define ID_BTN        1   /* calculate button */
#define ID_BTN_SAVE   2
#define ID_BTN_LOAD   3
#define ID_BTN_SWEEP  4
#define ID_BTN_EXPORT 5
#define ID_BTN_PRINT  6
/* Clickable unit IDs */
#define ID_UNIT_FREQ    400
#define ID_UNIT_TXALT   401
#define ID_UNIT_RXALT   402
#define ID_UNIT_SATPOW  403
#define ID_UNIT_FEEDPOW 404
#define ID_UNIT_RXPOW   405
#define ID_UNIT_MINALT  406
#define ID_UNIT_EIRP    407

/* ============================================================
 * Combo box selection indices
 * ============================================================ */
#define RANGE_MODE_HORIZ  1   /* h_range_mode -> Horiz. Dist. */
#define ANT_MODE_DISH     1   /* h_tx_mode / h_rx_mode -> Parabolic Dish */
#define LOSS_MODE_ITU     1   /* h_loss_mode -> ITU-R (P.676 + P.838) */

/* ============================================================
 * ITU-R standard atmosphere (P.676-12 Annex 1)
 * ============================================================ */
#define ITU_STD_P_HPA    1013.25   /* pressure (hPa) */
#define ITU_STD_T_C        15.0    /* temperature (deg C) */
#define ITU_STD_RHO_GM3     7.5    /* water vapour density (g/m^3) */

/* Helper: convert an array count to int */
#define NELEM(a) ((int)(sizeof(a)/sizeof((a)[0])))

#endif /* LINKBUDGET_GUI_CONFIG_H */
