/**
 * @file    main.c
 * @brief   数字钥匙显示端：通过UART读取UWB定位帧并显示标签信息。
 *
 * 标签TX接PA3(USART2_RX)，标签RX接PA2(USART2_TX，可选)，必须共地。
 */

#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "OLED.h"
#include <stdio.h>

#define BEEP_ON()       HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET)
#define BEEP_OFF()      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET)
#define UWB_FRAME_LEN   37U
#define ANGLE_LIMIT_DEG 45
#define ANGLE_WINDOW    5U
#define ANGLE_ZERO_DEG  3

static uint32_t g_tag_id;
static uint32_t g_distance_cm = 500U;
static int16_t g_azimuth;
static uint8_t g_beep_ticks;
static int16_t g_angle_samples[ANGLE_WINDOW];
static uint8_t g_angle_count;
static uint8_t g_angle_index;

static uint16_t ReadBE16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t ReadBE32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint8_t ValidVersion(uint16_t version)
{
    return (version == 0x0100U) || (version == 0x0101U) ||
           (version == 0x0102U);
}

static int16_t ClampAngle45(int16_t angle)
{
    if (angle > ANGLE_LIMIT_DEG) {
        return ANGLE_LIMIT_DEG;
    }
    if (angle < -ANGLE_LIMIT_DEG) {
        return -ANGLE_LIMIT_DEG;
    }
    return angle;
}

static int16_t FilterKeyAngle(int16_t raw_angle)
{
    int16_t sorted[ANGLE_WINDOW];
    int16_t median;
    uint8_t i;
    uint8_t j;

    raw_angle = ClampAngle45(raw_angle);
    g_angle_samples[g_angle_index] = raw_angle;
    g_angle_index = (uint8_t)((g_angle_index + 1U) % ANGLE_WINDOW);
    if (g_angle_count < ANGLE_WINDOW) {
        g_angle_count++;
    }

    for (i = 0U; i < g_angle_count; i++) {
        sorted[i] = g_angle_samples[i];
    }
    for (i = 1U; i < g_angle_count; i++) {
        int16_t value = sorted[i];
        j = i;
        while ((j > 0U) && (sorted[j - 1U] > value)) {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = value;
    }

    median = sorted[g_angle_count / 2U];
    if ((median >= -ANGLE_ZERO_DEG) && (median <= ANGLE_ZERO_DEG)) {
        median = 0;
    }

    /*
     * 中值滤波先去除80/90度尖峰，再用整数低通减小剩余摆动。
     * 所有入口和出口都限幅，保证最终值绝不超过±45度。
     */
    if (g_angle_count == 1U) {
        g_azimuth = median;
    } else {
        g_azimuth = (int16_t)(((int32_t)g_azimuth * 3 + median) / 4);
    }
    if ((g_azimuth >= -ANGLE_ZERO_DEG) &&
        (g_azimuth <= ANGLE_ZERO_DEG)) {
        g_azimuth = 0;
    }
    return ClampAngle45(g_azimuth);
}

/* 解析本次接收缓冲中的最后一个完整、校验正确的定位帧。 */
static uint8_t UWB_ParseLatest(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t found = 0U;

    if ((buf == NULL) || (len < UWB_FRAME_LEN)) {
        return 0U;
    }

    for (i = 0U; (uint16_t)(i + UWB_FRAME_LEN) <= len; i++) {
        uint16_t j;
        uint8_t checksum = 0U;

        if ((buf[i] != 0xFFU) || (buf[i + 1U] != 0xFFU) ||
            (buf[i + 2U] != 0xFFU) || (buf[i + 3U] != 0xFFU) ||
            (ReadBE16(&buf[i + 4U]) != UWB_FRAME_LEN) ||
            (ReadBE16(&buf[i + 8U]) != 0x2001U) ||
            !ValidVersion(ReadBE16(&buf[i + 10U]))) {
            continue;
        }

        for (j = 0U; j < UWB_FRAME_LEN - 1U; j++) {
            checksum ^= buf[i + j];
        }
        if (checksum != buf[i + UWB_FRAME_LEN - 1U]) {
            continue;
        }

        g_tag_id = ReadBE32(&buf[i + 16U]);
        g_distance_cm = ReadBE32(&buf[i + 20U]);
        g_azimuth = FilterKeyAngle((int16_t)ReadBE16(&buf[i + 24U]));
        found = 1U;
        i = (uint16_t)(i + UWB_FRAME_LEN - 1U);
    }
    return found;
}

static void UpdateDisplay(void)
{
    char line[17];
    const char *zone;

    if (g_distance_cm <= 100U) {
        zone = "OPEN";
    } else if (g_distance_cm <= 200U) {
        zone = "WELCOME";
    } else {
        zone = "DETECT";
    }

    snprintf(line, sizeof(line), "Tag:%04lX", (unsigned long)(g_tag_id & 0xFFFFU));
    OLED_ShowString(1, 1, line);
    snprintf(line, sizeof(line), "D:%lu.%02lum", (unsigned long)(g_distance_cm / 100U),
             (unsigned long)(g_distance_cm % 100U));
    OLED_ShowString(2, 1, line);
    g_azimuth = ClampAngle45(g_azimuth);
    /* 固定宽度，避免上一帧+10的末尾0残留，使+1看起来像+10。 */
    snprintf(line, sizeof(line), "Azimuth:%+03d     ", g_azimuth);
    OLED_ShowString(3, 1, line);
    snprintf(line, sizeof(line), "Zone:%-9s", zone);
    OLED_ShowString(4, 1, line);
}

void SystemClock_Config(void);
void Error_Handler(void);

int main(void)
{
    uint32_t display_at = 0U;
    uint8_t old_zone = 0xFFU;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    OLED_Init();

    OLED_ShowString(1, 1, "UWB DIGITAL KEY ");
    OLED_ShowString(2, 1, "UART RX MODE    ");
    OLED_ShowString(3, 1, "Waiting frame... ");

    while (1) {
        uint32_t now = HAL_GetTick();
        uint8_t updated = 0U;

        if (u2_is_ready()) {
            updated = UWB_ParseLatest(u2_get_data(), u2_get_length());
            u2_clear();
        }
        /* 保留USART1兼容原先接线方式。 */
        if (u1_is_ready()) {
            updated |= UWB_ParseLatest(u1_get_data(), u1_get_length());
            u1_clear();
        }

        if (updated) {
            uint8_t zone = (g_distance_cm <= 100U) ? 2U :
                           ((g_distance_cm <= 200U) ? 1U : 0U);
            if (zone != old_zone) {
                BEEP_ON();
                g_beep_ticks = 3U;
                old_zone = zone;
            }
        }

        if (g_beep_ticks && ((uint32_t)(now - display_at) >= 100U)) {
            g_beep_ticks--;
            if (!g_beep_ticks) {
                BEEP_OFF();
            }
        }

        if ((uint32_t)(now - display_at) >= 100U) {
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
