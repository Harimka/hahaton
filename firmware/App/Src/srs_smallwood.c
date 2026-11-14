/*******************************************************************************
 * @file    srs_smallwood.c
 * @author  team-04 (Hackathon 2026)
 * @brief   Shock Response Spectrum (SRS) calculation using the Smallwood
 *          Ramp Invariant digital recursive filter method.
 ******************************************************************************/

#include "srs_smallwood.h"
#include <math.h>
#include <stddef.h>

/* Private defines -----------------------------------------------------------*/
#define SRS_PI_F                (3.14159265358979323846f)
#define SRS_DEFAULT_SAMPLE_RATE (6660.0f)
#define SRS_EPSILON             (1e-7f)

/* Private constants ---------------------------------------------------------*/
/**
 * @brief Standard 94-frequency grid spanning 10 Hz to ~2113 Hz (1/12 octave / 40 per decade).
 */
static const float s_srs_frequencies[SRS_NUM_FREQUENCIES] = {
    10.000001f,   10.592537f,   11.220184f,   11.885021f,   12.589254f,
    13.335215f,   14.125375f,   14.962358f,   15.848932f,   16.788044f,
    17.782793f,   18.836493f,   19.952627f,   21.134890f,   22.387215f,
    23.713739f,   25.118864f,   26.607250f,   28.183832f,   29.853821f,
    31.622776f,   33.496555f,   35.481335f,   37.583748f,   39.810726f,
    42.169655f,   44.668362f,   47.315128f,   50.118721f,   53.088440f,
    56.234146f,   59.566204f,   63.095745f,   66.834396f,   70.794579f,
    74.989418f,   79.432854f,   84.139503f,   89.125114f,   94.406105f,
    100.000015f,  105.925385f,  112.201851f,  118.850220f,  125.892532f,
    133.352173f,  141.253723f,  149.623596f,  158.489334f,  167.880417f,
    177.827942f,  188.364975f,  199.526215f,  211.348953f,  223.872162f,
    237.137405f,  251.188660f,  266.072510f,  281.838409f,  298.538239f,
    316.227722f,  334.965515f,  354.813446f,  375.837280f,  398.107361f,
    421.696503f,  446.683563f,  473.151398f,  501.187347f,  530.884338f,
    562.341431f,  595.662170f,  630.957336f,  668.344177f,  707.945740f,
    749.894104f,  794.328430f,  841.395264f,  891.250671f,  944.061340f,
    1000.000000f, 1059.253662f, 1122.019287f, 1188.502075f, 1258.925171f,
    1333.522217f, 1412.538208f, 1496.235107f, 1584.893799f, 1678.804565f,
    1778.279907f, 1883.649536f, 1995.262695f, 2113.489258f
};

/* Private variables ---------------------------------------------------------*/
static srs_filter_coeff_t s_coeff_table[SRS_NUM_FREQUENCIES];
static bool s_coeffs_initialized = false;

/* Private function prototypes -----------------------------------------------*/
static float srs_filter_single_axis(const float *samples, uint32_t num_samples,
                                    const srs_filter_coeff_t *c);

/* Private functions ---------------------------------------------------------*/
/**
 * @brief  Computes Maximax absolute peak response for a single acceleration axis
 *         using 2nd-order recursive Smallwood filter difference equation:
 *         z(k) = -a1*z(k-1) - a2*z(k-2) + b0*x(k) + b1*x(k-1) + b2*x(k-2)
 * @param  samples     Pointer to array of acceleration samples [g]
 * @param  num_samples Total sample count in frame
 * @param  c           Pointer to precalculated Smallwood coefficients for frequency fn
 * @return Absolute maximum peak acceleration response |z(k)| across the frame
 */
static float srs_filter_single_axis(const float *samples, uint32_t num_samples,
                                    const srs_filter_coeff_t *c)
{
    if ((samples == NULL) || (num_samples == 0U) || (c == NULL)) {
        return 0.0f;
    }

    const float b0 = c->b0;
    const float b1 = c->b1;
    const float b2 = c->b2;
    const float a1 = c->a1;
    const float a2 = c->a2;

    float z_1 = 0.0f;
    float z_2 = 0.0f;
    float x_1 = 0.0f;
    float x_2 = 0.0f;
    float max_peak = 0.0f;

    for (uint32_t k = 0U; k < num_samples; ++k) {
        const float x_k = samples[k];
        const float z_0 = (-a1 * z_1) - (a2 * z_2) + (b0 * x_k) + (b1 * x_1) + (b2 * x_2);
        const float abs_z = fabsf(z_0);

        if (abs_z > max_peak) {
            max_peak = abs_z;
        }

        x_2 = x_1;
        x_1 = x_k;
        z_2 = z_1;
        z_1 = z_0;
    }

    return max_peak;
}

/* Exported functions --------------------------------------------------------*/
/**
 * @brief  Precalculates Smallwood coefficients for a given sampling rate.
 *         Formulas (Smallwood Ramp Invariant method):
 *           omega_n = 2 * pi * fn
 *           T = 1 / sample_rate_hz
 *           A = omega_n * T / (2 * Q)
 *           B = omega_n * T * sqrt(1 - 1 / (4 * Q^2))
 *           beta0 = 1 - exp(-A) * sin(B) / B
 *           beta1 = 2 * exp(-A) * (sin(B) / B - cos(B))
 *           beta2 = exp(-2 * A) - exp(-A) * sin(B) / B
 *           alpha1 = -2 * exp(-A) * cos(B)
 *           alpha2 = exp(-2 * A)
 * @param  sample_rate_hz Sampling rate (e.g. 6660.0f Hz for LSM6DS3)
 */
void srs_init_coefficients(float sample_rate_hz)
{
    if (sample_rate_hz <= 0.0f) {
        sample_rate_hz = SRS_DEFAULT_SAMPLE_RATE;
    }

    const float dt = 1.0f / sample_rate_hz;
    const float q = SRS_Q_FACTOR;
    const float one_over_four_q2 = 1.0f / (4.0f * q * q);
    const float q_term = sqrtf(1.0f - one_over_four_q2);

    for (uint32_t i = 0U; i < SRS_NUM_FREQUENCIES; ++i) {
        const float fn = s_srs_frequencies[i];
        const float wn = 2.0f * SRS_PI_F * fn;
        const float a = (wn * dt) / (2.0f * q);
        const float b = wn * dt * q_term;

        const float exp_neg_a = expf(-a);
        const float exp_neg_2a = expf(-2.0f * a);
        const float sin_b = sinf(b);
        const float cos_b = cosf(b);

        const float sin_b_div_b = (fabsf(b) > SRS_EPSILON) ? (sin_b / b) : 1.0f;

        s_coeff_table[i].fn = fn;
        s_coeff_table[i].b0 = 1.0f - (exp_neg_a * sin_b_div_b);
        s_coeff_table[i].b1 = 2.0f * exp_neg_a * (sin_b_div_b - cos_b);
        s_coeff_table[i].b2 = exp_neg_2a - (exp_neg_a * sin_b_div_b);
        s_coeff_table[i].a1 = -2.0f * exp_neg_a * cos_b;
        s_coeff_table[i].a2 = exp_neg_2a;
    }

    s_coeffs_initialized = true;
}

/**
 * @brief  Computes SRS across all 94 frequencies for a given frame of acceleration samples.
 *         Calculates Maximax absolute peak response |z(k)| for X, Y, and Z axes.
 * @param  ax          Array of X acceleration samples in g
 * @param  ay          Array of Y acceleration samples in g
 * @param  az          Array of Z acceleration samples in g
 * @param  num_samples Total samples in frame
 * @param  out_srs     Output buffer of SRS_NUM_FREQUENCIES points
 * @return true on successful calculation, false on invalid arguments
 */
bool srs_compute_frame(const float *ax, const float *ay, const float *az,
                       uint32_t num_samples, srs_point_t *out_srs)
{
    if ((out_srs == NULL) || (num_samples == 0U)) {
        return false;
    }

    if ((ax == NULL) && (ay == NULL) && (az == NULL)) {
        return false;
    }

    if (!s_coeffs_initialized) {
        srs_init_coefficients(SRS_DEFAULT_SAMPLE_RATE);
    }

    for (uint32_t i = 0U; i < SRS_NUM_FREQUENCIES; ++i) {
        const srs_filter_coeff_t *coeff = &s_coeff_table[i];

        out_srs[i].fn = coeff->fn;
        out_srs[i].gx = (ax != NULL) ? srs_filter_single_axis(ax, num_samples, coeff) : 0.0f;
        out_srs[i].gy = (ay != NULL) ? srs_filter_single_axis(ay, num_samples, coeff) : 0.0f;
        out_srs[i].gz = (az != NULL) ? srs_filter_single_axis(az, num_samples, coeff) : 0.0f;
    }

    return true;
}

/***************************** END OF FILE ************************************/
