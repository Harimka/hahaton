/*******************************************************************************
 * @file    srs_smallwood.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Shock Response Spectrum (SRS) calculation using Smallwood Ramp Invariant method.
 ******************************************************************************/

#ifndef SRS_SMALLWOOD_H_
#define SRS_SMALLWOOD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define SRS_NUM_FREQUENCIES     (94U)  ///< Number of frequencies according to frequency grid (10 to 2000 Hz)
#define SRS_Q_FACTOR            (10.0f)///< Standard oscillator quality factor Q=10

/**
 * @brief Precalculated digital filter coefficients for Smallwood oscillator at frequency fn.
 */
typedef struct {
    float fn;   ///< Natural frequency [Hz]
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} srs_filter_coeff_t;

/**
 * @brief Computed SRS response point for all 3 axes in Maximax mode.
 */
typedef struct {
    float fn;   ///< Frequency in Hz
    float gx;   ///< Peak response X [g]
    float gy;   ///< Peak response Y [g]
    float gz;   ///< Peak response Z [g]
} srs_point_t;

/**
 * @brief Precalculates Smallwood coefficients for a given sampling rate.
 * @param sample_rate_hz Sampling rate (6660.0f Hz for LSM6DS3)
 */
void srs_init_coefficients(float sample_rate_hz);

/**
 * @brief Computes SRS across all 94 frequencies for a given frame of acceleration samples.
 * @param ax Array of X acceleration samples in g
 * @param ay Array of Y acceleration samples in g
 * @param az Array of Z acceleration samples in g
 * @param num_samples Total samples in frame
 * @param out_srs Output buffer of SRS_NUM_FREQUENCIES points
 * @return true on successful calculation
 */
bool srs_compute_frame(const float *ax, const float *ay, const float *az,
                       uint32_t num_samples, srs_point_t *out_srs);

#ifdef __cplusplus
}
#endif

#endif /* SRS_SMALLWOOD_H_ */
/***************************** END OF FILE ************************************/
