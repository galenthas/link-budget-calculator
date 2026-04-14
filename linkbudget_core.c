/*
 * linkbudget_core.c — Physics engine for the link budget calculator.
 * Contains all RF/propagation math: gains, noise, path loss, geometry,
 * and ITU-R P.676-12 / P.838-3 atmospheric models.
 */

#include "linkbudget_core.h"
#include <math.h>
#include <float.h>

/* -------------------------------------------------------------------------
 * Lookup tables
 * ---------------------------------------------------------------------- */

/* Required Eb/N0 (dB) per FEC scheme for BPSK/QPSK modulation.
 * Columns: [1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8]
 * Calibrated: VA 1/2 (K=7) → 4.81 dB @ BER 1e-6. */
const double LB_FEC_BPSK_TABLE[LB_FEC_COUNT][LB_BER_COUNT] = {
    /* LB_FEC_UNCODED         */ {  4.3,  6.8,  8.4,  9.6, 10.5, 11.3, 12.0 },
    /* LB_FEC_VA_HALF_K7      */ {  2.0,  3.0,  3.8,  4.3,  4.81, 5.3,  5.7 },
    /* LB_FEC_VA_3Q_K7        */ {  2.9,  4.0,  4.9,  5.5,  6.0,  6.4,  6.8 },
    /* LB_FEC_VA_7E8_K7       */ {  3.8,  5.1,  6.2,  7.0,  7.6,  8.1,  8.5 },
    /* LB_FEC_RS_VA_HALF      */ {  0.8,  1.2,  1.5,  1.8,  2.0,  2.2,  2.4 },
    /* LB_FEC_TURBO_1_6       */ { -2.0, -1.4, -1.1, -0.9, -0.7, -0.6, -0.5 },
    /* LB_FEC_TURBO_1_3       */ { -1.2, -0.5, -0.2,  0.0,  0.2,  0.3,  0.4 },
    /* LB_FEC_TURBO_1_2       */ { -0.5,  0.2,  0.5,  0.7,  0.9,  1.0,  1.1 },
    /* LB_FEC_TURBO_3_4       */ {  1.2,  1.8,  2.2,  2.5,  2.7,  2.9,  3.0 },
    /* LB_FEC_LDPC_1_2        */ { -0.3,  0.3,  0.6,  0.8,  0.95, 1.05, 1.15},
    /* LB_FEC_LDPC_2_3        */ {  0.8,  1.4,  1.7,  1.9,  2.1,  2.2,  2.3 },
    /* LB_FEC_LDPC_3_4        */ {  1.4,  2.0,  2.3,  2.5,  2.7,  2.85, 2.95},
    /* LB_FEC_LDPC_5_6        */ {  2.4,  3.0,  3.4,  3.7,  3.9,  4.1,  4.25},
    /* LB_FEC_DVBS2_1_2       */ {  0.6,  1.0,  1.2,  1.4,  1.5,  1.6,  1.7 },
    /* LB_FEC_DVBS2_3_4       */ {  2.7,  3.2,  3.5,  3.7,  3.9,  4.0,  4.1 },
    /* LB_FEC_DVBS2_2_3       */ {  1.6,  2.2,  2.6,  2.9,  3.1,  3.3,  3.4 },
    /* LB_FEC_DVBS2_3_4P      */ {  3.5,  4.1,  4.5,  4.8,  5.0,  5.15, 5.25},
    /* LB_FEC_CCSDS_CC_1_2    */ {  2.0,  3.0,  3.8,  4.3,  4.81, 5.3,  5.7 },
    /* LB_FEC_CCSDS_TURBO_1_6 */ { -2.0, -1.4, -1.1, -0.9, -0.7, -0.6, -0.5 },
};

/* Uncoded Eb/N0 requirement per modulation (dB).
 * Columns: [1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8]
 * BPSK and QPSK share the same theoretical curve (same bits/symbol energy). */
const double LB_MODULATION_UNCODED[LB_MOD_COUNT][LB_BER_COUNT] = {
    /* LB_MOD_BPSK   */ {  4.3,  6.8,  8.4,  9.6, 10.5, 11.3, 12.0 },
    /* LB_MOD_QPSK   */ {  4.3,  6.8,  8.4,  9.6, 10.5, 11.3, 12.0 },
    /* LB_MOD_OQPSK  */ {  4.3,  6.8,  8.4,  9.6, 10.5, 11.3, 12.0 },
    /* LB_MOD_8PSK   */ {  7.9, 10.5, 12.2, 13.5, 14.4, 15.2, 15.9 },
    /* LB_MOD_16APSK */ { 11.5, 14.0, 15.8, 17.0, 17.9, 18.7, 19.3 },
    /* LB_MOD_32APSK */ { 14.5, 17.0, 18.8, 20.0, 20.9, 21.7, 22.3 },
    /* LB_MOD_16QAM  */ { 10.6, 13.4, 15.2, 16.5, 17.5, 18.3, 19.0 },
    /* LB_MOD_64QAM  */ { 16.0, 18.8, 20.6, 21.9, 22.8, 23.6, 24.3 },
    /* LB_MOD_128QAM */ { 19.0, 21.8, 23.6, 24.9, 25.8, 26.6, 27.3 },
    /* LB_MOD_256QAM */ { 22.0, 24.8, 26.6, 27.9, 28.8, 29.6, 30.3 },
};

/* -------------------------------------------------------------------------
 * Utility
 * ---------------------------------------------------------------------- */

static double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* -------------------------------------------------------------------------
 * Function implementations
 * ---------------------------------------------------------------------- */

double lb_required_ebn0_dB(lb_fec_t fec, lb_mod_t mod, lb_ber_t ber)
{
    double bpsk_req = LB_FEC_BPSK_TABLE[fec][ber];
    /* Higher-order modulations need more Eb/N0; penalty is their uncoded
     * requirement minus BPSK's uncoded requirement at the same BER. */
    double mod_pen  = LB_MODULATION_UNCODED[mod][ber]
                    - LB_MODULATION_UNCODED[LB_MOD_BPSK][ber];
    return bpsk_req + mod_pen;
}

double lb_pol_loss_dB(double ar_tx_dB, double ar_wave_dB, double pol_angle_deg)
{
    /* Convert axial-ratio dB values to linear voltage ratios (r = 10^(AR/20)) */
    double r_t   = pow(10.0, ar_tx_dB   / 20.0);
    double r_w   = pow(10.0, ar_wave_dB / 20.0);
    /* Ellipticity angles: ε = arctan(1/r), where r is the axial ratio */
    double eps_t = atan(1.0 / r_t);
    double eps_w = atan(1.0 / r_w);
    double tau   = pol_angle_deg * M_PI / 180.0; /* tilt angle in radians */
    /* Polarisation Loss Factor (PLF); formula from Kraus, Antennas §2-16 */
    double plf   = (1.0
                    + cos(2.0*eps_t) * cos(2.0*eps_w) * cos(2.0*tau)
                    + sin(2.0*eps_t) * sin(2.0*eps_w)) / 2.0;
    if (plf < 1e-12) plf = 1e-12; /* guard against log10(0) */
    return -10.0 * log10(plf);
}

double lb_rx_ant_gain_dBi(double diameter_m, double efficiency, double freq_GHz)
{
    double wavelength  = LB_SPEED_OF_LIGHT / (freq_GHz * 1e9);
    /* Standard parabolic dish gain formula: G = η (π D / λ)² */
    double gain_linear = efficiency * pow(M_PI * diameter_m / wavelength, 2.0);
    if (gain_linear < 1e-10) gain_linear = 1e-10; /* prevent log(0) */
    return 10.0 * log10(gain_linear);
}

double lb_rx_ant_beamwidth_deg(double diameter_m, double freq_GHz)
{
    double wavelength = LB_SPEED_OF_LIGHT / (freq_GHz * 1e9);
    if (diameter_m < 1e-6) diameter_m = 1e-6;
    /* 3 dB half-power beamwidth approximation: θ ≈ 67 λ/D (degrees) */
    return 67.0 * wavelength / diameter_m;
}

double lb_compute_slant_range_km(double tx_alt_km, double rx_alt_km,
                                  double radius_factor)
{
    double re = LB_EARTH_RADIUS_KM * radius_factor;
    double r1 = re + tx_alt_km;
    double r2 = re + rx_alt_km;
    /* Straight-line radial difference — valid only when horizontal offset is
     * unknown and one endpoint is directly above the other. */
    double diff = fabs(r1 - r2);
    return diff < 0.001 ? 0.001 : diff;
}

lb_geometry_t lb_geometry_angles(double tx_alt_km, double rx_alt_km,
                                  double slant_km, double radius_factor)
{
    lb_geometry_t geo;
    double Re   = LB_EARTH_RADIUS_KM * radius_factor;
    double r_tx = Re + tx_alt_km; /* geocentric radius to TX */
    double r_rx = Re + rx_alt_km; /* geocentric radius to RX */
    double R    = slant_km < 0.001 ? 0.001 : slant_km;

    /* Central (subtended) angle via law of cosines on the TX-Earth-center-RX triangle */
    double cos_c = (r_tx*r_tx + r_rx*r_rx - R*R) / (2.0 * r_tx * r_rx);
    geo.central_angle_deg = acos(clamp(cos_c, -1.0, 1.0)) * 180.0 / M_PI;

    /* Elevation angle at TX: interior angle at TX vertex minus 90° gives the
     * angle above the local horizontal tangent plane. */
    double cos_at_tx = (r_tx*r_tx + R*R - r_rx*r_rx) / (2.0 * r_tx * R);
    geo.tx_el_deg = acos(clamp(cos_at_tx, -1.0, 1.0)) * 180.0 / M_PI - 90.0;

    /* Same calculation for RX elevation angle */
    double cos_at_rx = (r_rx*r_rx + R*R - r_tx*r_tx) / (2.0 * r_rx * R);
    geo.rx_el_deg = acos(clamp(cos_at_rx, -1.0, 1.0)) * 180.0 / M_PI - 90.0;

    /* Minimum altitude of either endpoint; used for airspace/terrain checks */
    double min_alt_km = tx_alt_km < rx_alt_km ? tx_alt_km : rx_alt_km;
    geo.min_alt_ft = min_alt_km * 3280.84; /* km to feet: 1 km = 3280.84 ft */

    return geo;
}

double lb_eirp_dBW(double sat_power_dBW, double obo_dB,
                   double feed_loss_dB, double tx_ant_gain_dBi)
{
    /* EIRP = P_sat - OBO - L_feed + G_tx  (all in dB) */
    return sat_power_dBW - obo_dB - feed_loss_dB + tx_ant_gain_dBi;
}

double lb_fspl_dB(double range_km, double freq_GHz)
{
    double d_m  = range_km * 1e3;
    double f_Hz = freq_GHz * 1e9;
    /* Free-space path loss: FSPL = 20 log10(4π d f / c) */
    return 20.0 * log10(4.0 * M_PI * d_m * f_Hz / LB_SPEED_OF_LIGHT);
}

double lb_system_noise_temp_K(double rx_noise_K, double feedline_loss_dB,
                               double noise_figure_dB)
{
    double T0     = 290.0; /* IEEE reference temperature (K) */
    double L_feed = pow(10.0, feedline_loss_dB / 10.0); /* linear feed loss */
    double F_lin  = pow(10.0, noise_figure_dB  / 10.0); /* linear noise figure */
    /* Friis noise formula: T_sys = T_ant/L_feed + T0(1-1/L_feed) + T0(F-1)
     * Term 1: antenna noise reduced by feed attenuation
     * Term 2: thermal noise added by the lossy feed at T0
     * Term 3: noise contribution of the LNA/receiver */
    return (rx_noise_K / L_feed)
         + T0 * (1.0 - 1.0 / L_feed)
         + T0 * (F_lin - 1.0);
}

double lb_gt_dB(double rx_ant_gain_dBi, double t_sys_K)
{
    /* G/T figure of merit: G(dBi) − 10·log10(T_sys in K) */
    return rx_ant_gain_dBi - 10.0 * log10(t_sys_K);
}

double lb_cn0_dBHz(double eirp_val, double fspl_val,
                   double absorptive_loss, double non_absorptive_loss,
                   double gt_val)
{
    /* C/N0 (dBHz) = EIRP − FSPL − L_abs − L_nonabs + G/T − k
     * where k = Boltzmann's constant in dBW/K/Hz (LB_BOLTZMANN_DB ≈ −228.6) */
    return eirp_val - fspl_val - absorptive_loss - non_absorptive_loss
           + gt_val - LB_BOLTZMANN_DB;
}

double lb_ebn0_dB(double cn0_val, double data_rate_Mbps)
{
    /* Eb/N0 = C/N0 − 10·log10(Rb);  Rb converted from Mb/s to b/s */
    return cn0_val - 10.0 * log10(data_rate_Mbps * 1e6);
}

double lb_link_margin_dB(double achieved, double required, double impl_loss)
{
    /* Positive margin → link closed; negative → link open */
    return achieved - required - impl_loss;
}

lb_result_t lb_run(const lb_params_t *p)
{
    lb_result_t r;

    /* --- Geometry: use explicit slant range if provided, else derive it --- */
    r.slant_range_km_used = (p->slant_range_km > 0)
        ? p->slant_range_km
        : lb_compute_slant_range_km(p->tx_alt_km, p->rx_alt_km, p->radius_factor);

    /* --- Transmit chain --- */
    /* Subtract polarisation loss from EIRP rather than lumping it into path
     * losses, so the EIRP display reflects the actual net radiated power. */
    r.eirp_dBW = lb_eirp_dBW(p->sat_power_dBW, p->obo_dB,
                               p->feed_loss_tx_dB, p->tx_ant_gain_dBi)
                 - p->pol_loss_dB;

    /* --- Path loss --- */
    r.fspl_dB = lb_fspl_dB(r.slant_range_km_used, p->freq_GHz);

    /* --- Receive chain --- */
    r.t_sys_K = lb_system_noise_temp_K(p->rx_noise_K,
                                        p->feedline_loss_dB,
                                        p->noise_figure_dB);
    r.gt_dBK  = lb_gt_dB(p->rx_ant_gain_dBi, r.t_sys_K);

    /* --- C/N0, Eb/N0 --- */
    r.cn0_dBHz = lb_cn0_dBHz(r.eirp_dBW, r.fspl_dB,
                               p->absorptive_loss_dB,
                               p->non_absorptive_loss_dB,
                               r.gt_dBK);
    r.ebn0_dB  = lb_ebn0_dB(r.cn0_dBHz, p->data_rate_Mbps);

    /* --- Required Eb/N0 and link margin --- */
    r.req_ebn0_dB   = lb_required_ebn0_dB(p->fec_decoder, p->modulation, p->ber);
    r.link_margin_dB = lb_link_margin_dB(r.ebn0_dB, r.req_ebn0_dB, p->impl_loss_dB);
    r.closed         = (r.link_margin_dB >= 0.0) ? 1 : 0;

    return r;
}

/* =========================================================================
 * ITU-R P.676-12 Annex 2 — Atmospheric gaseous attenuation
 * ========================================================================= */

/* Specific attenuation due to dry air (oxygen) in dB/km.
 * Implements ITU-R P.676-12 Annex 2 simplified method.
 * Valid for frequencies up to ~350 GHz and standard atmospheric profiles. */
double lb_p676_gamma_o(double f, double p_hPa, double T_C)
{
    double rp  = p_hPa / 1013.25;       /* pressure ratio relative to standard */
    double rt  = 288.15 / (T_C + 273.15); /* inverse temperature ratio (288.15 K = 15 °C) */
    double f2  = f * f;

    /* ξ correction factors (P.676-12 Annex 2, Eq. 2a-c) */
    double xi1 = pow(rp, 0.0717) * pow(rt,-1.8132) * exp( 0.0156*(1-rp) - 1.6515*(1-rt));
    double xi2 = pow(rp, 0.5146) * pow(rt,-4.6368) * exp(-0.1921*(1-rp) - 5.7416*(1-rt));
    double xi3 = pow(rp, 0.3414) * pow(rt,-6.5851) * exp( 0.2130*(1-rp) - 8.5854*(1-rt));

    /* t1: broadband continuum and low-frequency lines below 54 GHz */
    double t1 = 7.2 * pow(rt, 2.8) / (f2 + 0.34*rp*rp*pow(rt, 1.6));
    /* t2: combined effect of oxygen lines near 60 GHz complex;
     * guard against f = 54.0 GHz exactly (singularity in the exponent) */
    double f54 = fabs(54.0 - f); if (f54 < 0.01) f54 = 0.01;
    double t2 = 0.62 * xi3 / (pow(f54, 1.16*xi1) + 0.83*xi2);

    /* Result in dB/km; ×1e-3 absorbs unit scaling from the ITU formula */
    return (t1 + t2) * f2 * rp * rt*rt * 1e-3;
}

/* Specific attenuation due to water vapour in dB/km.
 * Implements ITU-R P.676-12 Annex 2 simplified method for water-vapour lines.
 * rho: water vapour density (g/m³); typical surface value ~7.5 g/m³. */
double lb_p676_gamma_w(double f, double rho, double p_hPa, double T_C)
{
    double rp  = p_hPa / 1013.25;
    /* ITU-R P.676 uses 300 K (not 288.15 K) as the reference for water vapour */
    double rt  = 300.0  / (T_C + 273.15);
    double f2  = f * f;

    /* η1: line width broadening factor incorporating pressure and vapour density */
    double eta1 = 0.955*rp*pow(rt,0.68) + 0.006*rho;
    /* η2 is defined in the standard but not needed in this simplified form */
    (void)(0.735*rp*pow(rt, 0.5) + 0.0353*pow(rt,4)*rho); /* eta2 unused */
    if (eta1 < 1e-9) eta1 = 1e-9; /* avoid division by zero in line denominators */

    /* --- Principal water-vapour absorption lines ---
     * 22.235 GHz line with Van Vleck–Weisskopf lineshape correction factor F1 */
    double d22  = (f-22.235)*(f-22.235) + 9.42*eta1*eta1;
    double F1   = 11.96 / (1.0 + 0.0454*pow((f-22.235)/eta1, 2.0));
    /* 183.31 GHz, 321.226 GHz, 325.153 GHz lines (no F1 correction needed) */
    double d183 = (f-183.31)*(f-183.31) + 11.14*eta1*eta1;
    double d321 = (f-321.226)*(f-321.226)+ 6.29*eta1*eta1;
    double d325 = (f-325.153)*(f-325.153)+ 9.22*eta1*eta1;

    /* Weighted sum over the four lines; exponential factors are temperature
     * scaling for each line's intensity (P.676-12 Annex 2, Table 2) */
    double sum = 3.98 *eta1*exp(2.23*(1-rt))/d22 * F1
               + 11.96*eta1*exp(0.70*(1-rt))/d183
               + 0.081*eta1*exp(6.44*(1-rt))/d321
               + 3.66 *eta1*exp(1.60*(1-rt))/d325;

    /* ×1e-4 is the ITU unit-conversion scale factor for this simplified model */
    return sum * f2 * pow(rt, 2.5) * rho * 1e-4;
}

/* Total one-way gaseous attenuation along a slant path (dB).
 * Uses the zenith attenuation scaled by 1/sin(elevation) (flat-Earth cosecant
 * law), clamped to 5° minimum elevation to avoid unrealistic results
 * at very low angles. */
double lb_gaseous_atten_dB(double f_GHz, double el_deg,
                            double p_hPa, double T_C, double rho_gm3)
{
    double go = lb_p676_gamma_o(f_GHz, p_hPa, T_C);
    double gw = lb_p676_gamma_w(f_GHz, rho_gm3, p_hPa, T_C);

    /* Equivalent heights (P.676-12 Eqs. 7a–7b): scale specific attenuation
     * to total zenith path through the atmosphere */
    double h_o = 6.0;  /* oxygen effective height ≈ 6 km */
    double h_w = 1.6 + 1.0 / (1.0 + 0.0281*rho_gm3); /* water vapour ≈ 1.6–2.1 km */

    double zenith = go * h_o + gw * h_w; /* total zenith attenuation (dB) */

    /* Cosecant law: slant path = zenith / sin(θ); 5° floor avoids ÷near-zero */
    if (el_deg < 5.0) el_deg = 5.0;
    return zenith / sin(el_deg * M_PI / 180.0);
}

/* =========================================================================
 * ITU-R P.838-3 — Rain attenuation
 * ========================================================================= */

/* Specific rain attenuation γ_R (dB/km) as a function of frequency and
 * rain rate.  Implements the power-law γ_R = k R^α where k and α are
 * determined by Gaussian-sum fits to the P.838-3 Table 1 coefficients. */
double lb_p838_gamma_r(double f_GHz, double R_mmh)
{
    if (R_mmh <= 0.0) return 0.0;

    /* P.838-3 Table 1: Gaussian-sum coefficients for log10(k_H) fit.
     * Each row is {a_j, b_j, c_j}: log10(k) = Σ a_j exp(-((log10f-b_j)/c_j)^2) + m·log10f + c */
    static const double ak[4][3] = {
        {-5.33980, -0.10008, 1.13098},
        {-0.35351,  1.26970, 0.45400},
        {-0.23789,  0.86036, 0.15354},
        {-0.94158,  0.64552, 0.16817}
    };
    double mk = -0.18961, ck =  0.71147; /* linear slope and intercept for k */

    /* Gaussian-sum coefficients for α_H (exponent) fit */
    static const double aa[5][3] = {
        {-0.14318, 1.82442, -0.55187},
        { 0.29591, 0.77564,  0.19822},
        { 0.32177, 0.63773,  0.13164},
        {-5.37610,-0.96230,  1.47828},
        {16.1721, -3.29980,  3.43990}
    };
    double ma =  0.67849, ca = -1.95537; /* linear slope and intercept for α */

    double lf = log10(f_GHz); /* work in log-frequency space */
    double log_k = mk*lf + ck;
    for (int j = 0; j < 4; j++)
        log_k += ak[j][0] * exp(-pow((lf - ak[j][1])/ak[j][2], 2.0));
    double k = pow(10.0, log_k);

    double alpha = ma*lf + ca;
    for (int j = 0; j < 5; j++)
        alpha += aa[j][0] * exp(-pow((lf - aa[j][1])/aa[j][2], 2.0));

    return k * pow(R_mmh, alpha);   /* γ_R in dB/km (P.838-3 Eq. 1) */
}

/* Total rain attenuation along an effective path length L_eff (dB).
 * L_eff should already account for the effective path reduction factor
 * (e.g. slant range * sin(elevation) for a vertical profile). */
double lb_rain_atten_dB(double f_GHz, double R_mmh, double L_eff_km)
{
    return lb_p838_gamma_r(f_GHz, R_mmh) * L_eff_km;
}
