#ifndef __KALMAN_H__
#define __KALMAN_H__

#include <stdint.h>

/*
 * One-dimensional Kalman filter adapted from the ALX-AOA example library.
 * Q: process-noise variance per second. R: measurement-noise variance.
 */
typedef struct {
    float x;
    float P;
    float Q;
    float R;
    uint32_t last_time_ms;
    uint8_t initialized;
} KALMAN;

void Kalman_Init(KALMAN *filter, float initial, float q, float r,
                 uint32_t now_ms);
void Kalman_Reset(KALMAN *filter);
float Kalman_Update(KALMAN *filter, float measured, uint32_t now_ms,
                    float max_step);

#endif
