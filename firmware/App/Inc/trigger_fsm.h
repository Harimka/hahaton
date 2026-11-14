/*******************************************************************************
 * @file    trigger_fsm.h
 * @author  team-04 (Hackathon 2026)
 * @brief   State machine for shock pulse detection, pretrigger capture and framing.
 ******************************************************************************/

#ifndef TRIGGER_FSM_H_
#define TRIGGER_FSM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sensor_imu.h"

#define TRIGGER_THRESHOLD_G         (3.0f)   ///< Initial trigger threshold (3g)
#define TRIGGER_AMAX_RATIO          (0.25f)  ///< Cutoff boundary: 0.25 * Amax

/**
 * @brief Shock detector state machine states.
 */
typedef enum {
    TRIGGER_STATE_IDLE = 0,     ///< Monitoring baseline, filling pretrigger ring buffer
    TRIGGER_STATE_RECORDING,    ///< Shock detected (>3g), capturing pulse until decay
    TRIGGER_STATE_FRAME_READY,  ///< Pulse decayed, boundaries (0.25*Amax) calculated
    TRIGGER_STATE_PROCESSING    ///< SRS calculation & SD card flush in progress
} trigger_state_t;

/**
 * @brief Context structure for shock event detector.
 */
typedef struct {
    trigger_state_t state;
    float peak_amax;            ///< Maximum absolute acceleration recorded during shock
    uint32_t sample_counter;
    uint32_t trigger_start_idx;
    uint32_t trigger_end_idx;
} trigger_context_t;

/**
 * @brief Initialize shock detector FSM.
 */
void trigger_fsm_init(trigger_context_t *ctx);

/**
 * @brief Process single acceleration sample.
 * @return true if shock event was finalized and frame is ready for SRS/SD writing.
 */
bool trigger_fsm_feed_sample(trigger_context_t *ctx, const imu_accel_raw_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* TRIGGER_FSM_H_ */
/***************************** END OF FILE ************************************/
