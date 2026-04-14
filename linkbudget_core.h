/**
 * linkbudget_core.h
 * Link Budget Calculator - C Implementation
 */

#ifndef LINKBUDGET_CORE_H
#define LINKBUDGET_CORE_H

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define LB_SPEED_OF_LIGHT   2.99792458e8   /* m/s */
#define LB_BOLTZMANN_DB     (-228.6)        /* dBW/K/Hz */
#define LB_EARTH_RADIUS_KM  6371.0          /* km */

/* -------------------------------------------------------------------------
 * Enumerations
 * ---------------------------------------------------------------------- */

typedef enum {
    LB_BER_1E2 = 0,
    LB_BER_1E3,
    LB_BER_1E4,
    LB_BER_1E5,
    LB_BER_1E6,
    LB_BER_1E7,
    LB_BER_1E8,
    LB_BER_COUNT
} lb_ber_t;

typedef enum {
    LB_FEC_UNCODED = 0,
    LB_FEC_VA_HALF_K7,       /* VA 1/2 (K=7)           */
    LB_FEC_VA_3Q_K7,         /* VA 3/4 (K=7)           */
    LB_FEC_VA_7E8_K7,        /* VA 7/8 (K=7)           */
    LB_FEC_RS_VA_HALF,       /* RS(255,223) + VA 1/2   */
    LB_FEC_TURBO_1_6,        /* Turbo r=1/6            */
    LB_FEC_TURBO_1_3,        /* Turbo r=1/3            */
    LB_FEC_TURBO_1_2,        /* Turbo r=1/2            */
    LB_FEC_TURBO_3_4,        /* Turbo r=3/4            */
    LB_FEC_LDPC_1_2,         /* LDPC r=1/2             */
    LB_FEC_LDPC_2_3,         /* LDPC r=2/3             */
    LB_FEC_LDPC_3_4,         /* LDPC r=3/4             */
    LB_FEC_LDPC_5_6,         /* LDPC r=5/6             */
    LB_FEC_DVBS2_1_2,        /* DVB-S2 r=1/2           */
    LB_FEC_DVBS2_3_4,        /* DVB-S2 r=3/4           */
    LB_FEC_DVBS2_2_3,        /* DVB-S2 r=2/3           */
    LB_FEC_DVBS2_3_4P,       /* DVB-S2 r=3/4+          */
    LB_FEC_CCSDS_CC_1_2,     /* CCSDS CC r=1/2 K=7     */
    LB_FEC_CCSDS_TURBO_1_6,  /* CCSDS Turbo r=1/6      */
    LB_FEC_COUNT
} lb_fec_t;

typedef enum {
    LB_MOD_BPSK = 0,
    LB_MOD_QPSK,
    LB_MOD_OQPSK,
    LB_MOD_8PSK,
    LB_MOD_16APSK,
    LB_MOD_32APSK,
    LB_MOD_16QAM,
    LB_MOD_64QAM,
    LB_MOD_128QAM,
    LB_MOD_256QAM,
    LB_MOD_COUNT
} lb_mod_t;

/* -------------------------------------------------------------------------
 * Lookup tables (declared extern, defined in .c)
 * ---------------------------------------------------------------------- */

/* Required Eb/N0 for each FEC scheme assuming BPSK modulation [fec][ber] */
extern const double LB_FEC_BPSK_TABLE[LB_FEC_COUNT][LB_BER_COUNT];

/* Uncoded Eb/N0 requirement per modulation [mod][ber] */
extern const double LB_MODULATION_UNCODED[LB_MOD_COUNT][LB_BER_COUNT];

/* -------------------------------------------------------------------------
 * Structs
 * ---------------------------------------------------------------------- */

typedef struct {
    double tx_el_deg;
    double rx_el_deg;
    double central_angle_deg;
    double min_alt_ft;
} lb_geometry_t;

typedef struct {
    double slant_range_km_used;
    double eirp_dBW;
    double fspl_dB;
    double t_sys_K;
    double gt_dBK;
    double cn0_dBHz;
    double ebn0_dB;
    double req_ebn0_dB;
    double link_margin_dB;
    int    closed;   /* 1 = CLOSED, 0 = OPEN (FAILED) */
} lb_result_t;

typedef struct {
    double freq_GHz;
    double data_rate_Mbps;
    lb_fec_t fec_decoder;
    lb_mod_t modulation;
    lb_ber_t ber;
    double impl_loss_dB;
    double tx_alt_km;
    double rx_alt_km;
    double slant_range_km;   /* <= 0 → auto-compute */
    double radius_factor;
    double sat_power_dBW;
    double obo_dB;
    double feed_loss_tx_dB;
    double tx_ant_gain_dBi;
    double pol_loss_dB;
    double absorptive_loss_dB;
    double non_absorptive_loss_dB;
    double rx_ant_gain_dBi;
    double rx_noise_K;
    double feedline_loss_dB;
    double noise_figure_dB;
} lb_params_t;

/* -------------------------------------------------------------------------
 * Function declarations
 * ---------------------------------------------------------------------- */

/** Required Eb/N0 (dB) for given FEC decoder, modulation, and BER. */
double lb_required_ebn0_dB(lb_fec_t fec, lb_mod_t mod, lb_ber_t ber);

/** Polarization mismatch loss (dB). Axial ratios in dB, angle in degrees. */
double lb_pol_loss_dB(double ar_tx_dB, double ar_wave_dB, double pol_angle_deg);

/** Parabolic antenna gain (dBi). efficiency = 0..1. */
double lb_rx_ant_gain_dBi(double diameter_m, double efficiency, double freq_GHz);

/** Half-power beamwidth of a parabolic antenna (degrees). */
double lb_rx_ant_beamwidth_deg(double diameter_m, double freq_GHz);

/** Geometric angles for a slant path between two nodes. */
lb_geometry_t lb_geometry_angles(double tx_alt_km, double rx_alt_km,
                                  double slant_km, double radius_factor);

/**
 * Slant range between two nodes at given altitudes (km).
 * Returns |h_tx - h_rx| as a conservative approximation (min 0.001 km).
 */
double lb_compute_slant_range_km(double tx_alt_km, double rx_alt_km,
                                  double radius_factor);

/** EIRP = P_sat - OBO - L_feed + G_tx  (all dB). */
double lb_eirp_dBW(double sat_power_dBW, double obo_dB,
                   double feed_loss_dB, double tx_ant_gain_dBi);

/** Free-Space Path Loss (dB). range_km > 0, freq_GHz > 0. */
double lb_fspl_dB(double range_km, double freq_GHz);

/** System noise temperature (K) via cascade noise model. */
double lb_system_noise_temp_K(double rx_noise_K, double feedline_loss_dB,
                               double noise_figure_dB);

/** G/T ratio (dB/K). t_sys_K must be > 0. */
double lb_gt_dB(double rx_ant_gain_dBi, double t_sys_K);

/** C/N0 (dBHz). */
double lb_cn0_dBHz(double eirp_val, double fspl_val,
                   double absorptive_loss, double non_absorptive_loss,
                   double gt_val);

/** Eb/N0 (dB). data_rate_Mbps must be > 0. */
double lb_ebn0_dB(double cn0_val, double data_rate_Mbps);

/** Link Margin = Achieved Eb/N0 - Required Eb/N0 - Implementation Loss. */
double lb_link_margin_dB(double achieved, double required, double impl_loss);

/** Run complete link budget. Returns filled lb_result_t. */
lb_result_t lb_run(const lb_params_t *p);

/**
 * Specific gaseous attenuation (dB/km): oxygen component.
 * ITU-R P.676-12 Annex 2 simplified.
 * f_GHz: 1-54 GHz; p_hPa: pressure; T_C: temperature °C.
 */
double lb_p676_gamma_o(double f_GHz, double p_hPa, double T_C);

/**
 * Specific gaseous attenuation (dB/km): water vapour component.
 * ITU-R P.676-12 Annex 2 simplified.
 * rho: water vapour density g/m³.
 */
double lb_p676_gamma_w(double f_GHz, double rho, double p_hPa, double T_C);

/**
 * Total slant-path gaseous attenuation (dB).
 * Uses equivalent-height method (P.676-12).
 * el_deg: elevation angle (degrees, clamped to ≥5°).
 */
double lb_gaseous_atten_dB(double f_GHz, double el_deg,
                            double p_hPa, double T_C, double rho_gm3);

/**
 * Specific rain attenuation (dB/km). ITU-R P.838-3.
 * f_GHz: 1-100 GHz; R_mmh: rain rate (mm/h).
 */
double lb_p838_gamma_r(double f_GHz, double R_mmh);

/**
 * Total rain attenuation on a path (dB).
 * L_eff_km: effective path length through rain cell (km).
 */
double lb_rain_atten_dB(double f_GHz, double R_mmh, double L_eff_km);

#ifdef __cplusplus
}
#endif

#endif /* LINKBUDGET_CORE_H */
