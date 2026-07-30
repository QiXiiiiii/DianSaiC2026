/**
 * @file    main.c
 * @brief   C题智能门锁端 - STM32F103RCT6 + 单ALX-AOA基站
 *
 * UWB基站连接USART2(PA2/PA3)，115200-8-N-1。
 * PB12~PB15为4位ID拨码，拨到ON时接地。
 * PC2开锁灯、PC3闭锁灯、PC13迎宾灯，均按低电平有效连接。
 */

#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "OLED.h"
#include "kalman.h"
#include <stdio.h>

#define DIP_PORT              GPIOB
#define DIP_PIN1              GPIO_PIN_12
#define DIP_PIN2              GPIO_PIN_13
#define DIP_PIN3              GPIO_PIN_14
#define DIP_PIN4              GPIO_PIN_15
#define DIP_RD(pin)           ((HAL_GPIO_ReadPin(DIP_PORT, (pin)) == GPIO_PIN_RESET) ? 1U : 0U)

#define BEEPER_PIN            GPIO_PIN_1
#define LED_OPEN_PIN          GPIO_PIN_2
#define LED_CLOSED_PIN        GPIO_PIN_3
#define LED_WELCOME_PIN       GPIO_PIN_13

#define UWB_FRAME_LENGTH      37U
#define UWB_COMMAND_LOCATION  0x2001U
#define TAG_TIMEOUT_MS        600U
#define DISPLAY_PERIOD_MS     100U
#define TRANSITION_FRAMES     3U
#define FRONT_LIMIT_DEG       45

/* 现场标定项：题目距离零点是门锁圆柱体前边缘，不是UWB天线中心。 */
#define DISTANCE_OFFSET_CM    0
#define AZIMUTH_OFFSET_DEG    0

/*
 * 二次角度滤波参数。R越大越稳但响应越慢，Q越大响应越快。
 * 25 deg^2相当于把UWB角度测量标准差按约5度处理。
 */
#define AZIMUTH_KALMAN_Q      1.0f
#define AZIMUTH_KALMAN_R      64.0f
#define AZIMUTH_MAX_STEP_DEG  12.0f
#define AZIMUTH_MEDIAN_COUNT  9U
#define AZIMUTH_ZERO_LOCK_DEG 3
#define DISTANCE_MEDIAN_COUNT 5U
#define DISTANCE_HOLD_CM      3U
#define DISTANCE_FAST_CM      20U

/* 进入阈值与离开阈值分离，避免在1m/2m边界反复动作。 */
#define OPEN_ENTER_CM         95U
#define OPEN_EXIT_CM          110U
#define WELCOME_ENTER_CM      195U
#define WELCOME_EXIT_CM       210U

typedef enum {
    LOCK_NO_TAG = 0,
    LOCK_ID_DENIED,
    LOCK_OUTSIDE,
    LOCK_DETECTION,
    LOCK_WELCOME,
    LOCK_OPEN
} LockState;

typedef struct {
    uint16_t sequence;
    uint32_t anchor_id;
    uint32_t tag_id;
    uint32_t distance_cm;
    int16_t azimuth_deg;
    int16_t elevation_deg;
} UwbLocation;

static uint8_t UWB_IsSupportedVersion(uint16_t version)
{
    return (version == 0x0100U) || (version == 0x0101U) ||
           (version == 0x0102U);
}

static UwbLocation g_location;
static uint32_t g_last_frame_ms;
static uint8_t g_have_frame;
static uint8_t g_dip_id;
static LockState g_state = LOCK_NO_TAG;
static LockState g_candidate = LOCK_NO_TAG;
static uint8_t g_candidate_count;
static char g_event[17] = "WAITING TAG     ";
static uint32_t g_beep_until;
static KALMAN g_azimuth_kalman;
static int16_t g_filtered_azimuth;
static uint32_t g_filter_tag_id;
static int16_t g_angle_window[AZIMUTH_MEDIAN_COUNT];
static uint8_t g_angle_window_count;
static uint8_t g_angle_window_index;
static uint32_t g_distance_window[DISTANCE_MEDIAN_COUNT];
static uint8_t g_distance_window_count;
static uint8_t g_distance_window_index;
static uint32_t g_decision_distance_cm;
static uint32_t g_display_distance_cm;
static uint8_t g_distance_initialized;

static uint16_t ReadBE16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t ReadBE32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* 扫描缓冲区内全部数据，返回最后一个校验正确的新定位帧。 */
static uint8_t UWB_ParseLatest(const uint8_t *buf, uint16_t len,
                               UwbLocation *location)
{
    uint16_t i;
    uint8_t found = 0U;

    if ((buf == NULL) || (location == NULL) || (len < UWB_FRAME_LENGTH)) {
        return 0U;
    }

    for (i = 0U; (uint16_t)(i + UWB_FRAME_LENGTH) <= len; i++) {
        uint8_t check = 0U;
        uint16_t j;

        if ((buf[i] != 0xFFU) || (buf[i + 1U] != 0xFFU) ||
            (buf[i + 2U] != 0xFFU) || (buf[i + 3U] != 0xFFU)) {
            continue;
        }
        if ((ReadBE16(&buf[i + 4U]) != UWB_FRAME_LENGTH) ||
            (ReadBE16(&buf[i + 8U]) != UWB_COMMAND_LOCATION) ||
            !UWB_IsSupportedVersion(ReadBE16(&buf[i + 10U]))) {
            continue;
        }

        for (j = 0U; j < UWB_FRAME_LENGTH - 1U; j++) {
            check ^= buf[i + j];
        }
        if (check != buf[i + UWB_FRAME_LENGTH - 1U]) {
            continue;
        }

        location->sequence = ReadBE16(&buf[i + 6U]);
        location->anchor_id = ReadBE32(&buf[i + 12U]);
        location->tag_id = ReadBE32(&buf[i + 16U]);
        location->distance_cm = ReadBE32(&buf[i + 20U]);
        location->azimuth_deg = (int16_t)ReadBE16(&buf[i + 24U]);
        location->elevation_deg = (int16_t)ReadBE16(&buf[i + 26U]);
        found = 1U;
        i = (uint16_t)(i + UWB_FRAME_LENGTH - 1U);
    }
    return found;
}

static uint8_t ReadDipId(void)
{
    return (uint8_t)((DIP_RD(DIP_PIN1) << 0) |
                     (DIP_RD(DIP_PIN2) << 1) |
                     (DIP_RD(DIP_PIN3) << 2) |
                     (DIP_RD(DIP_PIN4) << 3));
}

static uint32_t CorrectedDistanceCm(uint32_t raw_cm)
{
    int32_t corrected = (int32_t)raw_cm - DISTANCE_OFFSET_CM;
    return (corrected > 0) ? (uint32_t)corrected : 0U;
}

static void ResetDistanceFilter(void)
{
    g_distance_window_count = 0U;
    g_distance_window_index = 0U;
    g_distance_initialized = 0U;
}

static uint32_t MedianDistance(uint32_t sample)
{
    uint32_t sorted[DISTANCE_MEDIAN_COUNT];
    uint8_t i;
    uint8_t j;

    g_distance_window[g_distance_window_index] = sample;
    g_distance_window_index =
        (uint8_t)((g_distance_window_index + 1U) % DISTANCE_MEDIAN_COUNT);
    if (g_distance_window_count < DISTANCE_MEDIAN_COUNT) {
        g_distance_window_count++;
    }

    for (i = 0U; i < g_distance_window_count; i++) {
        sorted[i] = g_distance_window[i];
    }
    for (i = 1U; i < g_distance_window_count; i++) {
        uint32_t value = sorted[i];
        j = i;
        while ((j > 0U) && (sorted[j - 1U] > value)) {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = value;
    }
    return sorted[g_distance_window_count / 2U];
}

static uint32_t FilterDistance(uint32_t raw_cm)
{
    uint32_t median = MedianDistance(CorrectedDistanceCm(raw_cm));
    int32_t difference;
    uint32_t magnitude;

    g_decision_distance_cm = median;
    if (!g_distance_initialized) {
        g_display_distance_cm = median;
        g_distance_initialized = 1U;
        return median;
    }

    difference = (int32_t)median - (int32_t)g_display_distance_cm;
    magnitude = (difference < 0) ? (uint32_t)(-difference) :
                                  (uint32_t)difference;

    if (magnitude <= DISTANCE_HOLD_CM) {
        /* 静止时3cm以内不刷新显示，消除个位跳动。 */
        return g_display_distance_cm;
    }
    if (magnitude >= DISTANCE_FAST_CM) {
        /* 明显移动时快速跟随，避免开锁边界产生过大延迟。 */
        g_display_distance_cm =
            (uint32_t)((int32_t)g_display_distance_cm + difference / 2);
    } else {
        /* 小幅变化只更新1/4，降低静止距离抖动。 */
        g_display_distance_cm =
            (uint32_t)((int32_t)g_display_distance_cm + difference / 4);
    }
    return g_display_distance_cm;
}

static int16_t CorrectedAzimuthDeg(int16_t raw_deg)
{
    return (int16_t)(raw_deg - AZIMUTH_OFFSET_DEG);
}

static int16_t RoundAngle(float angle)
{
    return (int16_t)(angle + ((angle >= 0.0f) ? 0.5f : -0.5f));
}

static int16_t ClampAzimuth45(int16_t angle)
{
    if (angle > FRONT_LIMIT_DEG) {
        return FRONT_LIMIT_DEG;
    }
    if (angle < -FRONT_LIMIT_DEG) {
        return -FRONT_LIMIT_DEG;
    }
    return angle;
}

static void ResetAngleWindow(void)
{
    g_angle_window_count = 0U;
    g_angle_window_index = 0U;
}

static int16_t MedianAngle(int16_t sample)
{
    int16_t sorted[AZIMUTH_MEDIAN_COUNT];
    uint8_t i;
    uint8_t j;

    g_angle_window[g_angle_window_index] = sample;
    g_angle_window_index =
        (uint8_t)((g_angle_window_index + 1U) % AZIMUTH_MEDIAN_COUNT);
    if (g_angle_window_count < AZIMUTH_MEDIAN_COUNT) {
        g_angle_window_count++;
    }

    for (i = 0U; i < g_angle_window_count; i++) {
        sorted[i] = g_angle_window[i];
    }
    for (i = 1U; i < g_angle_window_count; i++) {
        int16_t value = sorted[i];
        j = i;
        while ((j > 0U) && (sorted[j - 1U] > value)) {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = value;
    }
    return sorted[g_angle_window_count / 2U];
}

static int16_t FilterAzimuth(int16_t raw_deg, uint32_t tag_id, uint32_t now)
{
    int16_t limited = ClampAzimuth45(CorrectedAzimuthDeg(raw_deg));
    int16_t median;
    float filtered;

    if ((!g_azimuth_kalman.initialized) || (tag_id != g_filter_tag_id)) {
        ResetAngleWindow();
        Kalman_Reset(&g_azimuth_kalman);
        g_filter_tag_id = tag_id;
    }

    median = MedianAngle(limited);
    if ((median >= -AZIMUTH_ZERO_LOCK_DEG) &&
        (median <= AZIMUTH_ZERO_LOCK_DEG)) {
        median = 0;
    }

    if (!g_azimuth_kalman.initialized) {
        Kalman_Init(&g_azimuth_kalman, (float)median, AZIMUTH_KALMAN_Q,
                    AZIMUTH_KALMAN_R, now);
        filtered = (float)median;
    } else {
        /*
         * 正对时如果滤波状态仍被上一帧大跳变拖走，直接重新锁零；
         * 其余情况继续使用卡尔曼平滑，保留正常转动响应。
         */
        if ((median == 0) &&
            ((g_azimuth_kalman.x > 15.0f) ||
             (g_azimuth_kalman.x < -15.0f))) {
            Kalman_Init(&g_azimuth_kalman, 0.0f, AZIMUTH_KALMAN_Q,
                        AZIMUTH_KALMAN_R, now);
            filtered = 0.0f;
        } else {
            filtered = Kalman_Update(&g_azimuth_kalman, (float)median, now,
                                     AZIMUTH_MAX_STEP_DEG);
        }
    }
    return ClampAzimuth45(RoundAngle(filtered));
}

static uint8_t AngleInFront(int16_t angle)
{
    return ((angle >= -FRONT_LIMIT_DEG) && (angle <= FRONT_LIMIT_DEG)) ? 1U : 0U;
}

static LockState DistanceState(uint32_t distance_cm)
{
    switch (g_state) {
    case LOCK_OPEN:
        if (distance_cm < OPEN_EXIT_CM) {
            return LOCK_OPEN;
        }
        return (distance_cm < WELCOME_EXIT_CM) ? LOCK_WELCOME : LOCK_DETECTION;

    case LOCK_WELCOME:
        if (distance_cm <= OPEN_ENTER_CM) {
            return LOCK_OPEN;
        }
        if (distance_cm >= WELCOME_EXIT_CM) {
            return LOCK_DETECTION;
        }
        return LOCK_WELCOME;

    case LOCK_DETECTION:
        if (distance_cm <= OPEN_ENTER_CM) {
            return LOCK_OPEN;
        }
        return (distance_cm <= WELCOME_ENTER_CM) ? LOCK_WELCOME : LOCK_DETECTION;

    default:
        if (distance_cm <= OPEN_ENTER_CM) {
            return LOCK_OPEN;
        }
        return (distance_cm <= WELCOME_ENTER_CM) ? LOCK_WELCOME : LOCK_DETECTION;
    }
}

static void StartBeep(uint32_t now, uint32_t duration_ms)
{
    g_beep_until = now + duration_ms;
    HAL_GPIO_WritePin(GPIOC, BEEPER_PIN, GPIO_PIN_SET);
}

static void SetEventForTransition(LockState old_state, LockState new_state)
{
    if (new_state == LOCK_OPEN) {
        snprintf(g_event, sizeof(g_event), "ENTER OPEN      ");
    } else if ((old_state == LOCK_OPEN) && (new_state != LOCK_OPEN)) {
        snprintf(g_event, sizeof(g_event), "LEAVE OPEN      ");
    } else if (new_state == LOCK_WELCOME) {
        snprintf(g_event, sizeof(g_event), "ENTER WELCOME   ");
    } else if ((old_state == LOCK_WELCOME) &&
               (new_state == LOCK_DETECTION)) {
        snprintf(g_event, sizeof(g_event), "LEAVE WELCOME   ");
    } else if (new_state == LOCK_ID_DENIED) {
        snprintf(g_event, sizeof(g_event), "ID DENIED       ");
    } else if (new_state == LOCK_OUTSIDE) {
        snprintf(g_event, sizeof(g_event), "OUTSIDE +/-45   ");
    } else if (new_state == LOCK_NO_TAG) {
        snprintf(g_event, sizeof(g_event), "TAG LOST        ");
    } else {
        snprintf(g_event, sizeof(g_event), "TAG DETECTED    ");
    }
}

static void ApplyOutputs(void)
{
    uint8_t open = (g_state == LOCK_OPEN) ? 1U : 0U;
    uint8_t welcome = ((g_state == LOCK_WELCOME) ||
                       (g_state == LOCK_OPEN)) ? 1U : 0U;

    HAL_GPIO_WritePin(GPIOC, LED_OPEN_PIN,
                      open ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, LED_CLOSED_PIN,
                      open ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, LED_WELCOME_PIN,
                      welcome ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void CommitState(LockState new_state, uint32_t now)
{
    LockState old_state = g_state;
    if (new_state == old_state) {
        return;
    }

    g_state = new_state;
    SetEventForTransition(old_state, new_state);
    ApplyOutputs();

    if ((new_state == LOCK_OPEN) || (old_state == LOCK_OPEN)) {
        StartBeep(now, 600U);
    } else if ((new_state == LOCK_WELCOME) ||
               (old_state == LOCK_WELCOME)) {
        StartBeep(now, 180U);
    }
}

static void RequestStableState(LockState desired, uint32_t now)
{
    if (desired == g_state) {
        g_candidate = desired;
        g_candidate_count = 0U;
        return;
    }
    if (desired != g_candidate) {
        g_candidate = desired;
        g_candidate_count = 1U;
    } else if (g_candidate_count < TRANSITION_FRAMES) {
        g_candidate_count++;
    }
    if (g_candidate_count >= TRANSITION_FRAMES) {
        g_candidate_count = 0U;
        CommitState(desired, now);
    }
}

static void ProcessFreshLocation(uint32_t now)
{
    uint32_t distance_cm;
    uint8_t tag_changed = (g_location.tag_id != g_filter_tag_id) ? 1U : 0U;
    int16_t angle = FilterAzimuth(g_location.azimuth_deg,
                                  g_location.tag_id, now);

    if (tag_changed) {
        ResetDistanceFilter();
    }
    (void)FilterDistance(g_location.distance_cm);
    distance_cm = g_decision_distance_cm;
    g_filtered_azimuth = angle;

    if ((uint8_t)(g_location.tag_id & 0x0FU) != g_dip_id) {
        /* 安全失败立即闭锁，不等待三帧。 */
        g_candidate_count = 0U;
        CommitState(LOCK_ID_DENIED, now);
    } else if (!AngleInFront(angle)) {
        g_candidate_count = 0U;
        CommitState(LOCK_OUTSIDE, now);
    } else {
        RequestStableState(DistanceState(distance_cm), now);
    }
}

static const char *StateName(LockState state)
{
    switch (state) {
    case LOCK_ID_DENIED: return "DENIED";
    case LOCK_OUTSIDE: return "OUTSIDE";
    case LOCK_DETECTION: return "DETECT";
    case LOCK_WELCOME: return "WELCOME";
    case LOCK_OPEN: return "OPEN";
    default: return "NO TAG";
    }
}

static void UpdateDisplay(void)
{
    char line[17];
    uint32_t distance_cm = g_distance_initialized ?
                           g_display_distance_cm :
                           CorrectedDistanceCm(g_location.distance_cm);
    int16_t angle = ClampAzimuth45(g_filtered_azimuth);

    if (g_have_frame) {
        snprintf(line, sizeof(line), "ID:%d%d%d%d T:%X %c",
                 (g_dip_id >> 3) & 1U, (g_dip_id >> 2) & 1U,
                 (g_dip_id >> 1) & 1U, g_dip_id & 1U,
                 (unsigned int)(g_location.tag_id & 0x0FU),
                 ((g_location.tag_id & 0x0FU) == g_dip_id) ? 'V' : 'X');
    } else {
        snprintf(line, sizeof(line), "ID:%d%d%d%d T:- --",
                 (g_dip_id >> 3) & 1U, (g_dip_id >> 2) & 1U,
                 (g_dip_id >> 1) & 1U, g_dip_id & 1U);
    }
    OLED_ShowString(1, 1, line);

    /*
     * 角度固定显示为+01/-05/+10；整行固定16列，覆盖上一帧残留字符。
     */
    snprintf(line, sizeof(line), "D:%3lu.%02lum A:%+03d ",
             (unsigned long)(distance_cm / 100U),
             (unsigned long)(distance_cm % 100U), angle);
    OLED_ShowString(2, 1, line);

    snprintf(line, sizeof(line), "%-8s %s",
             StateName(g_state), (g_state == LOCK_OPEN) ? "UNLOCK" : "LOCK");
    OLED_ShowString(3, 1, line);
    OLED_ShowString(4, 1, g_event);
}

void SystemClock_Config(void);
void Error_Handler(void);

int main(void)
{
    uint32_t display_at = 0U;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    OLED_Init();
    ApplyOutputs();

    OLED_ShowString(1, 1, "DIGITAL DOORLOCK");
    OLED_ShowString(2, 1, "STM32F103RCT6   ");
    OLED_ShowString(3, 1, "UWB SINGLE BASE ");
    OLED_ShowString(4, 1, "WAITING TAG     ");
    HAL_Delay(800U);

    while (1) {
        uint32_t now = HAL_GetTick();
        uint8_t new_frame = 0U;

        g_dip_id = ReadDipId();

        if (u2_is_ready()) {
            uint16_t length = u2_get_length();
            new_frame = UWB_ParseLatest(u2_get_data(), length, &g_location);
            u2_clear();
            if (new_frame) {
                g_have_frame = 1U;
                g_last_frame_ms = now;
                ProcessFreshLocation(now);
            }
        }

        if (g_have_frame &&
            ((uint32_t)(now - g_last_frame_ms) > TAG_TIMEOUT_MS)) {
            g_have_frame = 0U;
            g_candidate_count = 0U;
            Kalman_Reset(&g_azimuth_kalman);
            ResetAngleWindow();
            ResetDistanceFilter();
            CommitState(LOCK_NO_TAG, now);
        }

        if ((g_beep_until != 0U) &&
            ((int32_t)(now - g_beep_until) >= 0)) {
            g_beep_until = 0U;
            HAL_GPIO_WritePin(GPIOC, BEEPER_PIN, GPIO_PIN_RESET);
        }

        if ((uint32_t)(now - display_at) >= DISPLAY_PERIOD_MS) {
            display_at = now;
            UpdateDisplay();
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
