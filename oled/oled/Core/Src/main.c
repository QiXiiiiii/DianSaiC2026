/**
 * @file    main.c
 * @brief   C题 数字钥匙实验系统 — 门锁端 (STM32F103RC)
 *
 * OLED 4行 (PB8=SCL PB9=SDA):
 *   Line1: ID:xxxx K:xxxx V/N  (拨码ID + 钥匙ID + 验证)
 *   Line2: Welc:YES Det:YES    (迎宾区 + 感应区)
 *   Line3: Door:YES Ang:45     (门锁 + 角度)
 *   Line4: D:1.2m  R:-68      (距离 + RSSI)
 *
 * GC-P2304-GS-2 帧格式 (10字节):
 *   F0 06 ID_L ID_H DIST_L DIST_H ANGL_L ANGL_H RSSI AA
 */

#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "OLED.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================== DIP + LED + BEEPER ==================== */

#define DIP_PORT    GPIOB
#define DIP_PIN1    GPIO_PIN_12
#define DIP_PIN2    GPIO_PIN_13
#define DIP_PIN3    GPIO_PIN_14
#define DIP_PIN4    GPIO_PIN_15
#define DIP_RD(p)   (!HAL_GPIO_ReadPin(DIP_PORT, (p)))

#define BEEPER_PIN  GPIO_PIN_1   /* PC1 蜂鸣器 */
#define LED_OPEN    GPIO_PIN_2   /* PC2 开门绿 */
#define LED_CLOSE   GPIO_PIN_3   /* PC3 关门红 */

#define BEEP_ON()   HAL_GPIO_WritePin(GPIOC, BEEPER_PIN, GPIO_PIN_SET)
#define BEEP_OFF()  HAL_GPIO_WritePin(GPIOC, BEEPER_PIN, GPIO_PIN_RESET)
#define LED_ON(p)   HAL_GPIO_WritePin(GPIOC, (p), GPIO_PIN_RESET)
#define LED_OFF(p)  HAL_GPIO_WritePin(GPIOC, (p), GPIO_PIN_SET)

/* ==================== UWB 二进制帧解析 ==================== */

/* 扫描 buffer 找完整帧 F0 06 ... AA，返回帧长度(10) 或 0 */
static int UWB_ParseFrame(const uint8_t *buf, int len,
    uint16_t *mod_id, int16_t *dist_cm, int16_t *angle, int8_t *rssi)
{
    for (int i = 0; i <= len - 10; i++) {
        if (buf[i] == 0xF0 && buf[i+1] == 0x06 && buf[i+9] == 0xAA) {
            *mod_id  = buf[i+2] | (buf[i+3] << 8);         /* 小端 ID */
            *dist_cm = (int16_t)(buf[i+4] | (buf[i+5] << 8)); /* 小端 距离 cm */
            *angle   = (int16_t)(buf[i+6] | (buf[i+7] << 8)); /* 小端 角度 ° */
            *rssi    = (int8_t)(buf[i+8] - 256);            /* RSSI dBm */
            return 10;   /* 返回帧长度 */
        }
    }
    return 0;
}

/* ==================== 全局状态 ==================== */
static uint16_t g_dip_id     = 0;
static uint16_t g_key_id     = 0;
static uint8_t  g_key_valid  = 0;
static float    g_distance   = 5.0f;
static int16_t  g_angle      = 0;
static int8_t   g_rssi       = -128;
static uint8_t  g_key_active = 0;   /* 是否收到过钥匙信号 */
static uint32_t g_key_last   = 0;   /* 上次收到钥匙的时间 */

/* 蜂鸣器 */
static uint8_t beep_timer = 0;
static void Beeper_Beep(void) { BEEP_ON(); beep_timer = 3; }

void SystemClock_Config(void);
void Error_Handler(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    OLED_Init();

    /* 开机画面 */
    OLED_ShowString(1, 1, "Digital Key Sys");
    OLED_ShowString(2, 3, "Vehicle Side");
    OLED_ShowString(3, 4, "C-TI Cup 2026");
    HAL_Delay(2000);
    OLED_Clear();

    uint32_t last = 0;

    while (1) {
        uint32_t now = HAL_GetTick();
        if (now - last < 100) continue;
        last = now;

        /* ---- 拨码 ---- */
        g_dip_id = (DIP_RD(DIP_PIN1) ? 1 : 0) |
                   (DIP_RD(DIP_PIN2) ? 2 : 0) |
                   (DIP_RD(DIP_PIN3) ? 4 : 0) |
                   (DIP_RD(DIP_PIN4) ? 8 : 0);

        /* ---- Zigbee (UART1) 收钥匙ID ---- */
        if (u1_is_ready()) {
            char *data = (char*)u1_get_data();
            char *p = strstr(data, "ID:");
            if (!p) p = strstr(data, "[ID,");
            if (p) {
                g_key_id = atoi(p + (p[0] == 'I' ? 3 : 4));
                g_key_active = 1;
                g_key_last = now;
            }
            u1_clear();
        }
        /* 密钥超时 5秒无信号 → 视为离开 */
        if (g_key_active && (now - g_key_last > 5000)) {
            g_key_active = 0;
            g_key_valid = 0;
        }

        /* ID 验证 */
        g_key_valid = g_key_active && (g_key_id == g_dip_id);

        /* ---- UWB (UART2) 二进制帧解析 ---- */
        if (u2_is_ready()) {
            uint8_t *raw = u2_get_data();
            uint16_t len = 0;
            /* 获取原始长度：扫描直到 buffer 结束 */
            for (len = 0; len < 120 && raw[len] != 0; len++);
            /* 尝试解析二进制帧 */
            uint16_t mod_id;
            int16_t  dist_cm, angle_raw;
            int8_t   rssi_raw;
            int flen = UWB_ParseFrame(raw, len, &mod_id, &dist_cm, &angle_raw, &rssi_raw);
            if (flen > 0) {
                g_distance = dist_cm / 100.0f;  /* cm → m */
                g_angle    = angle_raw;
                g_rssi     = rssi_raw;
            }
            u2_clear();
            /* 收到 UWB 说明钥匙在范围内 */
            if (!g_key_active) { g_key_active = 1; g_key_last = now; }
        }

        /* ---- 区域判断 ---- */
        uint8_t in_open = (g_distance <= 1.0f) && g_key_valid;
        uint8_t in_welc = (g_distance > 1.0f && g_distance <= 2.0f) && g_key_active;
        uint8_t in_det  = (g_distance > 2.0f) && g_key_active;

        /* 边沿检测 */
        static uint8_t open_prev, welc_prev, det_prev;
        if (in_open && !open_prev) Beeper_Beep();
        if (in_welc && !welc_prev) Beeper_Beep();
        if (in_det  && !det_prev)  Beeper_Beep();
        if ((open_prev && !in_open) || (welc_prev && !in_welc) || (det_prev && !in_det))
            Beeper_Beep();
        open_prev = in_open;
        welc_prev = in_welc;
        det_prev  = in_det;

        /* ---- LED ---- */
        if (in_open)       { LED_ON(LED_OPEN);  LED_OFF(LED_CLOSE); }
        else if (in_welc)  { LED_OFF(LED_OPEN); LED_OFF(LED_CLOSE); }
        else               { LED_OFF(LED_OPEN); LED_ON(LED_CLOSE);  }

        /* ---- 蜂鸣器 ---- */
        if (beep_timer) {
            if (--beep_timer == 0) BEEP_OFF();
        }

        /* ---- OLED ---- */
        char buf[17];

        /* Line1: 拨码ID(4位二进制) + 钥匙ID(4位) + 验证 */
        snprintf(buf, 17, "ID:%d%d%d%d K:%04d%c",
            DIP_RD(DIP_PIN1), DIP_RD(DIP_PIN2),
            DIP_RD(DIP_PIN3), DIP_RD(DIP_PIN4),
            g_key_id, g_key_valid ? 'V' : 'N');
        OLED_ShowString(1, 1, buf);

        /* Line2: 迎宾区 + 感应区 */
        snprintf(buf, 17, "Wel:%s Det:%s",
            in_welc ? "YES" : "NO ",
            in_det  ? "YES" : "NO ");
        OLED_ShowString(2, 1, buf);

        /* Line3: 门锁 + 角度 */
        snprintf(buf, 17, "Door:%-4s Ang:%+d",
            in_open ? "YES" : "NO", g_angle);
        OLED_ShowString(3, 1, buf);

        /* Line4: 距离(m) + RSSI */
        snprintf(buf, 17, "D:%.1fm  R:%d  ", g_distance, g_rssi);
        OLED_ShowString(4, 1, buf);
    }
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&osc);
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

void Error_Handler(void) { __disable_irq(); while (1); }
