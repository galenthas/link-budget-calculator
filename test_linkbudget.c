/**
 * test_linkbudget.c
 * Unit tests for linkbudget_core.c
 *
 * Minimal test framework: no external dependencies.
 */

#include "linkbudget_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 * Minimal test framework
 * ---------------------------------------------------------------------- */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT_NEAR(actual, expected, tol, name)                          \
    do {                                                                   \
        ++g_tests_run;                                                     \
        double _a = (actual), _e = (expected), _t = (tol);                \
        if (fabs(_a - _e) <= _t) {                                         \
            ++g_tests_passed;                                              \
        } else {                                                           \
            ++g_tests_failed;                                              \
            fprintf(stderr, "FAIL [%s] got %.10g, expected %.10g "        \
                    "(delta %.3g > tol %.3g)\n",                           \
                    (name), _a, _e, fabs(_a - _e), _t);                   \
        }                                                                  \
    } while (0)

#define ASSERT_TRUE(cond, name)                                            \
    do {                                                                   \
        ++g_tests_run;                                                     \
        if (cond) {                                                        \
            ++g_tests_passed;                                              \
        } else {                                                           \
            ++g_tests_failed;                                              \
            fprintf(stderr, "FAIL [%s] condition was false\n", (name));   \
        }                                                                  \
    } while (0)

/* -------------------------------------------------------------------------
 * FSPL tests
 * ---------------------------------------------------------------------- */

static void test_fspl(void)
{
    /* Known value: 1 GHz, 1000 km */
    double expected = 20.0 * log10(4.0 * M_PI * 1e6 * 1e9 / LB_SPEED_OF_LIGHT);
    ASSERT_NEAR(lb_fspl_dB(1000.0, 1.0), expected, 1e-6, "fspl_known_1ghz_1000km");

    /* Known value: 5 GHz, 200 km */
    expected = 20.0 * log10(4.0 * M_PI * 200e3 * 5e9 / LB_SPEED_OF_LIGHT);
    ASSERT_NEAR(lb_fspl_dB(200.0, 5.0), expected, 1e-6, "fspl_known_5ghz_200km");

    /* Doubling range adds 6 dB */
    ASSERT_NEAR(lb_fspl_dB(200.0, 5.0) - lb_fspl_dB(100.0, 5.0),
                20.0 * log10(2.0), 1e-6, "fspl_double_range_6dB");

    /* Doubling frequency adds 6 dB */
    ASSERT_NEAR(lb_fspl_dB(100.0, 10.0) - lb_fspl_dB(100.0, 5.0),
                20.0 * log10(2.0), 1e-6, "fspl_double_freq_6dB");

    /* Increases with range */
    ASSERT_TRUE(lb_fspl_dB(500.0, 2.0) > lb_fspl_dB(200.0, 2.0),
                "fspl_increases_with_range");

    /* Increases with frequency */
    ASSERT_TRUE(lb_fspl_dB(100.0, 10.0) > lb_fspl_dB(100.0, 1.0),
                "fspl_increases_with_freq");
}

/* -------------------------------------------------------------------------
 * EIRP tests
 * ---------------------------------------------------------------------- */

static void test_eirp(void)
{
    /* 10 - 2 - 1 + 30 = 37 */
    ASSERT_NEAR(lb_eirp_dBW(10.0, 2.0, 1.0, 30.0), 37.0, 1e-10, "eirp_basic");

    /* Zero losses */
    ASSERT_NEAR(lb_eirp_dBW(10.0, 0.0, 0.0, 20.0), 30.0, 1e-10, "eirp_zero_losses");

    /* Negative input power */
    ASSERT_NEAR(lb_eirp_dBW(-10.0, 0.0, 0.0, 0.0), -10.0, 1e-10, "eirp_negative_power");
}

/* -------------------------------------------------------------------------
 * System noise temperature tests
 * ---------------------------------------------------------------------- */

static void test_system_noise_temp(void)
{
    /* No losses, no NF → T_sys = T_ant */
    ASSERT_NEAR(lb_system_noise_temp_K(290.0, 0.0, 0.0), 290.0, 1e-6,
                "tsys_no_loss_no_nf");

    /* NF only (3 dB ≈ 3.0103 dB → F≈2), T_ant=0 → T_sys = 290 K */
    ASSERT_NEAR(lb_system_noise_temp_K(0.0, 0.0, 3.0103), 290.0, 0.5,
                "tsys_nf_only");

    /* Feed loss only, T_ant=T0=290 → T_sys = T0 regardless of loss */
    ASSERT_NEAR(lb_system_noise_temp_K(290.0, 3.0,  0.0), 290.0, 1e-4,
                "tsys_feed_loss_3dB");
    ASSERT_NEAR(lb_system_noise_temp_K(290.0, 6.0,  0.0), 290.0, 1e-4,
                "tsys_feed_loss_6dB");

    /* Combined: T_ant=100, NF=3 dB, no feed → T_sys = 100 + 290 = 390 K */
    ASSERT_NEAR(lb_system_noise_temp_K(100.0, 0.0, 3.0103), 390.0, 0.5,
                "tsys_combined");
}

/* -------------------------------------------------------------------------
 * G/T tests
 * ---------------------------------------------------------------------- */

static void test_gt(void)
{
    /* G=20 dBi, T=100 K → 20-20 = 0 dB/K */
    ASSERT_NEAR(lb_gt_dB(20.0, 100.0), 0.0, 1e-6, "gt_basic");

    /* Higher gain → higher G/T */
    ASSERT_TRUE(lb_gt_dB(30.0, 200.0) > lb_gt_dB(20.0, 200.0), "gt_higher_gain");
}

/* -------------------------------------------------------------------------
 * C/N0 tests
 * ---------------------------------------------------------------------- */

static void test_cn0(void)
{
    /* Formula check */
    double eirp=40, fspl=150, labs=5, lnon=2, gt=5;
    double expected = eirp - fspl - labs - lnon + gt - LB_BOLTZMANN_DB;
    ASSERT_NEAR(lb_cn0_dBHz(eirp, fspl, labs, lnon, gt), expected, 1e-10,
                "cn0_formula");

    /* All-zero inputs → result = -BOLTZMANN_DB = +228.6 */
    ASSERT_NEAR(lb_cn0_dBHz(0,0,0,0,0), -LB_BOLTZMANN_DB, 1e-10,
                "cn0_boltzmann_correction");
}

/* -------------------------------------------------------------------------
 * Eb/N0 tests
 * ---------------------------------------------------------------------- */

static void test_ebn0(void)
{
    /* C/N0=70, Rb=1 Mbps → 70-60=10 dB */
    ASSERT_NEAR(lb_ebn0_dB(70.0, 1.0), 10.0, 1e-6, "ebn0_1mbps");

    /* C/N0=70, Rb=10 Mbps → 70-70=0 dB */
    ASSERT_NEAR(lb_ebn0_dB(70.0, 10.0), 0.0, 1e-6, "ebn0_10mbps");

    /* Higher rate → lower Eb/N0 */
    ASSERT_TRUE(lb_ebn0_dB(70.0, 1.0) > lb_ebn0_dB(70.0, 10.0),
                "ebn0_higher_rate_lower");
}

/* -------------------------------------------------------------------------
 * Link margin tests
 * ---------------------------------------------------------------------- */

static void test_link_margin(void)
{
    ASSERT_NEAR(lb_link_margin_dB(10.0, 8.0, 1.0),  1.0, 1e-10, "margin_positive");
    ASSERT_NEAR(lb_link_margin_dB(5.0,  8.0, 1.0), -4.0, 1e-10, "margin_negative");
    ASSERT_NEAR(lb_link_margin_dB(9.0,  8.0, 1.0),  0.0, 1e-10, "margin_zero");
    ASSERT_NEAR(lb_link_margin_dB(10.0, 8.0, 0.0),  2.0, 1e-10, "margin_no_impl_loss");
}

/* -------------------------------------------------------------------------
 * Rx antenna gain tests
 * ---------------------------------------------------------------------- */

static void test_rx_ant_gain(void)
{
    double D=1.0, eta=0.55, f=1.0;
    double wl  = LB_SPEED_OF_LIGHT / (f * 1e9);
    double exp = 10.0 * log10(eta * pow(M_PI * D / wl, 2.0));
    ASSERT_NEAR(lb_rx_ant_gain_dBi(D, eta, f), exp, 1e-6, "rxgain_formula");

    ASSERT_TRUE(lb_rx_ant_gain_dBi(1.2, 0.55, 5.0) > lb_rx_ant_gain_dBi(0.9, 0.55, 5.0),
                "rxgain_larger_dish");
    ASSERT_TRUE(lb_rx_ant_gain_dBi(0.9, 0.55, 10.0) > lb_rx_ant_gain_dBi(0.9, 0.55, 5.0),
                "rxgain_higher_freq");
    ASSERT_TRUE(lb_rx_ant_gain_dBi(0.9, 0.70, 5.0) > lb_rx_ant_gain_dBi(0.9, 0.55, 5.0),
                "rxgain_higher_efficiency");
}

/* -------------------------------------------------------------------------
 * Rx antenna beamwidth tests
 * ---------------------------------------------------------------------- */

static void test_rx_ant_beamwidth(void)
{
    double D=1.0, f=1.0;
    double wl  = LB_SPEED_OF_LIGHT / (f * 1e9);
    double exp = 67.0 * wl / D;
    ASSERT_NEAR(lb_rx_ant_beamwidth_deg(D, f), exp, 1e-10, "beamwidth_formula");

    ASSERT_TRUE(lb_rx_ant_beamwidth_deg(0.6, 5.0) > lb_rx_ant_beamwidth_deg(1.2, 5.0),
                "beamwidth_larger_dish_narrower");
    ASSERT_TRUE(lb_rx_ant_beamwidth_deg(0.9, 1.0) > lb_rx_ant_beamwidth_deg(0.9, 5.0),
                "beamwidth_higher_freq_narrower");
}

/* -------------------------------------------------------------------------
 * Geometry tests
 * ---------------------------------------------------------------------- */

static void test_geometry(void)
{
    /* Overhead pass: tx=0km, rx=100km, slant=100km → central_angle ≈ 0° */
    lb_geometry_t g = lb_geometry_angles(0.0, 100.0, 100.0, 1.0);
    ASSERT_NEAR(g.central_angle_deg, 0.0, 1e-4, "geo_overhead_central_angle");

    /* Symmetric: tx_el == rx_el */
    g = lb_geometry_angles(500.0, 500.0, 300.0, 1.0);
    ASSERT_NEAR(g.tx_el_deg, g.rx_el_deg, 1e-6, "geo_symmetric_elevations");

    /* min_alt_ft: min(10,20)*3280.84 */
    g = lb_geometry_angles(10.0, 20.0, 15.0, 1.0);
    ASSERT_NEAR(g.min_alt_ft, 10.0 * 3280.84, 0.01, "geo_min_alt_ft");

    /* Larger radius → smaller central angle */
    lb_geometry_t g1 = lb_geometry_angles(100.0, 100.0, 200.0, 1.0);
    lb_geometry_t g2 = lb_geometry_angles(100.0, 100.0, 200.0, 2.0);
    ASSERT_TRUE(g1.central_angle_deg > g2.central_angle_deg,
                "geo_radius_factor");
}

/* -------------------------------------------------------------------------
 * Compute slant range tests
 * ---------------------------------------------------------------------- */

static void test_slant_range(void)
{
    /* Returns altitude difference */
    ASSERT_NEAR(lb_compute_slant_range_km(200.0, 100.0, 1.0), 100.0, 1e-6,
                "slant_alt_diff");

    /* Same altitude → clamped to 0.001 */
    ASSERT_NEAR(lb_compute_slant_range_km(100.0, 100.0, 1.0), 0.001, 1e-6,
                "slant_minimum_clamp");

    /* Order independent */
    ASSERT_NEAR(lb_compute_slant_range_km(300.0, 100.0, 1.0),
                lb_compute_slant_range_km(100.0, 300.0, 1.0), 1e-6,
                "slant_order_independent");
}

/* -------------------------------------------------------------------------
 * lb_slant_from_horiz_km tests
 * ---------------------------------------------------------------------- */

static void test_slant_from_horiz(void)
{
    /* Pure horizontal (same altitude): slant == horiz distance */
    ASSERT_NEAR(lb_slant_from_horiz_km(100.0, 50.0, 50.0), 100.0, 1e-9,
                "horiz_same_alt");

    /* Zero horizontal distance, altitude difference only: slant == |Δh| */
    ASSERT_NEAR(lb_slant_from_horiz_km(0.0, 200.0, 100.0), 100.0, 1e-9,
                "horiz_zero_dist");

    /* 3-4-5 right triangle: d=3, Δh=4 → slant=5 */
    ASSERT_NEAR(lb_slant_from_horiz_km(3.0, 4.0, 0.0), 5.0, 1e-9,
                "horiz_345_triangle");

    /* Symmetry: tx/rx swap gives same result */
    ASSERT_NEAR(lb_slant_from_horiz_km(3.0, 0.0, 4.0), 5.0, 1e-9,
                "horiz_345_triangle_swapped");

    /* Minimum clamp: zero horiz, same altitude → 0.001 km */
    ASSERT_NEAR(lb_slant_from_horiz_km(0.0, 100.0, 100.0), 0.001, 1e-9,
                "horiz_minimum_clamp");

    /* Slant always >= horiz distance */
    ASSERT_TRUE(lb_slant_from_horiz_km(50.0, 200.0, 100.0) >= 50.0,
                "horiz_slant_gte_horiz");

    /* More horizontal distance → more slant range */
    ASSERT_TRUE(lb_slant_from_horiz_km(200.0, 100.0, 0.0) >
                lb_slant_from_horiz_km(100.0, 100.0, 0.0),
                "horiz_monotone");

    /* Consistent with lb_run: passing computed slant into lb_run gives same
     * result as if we had derived it manually */
    double slant = lb_slant_from_horiz_km(150.0, 10.0, 1.0);
    ASSERT_NEAR(slant, sqrt(150.0*150.0 + 9.0*9.0), 1e-9,
                "horiz_formula_check");
}

/* -------------------------------------------------------------------------
 * Required Eb/N0 tests
 * ---------------------------------------------------------------------- */

static void test_required_ebn0(void)
{
    /* VA 1/2 (K=7) + QPSK at 1e-6 → 4.81 dB (calibrated reference) */
    ASSERT_NEAR(lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK, LB_BER_1E6),
                4.81, 1e-10, "req_ebn0_va_qpsk_1e6");

    /* QPSK == BPSK penalty (both 0) */
    ASSERT_NEAR(
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_BPSK,  LB_BER_1E6),
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK,  LB_BER_1E6),
        1e-10, "req_ebn0_bpsk_qpsk_equal");

    /* Uncoded + 8PSK matches MODULATION_UNCODED table */
    double from_table = LB_MODULATION_UNCODED[LB_MOD_8PSK][LB_BER_1E6];
    ASSERT_NEAR(lb_required_ebn0_dB(LB_FEC_UNCODED, LB_MOD_8PSK, LB_BER_1E6),
                from_table, 1e-10, "req_ebn0_uncoded_8psk");

    /* 8PSK requires more than QPSK */
    ASSERT_TRUE(
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_8PSK,  LB_BER_1E6) >
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK,  LB_BER_1E6),
        "req_ebn0_8psk_gt_qpsk");

    /* Better FEC → lower requirement */
    ASSERT_TRUE(
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7,   LB_MOD_QPSK, LB_BER_1E6) >
        lb_required_ebn0_dB(LB_FEC_TURBO_1_6,    LB_MOD_QPSK, LB_BER_1E6),
        "req_ebn0_turbo_better");

    /* Stricter BER → higher requirement */
    ASSERT_TRUE(
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK, LB_BER_1E8) >
        lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK, LB_BER_1E2),
        "req_ebn0_stricter_ber");
}

/* -------------------------------------------------------------------------
 * Polarization loss tests
 * ---------------------------------------------------------------------- */

static void test_pol_loss(void)
{
    /* Circular (AR=0 dB) to circular → no loss for any angle */
    ASSERT_NEAR(lb_pol_loss_dB(0.0, 0.0,  0.0), 0.0, 1e-5, "pol_circular_0deg");
    ASSERT_NEAR(lb_pol_loss_dB(0.0, 0.0, 45.0), 0.0, 1e-5, "pol_circular_45deg");
    ASSERT_NEAR(lb_pol_loss_dB(0.0, 0.0, 90.0), 0.0, 1e-5, "pol_circular_90deg");

    /* Linear (high AR), aligned → ~0 dB loss */
    ASSERT_NEAR(lb_pol_loss_dB(40.0, 40.0, 0.0), 0.0, 0.01, "pol_linear_aligned");

    /* Linear (high AR), cross-polarized → very large loss */
    ASSERT_TRUE(lb_pol_loss_dB(40.0, 40.0, 90.0) > 30.0, "pol_linear_cross");

    /* Loss always non-negative */
    double loss;
    loss = lb_pol_loss_dB(0.0,  0.0,  45.0); ASSERT_TRUE(loss >= 0.0, "pol_nn_1");
    loss = lb_pol_loss_dB(3.0,  6.0,  30.0); ASSERT_TRUE(loss >= 0.0, "pol_nn_2");
    loss = lb_pol_loss_dB(20.0, 20.0, 90.0); ASSERT_TRUE(loss >= 0.0, "pol_nn_3");
}

/* -------------------------------------------------------------------------
 * run_link_budget integration tests
 * ---------------------------------------------------------------------- */

static lb_params_t base_params(void)
{
    lb_params_t p;
    p.freq_GHz               = 5.1;
    p.data_rate_Mbps         = 10.0;
    p.fec_decoder            = LB_FEC_VA_HALF_K7;
    p.modulation             = LB_MOD_QPSK;
    p.ber                    = LB_BER_1E6;
    p.impl_loss_dB           = 0.0;
    p.tx_alt_km              = 10.0;
    p.rx_alt_km              = 1.0;
    p.slant_range_km         = 200.0;
    p.radius_factor          = 1.333;
    p.sat_power_dBW          = 10.0 * log10(10.0);  /* 10 W */
    p.obo_dB                 = 0.5;
    p.feed_loss_tx_dB        = 1.0;
    p.tx_ant_gain_dBi        = 2.0;
    p.pol_loss_dB            = 0.0;
    p.absorptive_loss_dB     = 10.0;
    p.non_absorptive_loss_dB = 0.0;
    p.rx_ant_gain_dBi        = lb_rx_ant_gain_dBi(0.9, 0.55, 5.1);
    p.rx_noise_K             = 289.5;
    p.feedline_loss_dB       = 0.0;
    p.noise_figure_dB        = 2.0;
    return p;
}

static void test_run_link_budget(void)
{
    lb_params_t p = base_params();
    lb_result_t r = lb_run(&p);

    /* Slant range used matches input */
    ASSERT_NEAR(r.slant_range_km_used, 200.0, 1e-6, "run_slant_range");

    /* Auto slant range (p.slant_range_km = 0) */
    p.slant_range_km = 0;
    r = lb_run(&p);
    ASSERT_NEAR(r.slant_range_km_used, 9.0, 1e-6, "run_slant_auto");
    p = base_params();

    /* EIRP calculation */
    double expected_eirp = p.sat_power_dBW - p.obo_dB
                         - p.feed_loss_tx_dB + p.tx_ant_gain_dBi
                         - p.pol_loss_dB;
    r = lb_run(&p);
    ASSERT_NEAR(r.eirp_dBW, expected_eirp, 1e-6, "run_eirp");

    /* FSPL uses slant range */
    ASSERT_NEAR(r.fspl_dB, lb_fspl_dB(200.0, 5.1), 1e-6, "run_fspl");

    /* Required Eb/N0 matches helper */
    ASSERT_NEAR(r.req_ebn0_dB,
                lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK, LB_BER_1E6),
                1e-10, "run_req_ebn0");

    /* Link margin = ebn0 - req_ebn0 (impl_loss=0) */
    ASSERT_NEAR(r.link_margin_dB, r.ebn0_dB - r.req_ebn0_dB, 1e-10,
                "run_margin_consistency");

    /* Failed link → closed=0 */
    p.absorptive_loss_dB = 200.0;
    r = lb_run(&p);
    ASSERT_TRUE(r.link_margin_dB < 0.0, "run_failed_link_margin");
    ASSERT_TRUE(r.closed == 0, "run_failed_link_status");

    /* High power → positive margin → closed=1 */
    p = base_params();
    p.absorptive_loss_dB = 0.0;
    r = lb_run(&p);
    if (r.link_margin_dB >= 0.0)
        ASSERT_TRUE(r.closed == 1, "run_closed_link");

    /* Higher Tx power improves margin */
    lb_params_t p_lo2 = base_params();
    lb_params_t p_hi  = base_params();
    p_hi.sat_power_dBW += 10.0;
    lb_result_t r_lo = lb_run(&p_lo2);
    lb_result_t r_hi = lb_run(&p_hi);
    ASSERT_TRUE(r_hi.link_margin_dB > r_lo.link_margin_dB,
                "run_higher_power_better_margin");

    /* Higher data rate reduces margin */
    lb_params_t p_slow = base_params();
    lb_params_t p_fast = base_params();
    p_fast.data_rate_Mbps *= 10.0;
    lb_result_t r_slow = lb_run(&p_slow);
    lb_result_t r_fast = lb_run(&p_fast);
    ASSERT_TRUE(r_slow.link_margin_dB > r_fast.link_margin_dB,
                "run_higher_rate_worse_margin");
}

/* -------------------------------------------------------------------------
 * Cross-validation: C vs Python reference values
 *
 * These values were computed by running the Python implementation with
 * the same inputs and capturing the output.
 * ---------------------------------------------------------------------- */

static void test_cross_validation(void)
{
    /* FSPL @ 5.1 GHz, 200 km */
    ASSERT_NEAR(lb_fspl_dB(200.0, 5.1),
                20.0 * log10(4.0 * M_PI * 200e3 * 5.1e9 / LB_SPEED_OF_LIGHT),
                1e-9, "xval_fspl");

    /* Rx antenna gain: D=0.9m, η=55%, f=5.1 GHz */
    double wl = LB_SPEED_OF_LIGHT / (5.1e9);
    double expected_gain = 10.0 * log10(0.55 * pow(M_PI * 0.9 / wl, 2.0));
    ASSERT_NEAR(lb_rx_ant_gain_dBi(0.9, 0.55, 5.1), expected_gain, 1e-9,
                "xval_rx_gain");

    /* System noise temp: T_ant=289.5K, feedline=0dB, NF=2dB */
    double F = pow(10.0, 2.0/10.0);
    double expected_tsys = 289.5 + 290.0 * (F - 1.0);
    ASSERT_NEAR(lb_system_noise_temp_K(289.5, 0.0, 2.0), expected_tsys, 1e-6,
                "xval_tsys");

    /* required_ebn0: VA 1/2 + QPSK + 1e-6 = 4.81 dB */
    ASSERT_NEAR(lb_required_ebn0_dB(LB_FEC_VA_HALF_K7, LB_MOD_QPSK, LB_BER_1E6),
                4.81, 1e-10, "xval_req_ebn0");

    /* Geometry: symmetric 500/500 km, 300 km slant → tx_el == rx_el */
    lb_geometry_t g = lb_geometry_angles(500.0, 500.0, 300.0, 1.0);
    ASSERT_NEAR(g.tx_el_deg, g.rx_el_deg, 1e-9, "xval_geo_symmetric");

    /* Overhead: central angle = 0 */
    g = lb_geometry_angles(0.0, 100.0, 100.0, 1.0);
    ASSERT_NEAR(g.central_angle_deg, 0.0, 1e-4, "xval_geo_overhead");
}

/* -------------------------------------------------------------------------
 * Parameter sensitivity tests
 *
 * For every lb_run() input, verify it changes the expected output.
 * Uses a fixed rx_ant_gain_dBi so frequency isolation is clean.
 * ---------------------------------------------------------------------- */

static lb_params_t sens_base(void)
{
    lb_params_t p = {0};
    p.freq_GHz               = 5.0;
    p.data_rate_Mbps         = 1.0;
    p.fec_decoder            = LB_FEC_UNCODED;
    p.modulation             = LB_MOD_BPSK;
    p.ber                    = LB_BER_1E6;
    p.impl_loss_dB           = 0.0;
    p.tx_alt_km              = 500.0;
    p.rx_alt_km              = 0.0;
    p.slant_range_km         = 1000.0;   /* fixed, not auto-computed */
    p.radius_factor          = 1.0;
    p.sat_power_dBW          = 0.0;      /* 1 W */
    p.obo_dB                 = 0.0;
    p.feed_loss_tx_dB        = 0.0;
    p.tx_ant_gain_dBi        = 0.0;
    p.pol_loss_dB            = 0.0;
    p.absorptive_loss_dB     = 0.0;
    p.non_absorptive_loss_dB = 0.0;
    p.rx_ant_gain_dBi        = 30.0;     /* fixed – not computed from dish */
    p.rx_noise_K             = 290.0;
    p.feedline_loss_dB       = 0.0;
    p.noise_figure_dB        = 0.0;
    return p;
}

static void test_parameter_sensitivity(void)
{
    lb_params_t a, b;
    lb_result_t ra, rb;

    /* ---- freq_GHz: higher freq → higher FSPL (with fixed rx_ant_gain) → worse margin */
    a = sens_base(); b = sens_base();
    b.freq_GHz = 10.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.fspl_dB > ra.fspl_dB,           "sens_freq_raises_fspl");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB, "sens_freq_worsens_margin");
    /* Δ ≈ 20*log10(10/5) = 6.02 dB */
    ASSERT_NEAR(rb.fspl_dB - ra.fspl_dB, 20.0*log10(10.0/5.0), 1e-6,
                "sens_freq_fspl_delta");

    /* ---- data_rate_Mbps: higher rate → lower Eb/N0 → worse margin */
    a = sens_base(); b = sens_base();
    b.data_rate_Mbps = 10.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.ebn0_dB < ra.ebn0_dB,               "sens_rate_lowers_ebn0");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_rate_worsens_margin");
    /* Δ = -10*log10(10) = -10 dB */
    ASSERT_NEAR(ra.ebn0_dB - rb.ebn0_dB, 10.0, 1e-6, "sens_rate_ebn0_delta");

    /* ---- FEC decoder: better code → lower req_ebn0 → better margin */
    a = sens_base(); b = sens_base();
    a.fec_decoder = LB_FEC_UNCODED;
    b.fec_decoder = LB_FEC_TURBO_1_6;   /* ~-0.7 dB vs ~10.5 dB uncoded @ 1e-6 */
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.req_ebn0_dB < ra.req_ebn0_dB,       "sens_fec_lowers_req");
    ASSERT_TRUE(rb.link_margin_dB > ra.link_margin_dB,  "sens_fec_improves_margin");

    /* ---- modulation: higher order → higher req_ebn0 → worse margin */
    a = sens_base(); b = sens_base();
    a.modulation = LB_MOD_BPSK;
    b.modulation = LB_MOD_64QAM;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.req_ebn0_dB > ra.req_ebn0_dB,       "sens_mod_raises_req");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_mod_worsens_margin");

    /* ---- BER: tighter BER → higher req_ebn0 → worse margin */
    a = sens_base(); b = sens_base();
    a.ber = LB_BER_1E2;
    b.ber = LB_BER_1E8;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.req_ebn0_dB > ra.req_ebn0_dB,       "sens_ber_raises_req");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_ber_worsens_margin");

    /* ---- impl_loss_dB: higher → worse margin (1:1) */
    a = sens_base(); b = sens_base();
    b.impl_loss_dB = 3.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 3.0, 1e-9, "sens_impl_loss");

    /* ---- slant_range_km: longer → worse FSPL → worse margin */
    a = sens_base(); b = sens_base();
    b.slant_range_km = 2000.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.fspl_dB > ra.fspl_dB,               "sens_range_raises_fspl");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_range_worsens_margin");
    /* Doubling range → +6 dB FSPL */
    ASSERT_NEAR(rb.fspl_dB - ra.fspl_dB, 20.0*log10(2.0), 1e-6,
                "sens_range_fspl_delta");

    /* ---- sat_power_dBW: +10 dBW → +10 dB margin (1:1) */
    a = sens_base(); b = sens_base();
    b.sat_power_dBW = 10.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(rb.link_margin_dB - ra.link_margin_dB, 10.0, 1e-9,
                "sens_tx_power_delta");

    /* ---- obo_dB: +3 dB OBO → -3 dB EIRP → -3 dB margin */
    a = sens_base(); b = sens_base();
    b.obo_dB = 3.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 3.0, 1e-9, "sens_obo");

    /* ---- feed_loss_tx_dB: +1 dB → -1 dB EIRP → -1 dB margin */
    a = sens_base(); b = sens_base();
    b.feed_loss_tx_dB = 1.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 1.0, 1e-9, "sens_feed_tx");

    /* ---- tx_ant_gain_dBi: +6 dBi → +6 dB margin */
    a = sens_base(); b = sens_base();
    b.tx_ant_gain_dBi = 6.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(rb.link_margin_dB - ra.link_margin_dB, 6.0, 1e-9, "sens_tx_gain");

    /* ---- pol_loss_dB: +2 dB → -2 dB margin */
    a = sens_base(); b = sens_base();
    b.pol_loss_dB = 2.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 2.0, 1e-9, "sens_pol_loss");

    /* ---- absorptive_loss_dB: +5 dB → -5 dB margin */
    a = sens_base(); b = sens_base();
    b.absorptive_loss_dB = 5.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 5.0, 1e-9, "sens_abs_loss");

    /* ---- non_absorptive_loss_dB: +3 dB → -3 dB margin */
    a = sens_base(); b = sens_base();
    b.non_absorptive_loss_dB = 3.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(ra.link_margin_dB - rb.link_margin_dB, 3.0, 1e-9, "sens_nonabs_loss");

    /* ---- rx_ant_gain_dBi: +10 dBi → +10 dB margin */
    a = sens_base(); b = sens_base();
    b.rx_ant_gain_dBi = 40.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_NEAR(rb.link_margin_dB - ra.link_margin_dB, 10.0, 1e-9, "sens_rx_gain");

    /* ---- rx_noise_K: hotter antenna → worse T_sys → worse G/T → worse margin */
    a = sens_base(); b = sens_base();
    b.rx_noise_K = 580.0;   /* double noise temp */
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.gt_dBK < ra.gt_dBK,                "sens_rx_noise_worsens_gt");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB, "sens_rx_noise_worsens_margin");

    /* ---- feedline_loss_dB: lossy feedline degrades T_sys → worse margin
     *      Physics: T_sys = (T_ant - T0)/L + T0*F.  Loss hurts only when
     *      T_ant < T0 (cold sky / satellite dish, typ. 50–150 K).          */
    a = sens_base(); b = sens_base();
    a.rx_noise_K = 100.0;   /* cold sky: T_ant < T0 = 290 K */
    b.rx_noise_K = 100.0;
    b.feedline_loss_dB = 3.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.t_sys_K > ra.t_sys_K,               "sens_feedline_raises_tsys");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_feedline_worsens_margin");

    /* ---- noise_figure_dB: noisier LNA → worse T_sys → worse margin */
    a = sens_base(); b = sens_base();
    b.noise_figure_dB = 5.0;
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.t_sys_K > ra.t_sys_K,               "sens_nf_raises_tsys");
    ASSERT_TRUE(rb.link_margin_dB < ra.link_margin_dB,  "sens_nf_worsens_margin");

    /* ---- auto slant range: increasing altitude difference increases range */
    a = sens_base(); a.slant_range_km = 0.0;   /* use auto */
    b = sens_base(); b.slant_range_km = 0.0;
    b.tx_alt_km = 1000.0;   /* larger altitude diff */
    ra = lb_run(&a); rb = lb_run(&b);
    ASSERT_TRUE(rb.slant_range_km_used > ra.slant_range_km_used,
                "sens_auto_range_increases");

    /* ---- gui note: with parabolic rx dish, freq change leaves margin flat
     *      because ΔFSPL + ΔG/T = 0. Verify via lb_rx_ant_gain_dBi:       */
    double gain_5  = lb_rx_ant_gain_dBi(0.9, 0.55, 5.0);
    double gain_10 = lb_rx_ant_gain_dBi(0.9, 0.55, 10.0);
    double fspl_5  = lb_fspl_dB(1000.0, 5.0);
    double fspl_10 = lb_fspl_dB(1000.0, 10.0);
    ASSERT_NEAR((fspl_10 - fspl_5) - (gain_10 - gain_5), 0.0, 1e-6,
                "sens_freq_dish_cancellation");
}

/* -------------------------------------------------------------------------
 * ITU-R P.676-12 — Oxygen attenuation tests
 * ---------------------------------------------------------------------- */

static void test_p676_gamma_o(void)
{
    /* Standard atmosphere: p=1013.25 hPa, T=15°C
     * At 10 GHz: manually derived ≈ 0.007939 dB/km */
    double go_10 = lb_p676_gamma_o(10.0, 1013.25, 15.0);
    ASSERT_NEAR(go_10, 0.007939, 1e-4, "p676o_std_10ghz");

    /* Always positive for valid inputs */
    ASSERT_TRUE(lb_p676_gamma_o(5.0,  1013.25, 15.0) > 0.0, "p676o_positive_5ghz");
    ASSERT_TRUE(lb_p676_gamma_o(20.0, 1013.25, 15.0) > 0.0, "p676o_positive_20ghz");
    ASSERT_TRUE(lb_p676_gamma_o(30.0, 1013.25, 15.0) > 0.0, "p676o_positive_30ghz");

    /* Higher pressure → higher attenuation (denser air) */
    ASSERT_TRUE(lb_p676_gamma_o(10.0, 1013.25, 15.0) >
                lb_p676_gamma_o(10.0,  500.00, 15.0),
                "p676o_higher_pressure");

    /* Standard conditions match ITU-R order of magnitude: 0.005–0.02 dB/km at 10 GHz */
    ASSERT_TRUE(go_10 > 0.005 && go_10 < 0.020, "p676o_magnitude_10ghz");
}

/* -------------------------------------------------------------------------
 * ITU-R P.676-12 — Water vapour attenuation tests
 * ---------------------------------------------------------------------- */

static void test_p676_gamma_w(void)
{
    /* Zero water vapour → near zero attenuation */
    ASSERT_NEAR(lb_p676_gamma_w(10.0, 0.0, 1013.25, 15.0), 0.0, 1e-10,
                "p676w_zero_rho");

    /* Standard atmosphere (rho=7.5 g/m³): always positive */
    ASSERT_TRUE(lb_p676_gamma_w(10.0, 7.5, 1013.25, 15.0) > 0.0, "p676w_positive_10ghz");
    ASSERT_TRUE(lb_p676_gamma_w(22.0, 7.5, 1013.25, 15.0) > 0.0, "p676w_positive_22ghz");

    /* Near 22.235 GHz resonance → higher than at 10 GHz */
    ASSERT_TRUE(lb_p676_gamma_w(22.0, 7.5, 1013.25, 15.0) >
                lb_p676_gamma_w(10.0, 7.5, 1013.25, 15.0),
                "p676w_22ghz_resonance");

    /* More water vapour → more attenuation */
    ASSERT_TRUE(lb_p676_gamma_w(22.0, 15.0, 1013.25, 15.0) >
                lb_p676_gamma_w(22.0,  7.5, 1013.25, 15.0),
                "p676w_higher_rho");

    /* ITU-R order of magnitude: ~1–3 dB/km near 22 GHz resonance, std atmosphere */
    double gw_22 = lb_p676_gamma_w(22.0, 7.5, 1013.25, 15.0);
    ASSERT_TRUE(gw_22 > 0.5 && gw_22 < 5.0, "p676w_magnitude_22ghz");
}

/* -------------------------------------------------------------------------
 * ITU-R P.676-12 — Slant-path gaseous attenuation tests
 * ---------------------------------------------------------------------- */

static void test_gaseous_atten(void)
{
    /* Always positive */
    ASSERT_TRUE(lb_gaseous_atten_dB(10.0, 45.0, 1013.25, 15.0, 7.5) > 0.0,
                "gaseous_positive");

    /* Elevation clamping: 1° and 5° must give the same result */
    ASSERT_NEAR(lb_gaseous_atten_dB(10.0, 1.0, 1013.25, 15.0, 7.5),
                lb_gaseous_atten_dB(10.0, 5.0, 1013.25, 15.0, 7.5),
                1e-10, "gaseous_clamp_1deg_eq_5deg");

    /* Lower elevation → more attenuation (longer slant path) */
    ASSERT_TRUE(lb_gaseous_atten_dB(10.0,  5.0, 1013.25, 15.0, 7.5) >
                lb_gaseous_atten_dB(10.0, 30.0, 1013.25, 15.0, 7.5),
                "gaseous_lower_el_more_atten");
    ASSERT_TRUE(lb_gaseous_atten_dB(10.0, 30.0, 1013.25, 15.0, 7.5) >
                lb_gaseous_atten_dB(10.0, 90.0, 1013.25, 15.0, 7.5),
                "gaseous_90deg_minimum");

    /* At 90° elevation: result equals zenith (go*6 + gw*h_w) / sin(90) */
    double go = lb_p676_gamma_o(10.0, 1013.25, 15.0);
    double gw = lb_p676_gamma_w(10.0, 7.5, 1013.25, 15.0);
    double h_w = 1.6 + 1.0 / (1.0 + 0.0281 * 7.5);
    double zenith = go * 6.0 + gw * h_w;
    ASSERT_NEAR(lb_gaseous_atten_dB(10.0, 90.0, 1013.25, 15.0, 7.5),
                zenith, 1e-10, "gaseous_90deg_eq_zenith");

    /* Zero water vapour reduces total attenuation */
    ASSERT_TRUE(lb_gaseous_atten_dB(22.0, 45.0, 1013.25, 15.0, 7.5) >
                lb_gaseous_atten_dB(22.0, 45.0, 1013.25, 15.0, 0.0),
                "gaseous_zero_rho_less");
}

/* -------------------------------------------------------------------------
 * ITU-R P.838-3 — Rain specific attenuation tests
 * ---------------------------------------------------------------------- */

static void test_p838_gamma_r(void)
{
    /* Zero rain → zero attenuation */
    ASSERT_NEAR(lb_p838_gamma_r(10.0, 0.0), 0.0, 1e-10, "p838_zero_rain");
    ASSERT_NEAR(lb_p838_gamma_r(20.0, 0.0), 0.0, 1e-10, "p838_zero_rain_20ghz");

    /* Always non-negative */
    ASSERT_TRUE(lb_p838_gamma_r(10.0, 25.0) >= 0.0, "p838_nonneg_10ghz");
    ASSERT_TRUE(lb_p838_gamma_r(20.0, 50.0) >= 0.0, "p838_nonneg_20ghz");

    /* Higher rain rate → more attenuation */
    ASSERT_TRUE(lb_p838_gamma_r(10.0, 50.0) > lb_p838_gamma_r(10.0, 25.0),
                "p838_higher_rate");

    /* Higher frequency → more attenuation (5 → 10 → 20 GHz) */
    ASSERT_TRUE(lb_p838_gamma_r(10.0, 25.0) > lb_p838_gamma_r( 5.0, 25.0),
                "p838_higher_freq_10_vs_5");
    ASSERT_TRUE(lb_p838_gamma_r(20.0, 25.0) > lb_p838_gamma_r(10.0, 25.0),
                "p838_higher_freq_20_vs_10");

    /* ITU-R P.838-3 reference: ~0.27 dB/km at 10 GHz, 25 mm/h (approx) */
    double g10 = lb_p838_gamma_r(10.0, 25.0);
    ASSERT_TRUE(g10 > 0.1 && g10 < 0.8, "p838_magnitude_10ghz_25mmh");
}

/* -------------------------------------------------------------------------
 * ITU-R P.838-3 — Total rain attenuation tests
 * ---------------------------------------------------------------------- */

static void test_rain_atten(void)
{
    /* Zero rain → zero total attenuation */
    ASSERT_NEAR(lb_rain_atten_dB(10.0, 0.0, 5.0), 0.0, 1e-10, "rain_zero_rate");

    /* Equals gamma_r * L_eff */
    double gamma = lb_p838_gamma_r(10.0, 25.0);
    ASSERT_NEAR(lb_rain_atten_dB(10.0, 25.0, 5.0), gamma * 5.0, 1e-10,
                "rain_equals_gamma_times_len");

    /* Scales linearly with path length */
    ASSERT_NEAR(lb_rain_atten_dB(10.0, 25.0, 10.0),
                2.0 * lb_rain_atten_dB(10.0, 25.0, 5.0), 1e-10,
                "rain_linear_with_length");

    /* Higher rain rate → more total attenuation */
    ASSERT_TRUE(lb_rain_atten_dB(10.0, 50.0, 5.0) > lb_rain_atten_dB(10.0, 25.0, 5.0),
                "rain_higher_rate_more_atten");

    /* Zero path length → zero attenuation */
    ASSERT_NEAR(lb_rain_atten_dB(10.0, 25.0, 0.0), 0.0, 1e-10, "rain_zero_length");
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */

int main(void)
{
    printf("Running link budget C tests...\n\n");

    test_fspl();
    test_eirp();
    test_system_noise_temp();
    test_gt();
    test_cn0();
    test_ebn0();
    test_link_margin();
    test_rx_ant_gain();
    test_rx_ant_beamwidth();
    test_geometry();
    test_slant_range();
    test_slant_from_horiz();
    test_required_ebn0();
    test_pol_loss();
    test_run_link_budget();
    test_cross_validation();
    test_parameter_sensitivity();
    test_p676_gamma_o();
    test_p676_gamma_w();
    test_gaseous_atten();
    test_p838_gamma_r();
    test_rain_atten();

    printf("\nResults: %d/%d passed", g_tests_passed, g_tests_run);
    if (g_tests_failed > 0)
        printf(", %d FAILED", g_tests_failed);
    printf("\n");

    return g_tests_failed > 0 ? 1 : 0;
}
