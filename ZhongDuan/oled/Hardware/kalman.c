#include "kalman.h"

static float AbsFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

void Kalman_Init(KALMAN *filter, float initial, float q, float r,
                 uint32_t now_ms)
{
    filter->x = initial;
    filter->P = r;
    filter->Q = q;
    filter->R = r;
    filter->last_time_ms = now_ms;
    filter->initialized = 1U;
}

void Kalman_Reset(KALMAN *filter)
{
    filter->initialized = 0U;
    filter->last_time_ms = 0U;
}

float Kalman_Update(KALMAN *filter, float measured, uint32_t now_ms,
                    float max_step)
{
    float dt;
    float innovation;
    float gain;

    if (!filter->initialized) {
        Kalman_Init(filter, measured, filter->Q, filter->R, now_ms);
        return measured;
    }

    dt = (float)(now_ms - filter->last_time_ms) / 1000.0f;
    filter->last_time_ms = now_ms;
    if (dt < 0.001f) {
        dt = 0.001f;
    } else if (dt > 1.0f) {
        dt = 1.0f;
    }

    /* Prediction step from the vendor sample. */
    filter->P += filter->Q * dt;

    /* Keep angle innovation continuous across the -180/+180 boundary. */
    innovation = measured - filter->x;
    while (innovation > 180.0f) {
        innovation -= 360.0f;
    }
    while (innovation < -180.0f) {
        innovation += 360.0f;
    }

    /* A single multipath spike must not drag the filter by tens of degrees. */
    if ((max_step > 0.0f) && (AbsFloat(innovation) > max_step)) {
        innovation = (innovation > 0.0f) ? max_step : -max_step;
    }

    /* Measurement update from the vendor sample. */
    gain = filter->P / (filter->P + filter->R);
    filter->x += gain * innovation;
    filter->P = (1.0f - gain) * filter->P;

    while (filter->x > 180.0f) {
        filter->x -= 360.0f;
    }
    while (filter->x < -180.0f) {
        filter->x += 360.0f;
    }
    return filter->x;
}
