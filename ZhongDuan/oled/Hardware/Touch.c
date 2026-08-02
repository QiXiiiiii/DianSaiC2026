#include "Touch.h"

#define TOUCH_CHANNEL_X     0x90U
#define TOUCH_CHANNEL_Y     0xD0U
#define TOUCH_SAMPLE_COUNT  10U
#define TOUCH_WIDTH         320
#define TOUCH_HEIGHT        240

/*
 * Six-point keypad calibration measured through DAPLink on this panel.
 * The fitted points are the visible centres of 1, 3, 9, 7, 5 and 0.
 * This is intentionally matched to the password UI instead of the bezel.
 */
#define TOUCH_X_X           ( 0.0884721f)
#define TOUCH_X_Y           ( 0.0050083f)
#define TOUCH_X_OFFSET      (-19.1518468f)
#define TOUCH_Y_X           ( 0.0007197f)
#define TOUCH_Y_Y           ( 0.0659352f)
#define TOUCH_Y_OFFSET      (-5.9474909f)

#define TOUCH_CS_HIGH()     (TOUCH_PORT->BSRR = TOUCH_CS_PIN)
#define TOUCH_CS_LOW()      (TOUCH_PORT->BRR = TOUCH_CS_PIN)
#define TOUCH_CLK_HIGH()    (TOUCH_PORT->BSRR = TOUCH_CLK_PIN)
#define TOUCH_CLK_LOW()     (TOUCH_PORT->BRR = TOUCH_CLK_PIN)
#define TOUCH_MOSI_HIGH()   (TOUCH_PORT->BSRR = TOUCH_MOSI_PIN)
#define TOUCH_MOSI_LOW()    (TOUCH_PORT->BRR = TOUCH_MOSI_PIN)

/* Live calibration values read through DAPLink; retained by volatile. */
volatile uint16_t g_touch_debug_raw_x;
volatile uint16_t g_touch_debug_raw_y;
volatile uint16_t g_touch_debug_screen_x;
volatile uint16_t g_touch_debug_screen_y;
volatile uint32_t g_touch_debug_sample_count;
volatile uint8_t g_touch_debug_pressed;

static void Touch_Delay(void)
{
    /* About 5 us at 72 MHz with the ARMCC loop overhead. */
    volatile uint16_t count = 120U;
    while (count-- != 0U) {
        __NOP();
    }
}

static uint16_t Touch_ReadChannel(uint8_t command)
{
    uint8_t bit;
    uint16_t value = 0U;

    TOUCH_CS_LOW();
    TOUCH_CLK_LOW();
    for (bit = 0U; bit < 8U; bit++) {
        if ((command & (uint8_t)(0x80U >> bit)) != 0U) {
            TOUCH_MOSI_HIGH();
        } else {
            TOUCH_MOSI_LOW();
        }
        Touch_Delay();
        TOUCH_CLK_HIGH();
        Touch_Delay();
        TOUCH_CLK_LOW();
    }

    TOUCH_MOSI_LOW();
    TOUCH_CLK_HIGH();
    /* The initial high edge advances past BUSY; sample D11..D0 on 12 falls. */
    Touch_Delay();
    for (bit = 0U; bit < 12U; bit++) {
        TOUCH_CLK_LOW();
        Touch_Delay();
        if ((TOUCH_PORT->IDR & TOUCH_MISO_PIN) != 0U) {
            value |= (uint16_t)(1U << (11U - bit));
        }
        TOUCH_CLK_HIGH();
        Touch_Delay();
    }
    TOUCH_CS_HIGH();
    return value;
}

static int16_t Touch_TrimmedMean(int16_t *samples)
{
    uint8_t i;
    int16_t minimum = samples[0];
    int16_t maximum = samples[0];
    int32_t sum = 0;

    for (i = 0U; i < TOUCH_SAMPLE_COUNT; i++) {
        int16_t value = samples[i];
        sum += value;
        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
    }
    return (int16_t)((sum - minimum - maximum) / (TOUCH_SAMPLE_COUNT - 2U));
}

void Touch_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(TOUCH_PORT,
                      TOUCH_CLK_PIN | TOUCH_CS_PIN | TOUCH_MOSI_PIN,
                      GPIO_PIN_SET);

    gpio.Pin = TOUCH_CLK_PIN | TOUCH_CS_PIN | TOUCH_MOSI_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(TOUCH_PORT, &gpio);

    gpio.Pin = TOUCH_MISO_PIN | TOUCH_IRQ_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_PORT, &gpio);

    TOUCH_CLK_LOW();
    TOUCH_CS_HIGH();
    TOUCH_MOSI_LOW();
}

uint8_t Touch_IsPressed(void)
{
    g_touch_debug_pressed =
        ((TOUCH_PORT->IDR & TOUCH_IRQ_PIN) == 0U) ? 1U : 0U;
    return g_touch_debug_pressed;
}

uint8_t Touch_Read(uint16_t *x, uint16_t *y)
{
    int16_t x_samples[TOUCH_SAMPLE_COUNT];
    int16_t y_samples[TOUCH_SAMPLE_COUNT];
    int16_t raw_x;
    int16_t raw_y;
    int32_t screen_x;
    int32_t screen_y;
    uint8_t i;

    if ((x == NULL) || (y == NULL) || !Touch_IsPressed()) {
        return 0U;
    }

    for (i = 0U; i < TOUCH_SAMPLE_COUNT; i++) {
        x_samples[i] = (int16_t)Touch_ReadChannel(TOUCH_CHANNEL_X);
        y_samples[i] = (int16_t)Touch_ReadChannel(TOUCH_CHANNEL_Y);
    }

    raw_x = Touch_TrimmedMean(x_samples);
    raw_y = Touch_TrimmedMean(y_samples);
    g_touch_debug_raw_x = (uint16_t)raw_x;
    g_touch_debug_raw_y = (uint16_t)raw_y;
    screen_x = (int32_t)(TOUCH_X_X * raw_x + TOUCH_X_Y * raw_y +
                         TOUCH_X_OFFSET + 0.5f);
    screen_y = (int32_t)(TOUCH_Y_X * raw_x + TOUCH_Y_Y * raw_y +
                         TOUCH_Y_OFFSET + 0.5f);

    if (screen_x < 0) {
        screen_x = 0;
    } else if (screen_x >= TOUCH_WIDTH) {
        screen_x = TOUCH_WIDTH - 1;
    }
    if (screen_y < 0) {
        screen_y = 0;
    } else if (screen_y >= TOUCH_HEIGHT) {
        screen_y = TOUCH_HEIGHT - 1;
    }

    *x = (uint16_t)screen_x;
    *y = (uint16_t)screen_y;
    g_touch_debug_screen_x = *x;
    g_touch_debug_screen_y = *y;
    g_touch_debug_sample_count++;
    return 1U;
}

void Touch_GetLastRaw(uint16_t *raw_x, uint16_t *raw_y)
{
    if (raw_x != NULL) {
        *raw_x = g_touch_debug_raw_x;
    }
    if (raw_y != NULL) {
        *raw_y = g_touch_debug_raw_y;
    }
}
